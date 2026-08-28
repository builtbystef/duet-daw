#include <duet/collab/ContentEstimates.h>

#include "AnalysisCall.h"

#include <duet/collab/Analysis.h>

#include <duet/model/AudioFile.h>

#include <string>
#include <utility>
#include <vector>

namespace duet::collab
{
namespace
{
    using analysis::Waveform;
    using analysisCall::Plan;

    /** The name this tool answers to, which is also what its ledger lines say
        handed the estimate over.
    */
    constexpr const char* toolName = "estimate_audio_content";

    /** What this build can estimate. The other two aspects the contract names —
        the notes of a polyphonic part, and which instrument it sounds like —
        need the transcription model, and arrive with it (issue bmrxnw).
    */
    constexpr const char* keyAspect = "key";
    constexpr const char* chordsAspect = "chords";

    /** How much of a bar is left out at each end before its chord is read.

        A chord is named over the middle of its bar, because the ends of a bar
        are where the chord before it is still ringing and the chord after it
        has already been struck — the engine starts a sound at the beginning of
        the block that holds it, so the next bar's arrival is early by as much
        as a block.
    */
    constexpr double barEdgeTrim = 0.05;

    /** What a call asked to be estimated: what it named, or everything when it
        named nothing.
    */
    struct Aspects
    {
        bool key = true;
        bool chords = true;

        [[nodiscard]] bool nothing() const { return ! key && ! chords; }
    };

    /** Nothing when `aspects` is not a list of names, which is an error the
        model can correct against rather than a list guessed out of it.
    */
    std::optional<Aspects> aspectsOf (const ToolCall& call)
    {
        const auto found = call.arguments.find ("aspects");

        if (found == call.arguments.end() || found->is_null())
            return Aspects {};

        if (! found->is_array())
            return {};

        Aspects asked { false, false };

        for (const auto& aspect : *found)
        {
            if (! aspect.is_string())
                return {};

            // A name this build does not estimate is not an error: the contract
            // holds four aspects and this build answers two of them, so a call
            // that asks for one of the others is answered with what it can have.
            asked.key = asked.key || aspect == keyAspect;
            asked.chords = asked.chords || aspect == chordsAspect;
        }

        return asked;
    }

    /** The stretch of the render one bar of the plan covers, with its edges
        left out.
    */
    Waveform barOf (const Waveform& whole, const Plan& plan, std::size_t bar)
    {
        const auto from = plan.barStarts.at (bar);
        const auto to = plan.barStarts.at (bar + 1);
        const auto trim = (to - from) * barEdgeTrim;

        return whole.between (from + trim, to - trim);
    }
} // namespace

//==============================================================================
ContentEstimates::ContentEstimates (model::Session& projectSession,
                                    ProjectReadMarshal readMarshal,
                                    TrackRenders& trackRenders,
                                    EstimateLedger& estimateLedger)
    : session (projectSession), marshal (std::move (readMarshal)), renders (trackRenders),
      ledger (estimateLedger)
{
}

void ContentEstimates::addTo (ToolRegistry& registry)
{
    registry.add (toolName, [this] (const ToolCall& call) { return estimate (call); });
}

RpcOutcome ContentEstimates::estimate (const ToolCall& call)
{
    const auto id = analysisCall::argument (call, "trackId");

    if (id.empty())
        return RpcOutcome::failure (rpcError::invalidParams, "this tool needs a trackId");

    const auto bars = analysisCall::barRangeOf (call);

    if (! bars.has_value())
        return RpcOutcome::failure (
            rpcError::invalidParams,
            "barRange is the first and last bar to look at, counting from 1");

    const auto asked = aspectsOf (call);

    if (! asked.has_value())
        return RpcOutcome::failure (rpcError::invalidParams,
                                    "aspects is a list of what to estimate");

    if (asked->nothing())
        return RpcOutcome::failure (rpcError::invalidParams,
                                    "the aspects this Duet can estimate are key and chords");

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

    Json result = Json::object();

    if (asked->key)
    {
        // Nothing rather than a name for a stretch that gives nothing to read:
        // an absent key says the track has none to find, where the key that
        // silence fits least badly would say something untrue.
        if (const auto key = analysis::estimatedKey (stretch); ! key.name.empty())
            result["key"] = ledger.record (
                call.runId,
                toolName,
                keyAspect,
                Estimate { key.name, std::string { analysis::keyMethod }, key.confidence });
    }

    if (! renders.wanted (call.runId))
        return analysisCall::abandoned();

    if (asked->chords)
    {
        auto named = Json::array();
        double confidence = 0.0;

        for (std::size_t bar = 0; bar + 1 < plan->barStarts.size(); ++bar)
        {
            if (! renders.wanted (call.runId))
                return analysisCall::abandoned();

            const auto chord = analysis::estimatedChord (barOf (whole, *plan, bar));

            if (chord.name.empty())
                continue;

            named.push_back (Json { { "bar", plan->firstBar + static_cast<int> (bar) },
                                    { "chord", chord.name } });
            confidence += chord.confidence;
        }

        if (! named.empty())
        {
            // One confidence for the list, because the list is one estimate:
            // what it says is how well the bars it named fit the chords it
            // named them, on average.
            confidence /= static_cast<double> (named.size());

            result["chords"] = ledger.record (
                call.runId,
                toolName,
                chordsAspect,
                Estimate { std::move (named), std::string { analysis::chordMethod }, confidence });
        }
    }

    return RpcOutcome::success (std::move (result));
}
} // namespace duet::collab
