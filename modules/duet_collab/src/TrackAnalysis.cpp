#include <duet/collab/TrackAnalysis.h>

#include <duet/collab/Analysis.h>

#include <duet/model/AudioFile.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace duet::collab
{
namespace
{
    using analysis::Waveform;

    RpcOutcome abandoned()
    {
        return RpcOutcome::failure (rpcError::runAbandoned, "the run was canceled");
    }

    /** One of a call's arguments as text, and empty when it is not there or is
        not text. A number where an id belongs is a call this project cannot
        answer, and it goes down the same path as one naming a track that is not
        there.
    */
    std::string argument (const ToolCall& call, std::string_view name)
    {
        const auto found = call.arguments.find (name);

        if (found == call.arguments.end() || ! found->is_string())
            return {};

        return found->get<std::string>();
    }

    /** The bars a call asked about, first and last and both included, or the
        whole track when it asked about no bars in particular.
    */
    struct BarRange
    {
        bool stated = false;
        double firstBar = 1.0;
        double lastBar = 1.0;
    };

    /** Nothing for a barRange that is not one, which is an error the model can
        correct against rather than a range guessed out of it.
    */
    std::optional<BarRange> barRangeOf (const ToolCall& call)
    {
        const auto found = call.arguments.find ("barRange");

        if (found == call.arguments.end() || found->is_null())
            return BarRange {};

        if (! found->is_array() || found->size() != 2 || ! found->at (0).is_number()
            || ! found->at (1).is_number())
            return {};

        const auto first = found->at (0).get<double>();
        const auto last = found->at (1).get<double>();

        if (first < 1.0 || last < first)
            return {};

        return BarRange { true, first, last };
    }

    /** What the project has to say before anything is rendered: which track,
        what state it is in, and which stretch of the timeline was asked about.
    */
    struct Plan
    {
        model::TrackRef track = model::noTrack;
        std::string digest;
        double fromSeconds = 0.0;
        double toSeconds = 0.0;
        bool wholeTrack = true;
    };

    /** That, read on the thread the project is written on. Nothing when the
        project holds no such track.
    */
    std::optional<Plan> readPlan (const model::Session& session,
                                  const ProjectReadMarshal& marshal,
                                  const std::string& id,
                                  const BarRange& bars)
    {
        std::optional<Plan> plan;

        marshal (
            [&]
            {
                const auto ref = toolId::toTrack (id);

                if (! ref.has_value())
                    return;

                const auto digest = session.trackStateDigest (*ref);

                if (digest.empty())
                    return;

                plan = Plan { *ref,
                              digest,
                              bars.stated ? session.secondsAtBar (bars.firstBar) : 0.0,
                              bars.stated ? session.secondsAtBar (bars.lastBar + 1.0) : 0.0,
                              ! bars.stated };
            });

        return plan;
    }

    /** Every measurement of the contract, in the order the contract states them.

        Stable content first and the content an edit moves last is what the
        prompt-cache discipline asks of a result, and here everything moves
        together: what a track puts out is one measurement, so the order is the
        contract's own.
    */
    Json measurementsOf (const Waveform& stretch)
    {
        Json measured = Json::object();

        measured["peakDb"] = analysis::peakDb (stretch);
        measured["truePeakDbtp"] = analysis::truePeakDbtp (stretch);
        measured["rmsDb"] = analysis::rmsDb (stretch);
        measured["lufsIntegrated"] = analysis::lufsIntegrated (stretch);
        measured["lufsShortTermMax"] = analysis::lufsShortTermMax (stretch);
        measured["crestFactorDb"] = analysis::crestFactorDb (stretch);

        const auto energies = analysis::spectralBandEnergiesDb (stretch);
        auto bands = Json::array();

        for (std::size_t band = 0; band < analysis::spectralBands.size(); ++band)
            bands.push_back (Json { { "band", analysis::spectralBands.at (band).name },
                                    { "energyDb", energies[band] } });

        measured["spectralBands"] = std::move (bands);
        measured["spectralCentroidHz"] = analysis::spectralCentroidHz (stretch);
        measured["spectralFlatness"] = analysis::spectralFlatness (stretch);
        measured["stereoCorrelation"] = analysis::stereoCorrelation (stretch);
        measured["stereoWidth"] = analysis::stereoWidth (stretch);

        return measured;
    }

    /** Whether an onset of the render belongs to the stretch that was asked
        about.

        Both edges stand one render block earlier than the bars do, because that
        is how early the engine starts a sound: what the producer wrote on bar 5
        is in the render a block before bar 5, and an onset must not fall out of
        the range that asked for it over that.
    */
    bool insideRange (double atSeconds, const Plan& plan)
    {
        return plan.wholeTrack
               || (atSeconds >= plan.fromSeconds - model::renderBlockSeconds
                   && atSeconds < plan.toSeconds - model::renderBlockSeconds);
    }

    /** The onsets of the range, in beats, which is the one measurement that
        needs the project again: a moment is beats only through the tempo map.
    */
    Json onsetBeats (const model::Session& session,
                     const ProjectReadMarshal& marshal,
                     const Plan& plan,
                     const std::vector<double>& onsets)
    {
        auto beats = Json::array();

        marshal (
            [&]
            {
                for (const auto at : onsets)
                    if (insideRange (at, plan))
                        beats.push_back (session.beatsAtSeconds (at));
            });

        return beats;
    }
} // namespace

//==============================================================================
TrackRenderer offlineTrackRenderer (model::Session& session)
{
    return [&session] (model::TrackRef track,
                       const std::filesystem::path& destination,
                       const std::function<bool()>& keepGoing)
    {
        // The master is the whole project through the master chain, which is
        // exactly what a whole-project render is.
        return track == model::masterChannel
                   ? session.renderToFile (destination, keepGoing)
                   : session.renderTrackToFile (track, destination, keepGoing);
    };
}

//==============================================================================
TrackAnalysis::TrackAnalysis (model::Session& projectSession,
                              ProjectReadMarshal readMarshal,
                              TrackRenderer renderTrack,
                              std::filesystem::path renderFolder,
                              RunStillWanted runStillWanted)
    : session (projectSession), marshal (std::move (readMarshal)), render (std::move (renderTrack)),
      folder (std::move (renderFolder)), stillWanted (std::move (runStillWanted))
{
}

TrackAnalysis::~TrackAnalysis()
{
    for (const auto& [track, rendered] : cache)
    {
        std::error_code ignored;
        std::filesystem::remove (rendered.file, ignored);
    }
}

bool TrackAnalysis::wanted (const std::string& runId) const
{
    return ! stillWanted || stillWanted (runId);
}

void TrackAnalysis::addTo (ToolRegistry& registry)
{
    registry.add ("get_track_analysis", [this] (const ToolCall& call) { return analyse (call); });
}

std::optional<std::filesystem::path> TrackAnalysis::renderFor (model::TrackRef track,
                                                               const std::string& digest,
                                                               const std::string& runId)
{
    const auto held = cache.find (track);

    if (held != cache.end() && held->second.digest == digest)
        return held->second.file;

    auto destination =
        folder / ("analysis-" + std::to_string (track) + "-" + std::to_string (renders) + ".wav");
    ++renders;

    if (! render (track, destination, [this, &runId] { return wanted (runId); }))
    {
        std::error_code ignored;
        std::filesystem::remove (destination, ignored);

        return {};
    }

    if (held != cache.end())
    {
        std::error_code ignored;
        std::filesystem::remove (held->second.file, ignored);
        held->second = { digest, destination };
    }
    else
    {
        cache.emplace (track, Rendered { digest, destination });
    }

    return destination;
}

RpcOutcome TrackAnalysis::analyse (const ToolCall& call)
{
    const auto id = argument (call, "trackId");

    if (id.empty())
        return RpcOutcome::failure (rpcError::invalidParams, "this tool needs a trackId");

    const auto bars = barRangeOf (call);

    if (! bars.has_value())
        return RpcOutcome::failure (
            rpcError::invalidParams,
            "barRange is the first and last bar to look at, counting from 1");

    const auto plan = readPlan (session, marshal, id, *bars);

    if (! plan.has_value())
        return RpcOutcome::failure (rpcError::invalidParams,
                                    "this project has no track called " + id);

    if (! wanted (call.runId))
        return abandoned();

    const auto file = renderFor (plan->track, plan->digest, call.runId);

    if (! file.has_value())
        return wanted (call.runId) ? RpcOutcome::failure (rpcError::internalError,
                                                          "this track could not be rendered: " + id)
                                   : abandoned();

    auto samples = model::readAudioFile (*file);
    const Waveform whole { samples.sampleRate, std::move (samples.channels) };
    const auto stretch =
        plan->wholeTrack ? whole : whole.between (plan->fromSeconds, plan->toSeconds);

    auto measured = measurementsOf (stretch);

    if (! wanted (call.runId))
        return abandoned();

    // Onsets over the whole render and then read for the part that was asked
    // about: a note held across the start of a range began before it, and a
    // stretch cut out of a render cannot tell that on its own.
    measured["onsetsBeats"] = onsetBeats (session, marshal, *plan, analysis::onsetsSeconds (whole));

    return RpcOutcome::success (std::move (measured));
}
} // namespace duet::collab
