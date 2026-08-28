#include <duet/collab/TrackAnalysis.h>

#include "AnalysisCall.h"

#include <duet/collab/Analysis.h>

#include <duet/model/AudioFile.h>

#include <utility>
#include <vector>

namespace duet::collab
{
namespace
{
    using analysis::Waveform;
    using analysisCall::Plan;

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
TrackAnalysis::TrackAnalysis (model::Session& projectSession,
                              ProjectReadMarshal readMarshal,
                              TrackRenders& trackRenders)
    : session (projectSession), marshal (std::move (readMarshal)), renders (trackRenders)
{
}

void TrackAnalysis::addTo (ToolRegistry& registry)
{
    registry.add ("get_track_analysis", [this] (const ToolCall& call) { return analyse (call); });
}

RpcOutcome TrackAnalysis::analyse (const ToolCall& call)
{
    const auto id = analysisCall::argument (call, "trackId");

    if (id.empty())
        return RpcOutcome::failure (rpcError::invalidParams, "this tool needs a trackId");

    const auto bars = analysisCall::barRangeOf (call);

    if (! bars.has_value())
        return RpcOutcome::failure (
            rpcError::invalidParams,
            "barRange is the first and last bar to look at, counting from 1");

    const auto plan = analysisCall::readPlan (session, marshal, id, *bars);

    if (! plan.has_value())
        return RpcOutcome::failure (rpcError::invalidParams,
                                    "this project has no track called " + id);

    if (! renders.wanted (call.runId))
        return analysisCall::abandoned();

    const auto file = renders.fileFor (plan->track, plan->digest, call.runId);

    if (! file.has_value())
        return renders.wanted (call.runId)
                   ? RpcOutcome::failure (rpcError::internalError,
                                          "this track could not be rendered: " + id)
                   : analysisCall::abandoned();

    auto samples = model::readAudioFile (*file);
    const Waveform whole { samples.sampleRate, std::move (samples.channels) };
    const auto stretch =
        plan->wholeTrack ? whole : whole.between (plan->fromSeconds, plan->toSeconds);

    auto measured = measurementsOf (stretch);

    if (! renders.wanted (call.runId))
        return analysisCall::abandoned();

    // Onsets over the whole render and then read for the part that was asked
    // about: a note held across the start of a range began before it, and a
    // stretch cut out of a render cannot tell that on its own.
    measured["onsetsBeats"] = onsetBeats (session, marshal, *plan, analysis::onsetsSeconds (whole));

    return RpcOutcome::success (std::move (measured));
}
} // namespace duet::collab
