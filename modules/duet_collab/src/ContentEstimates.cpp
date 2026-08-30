#include <duet/collab/ContentEstimates.h>

#include "AnalysisCall.h"

#include <duet/collab/Analysis.h>
#include <duet/collab/Transcription.h>

#include <duet/model/AudioFile.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
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

    /** The four aspects the contract names. The last two need the
        transcription model, so what this build can estimate is the first two
        always and all four when the model is there.
    */
    constexpr const char* keyAspect = "key";
    constexpr const char* chordsAspect = "chords";
    constexpr const char* notesAspect = "notes";
    constexpr const char* instrumentAspect = "instrument";

    /** How much of a bar is left out at each end before its chord is read.

        A chord is named over the middle of its bar, because the ends of a bar
        are where the chord before it is still ringing and the chord after it
        has already been struck — the engine starts a sound at the beginning of
        the block that holds it, so the next bar's arrival is early by as much
        as a block.
    */
    constexpr double barEdgeTrim = 0.05;

    /** What a call asked to be estimated: what it named, or everything this
        build can estimate when it named nothing.
    */
    struct Aspects
    {
        bool key = true;
        bool chords = true;
        bool notes = true;
        bool instrument = true;

        [[nodiscard]] bool nothing() const { return ! key && ! chords && ! notes && ! instrument; }

        /** Whether anything asked for has to be transcribed before it can be
            answered. The model is read once for both, since the instrument is
            a reading of the notes.
        */
        [[nodiscard]] bool transcribing() const { return notes || instrument; }
    };

    /** What this Duet can estimate, in the contract's own order, as the error
        that turns a call away says it.

        Built rather than written out, because a build without the transcription
        model can estimate two of the four, and a model told it can ask for an
        aspect that will never be answered is worse off than one that was told
        the truth.
    */
    std::string estimable()
    {
        if (! transcription::available())
            return "key and chords";

        return "key, chords, notes and instrument";
    }

    /** Nothing when `aspects` is not a list of names, which is an error the
        model can correct against rather than a list guessed out of it.
    */
    std::optional<Aspects> aspectsOf (const ToolCall& call)
    {
        const auto transcribes = transcription::available();

        const auto everything = [&] { return Aspects { true, true, transcribes, transcribes }; };

        const auto found = call.arguments.find ("aspects");

        if (found == call.arguments.end() || found->is_null())
            return everything();

        if (! found->is_array())
            return {};

        Aspects asked { false, false, false, false };

        for (const auto& aspect : *found)
        {
            if (! aspect.is_string())
                return {};

            // A name this build does not estimate is not an error in itself:
            // it is one only when it is all the call asked for, which is what
            // `nothing` below is.
            asked.key = asked.key || aspect == keyAspect;
            asked.chords = asked.chords || aspect == chordsAspect;
            asked.notes = asked.notes || (transcribes && aspect == notesAspect);
            asked.instrument = asked.instrument || (transcribes && aspect == instrumentAspect);
        }

        return asked;
    }

    /** How hard a transcribed note was played, as MIDI writes it. The model
        answers with how strongly the pitch was sounding, from 0 to 1, and a
        note that sounded at all was played at all, so nothing lands on zero.
    */
    int velocityOf (double strength)
    {
        return std::clamp (static_cast<int> (std::lround (strength * 127.0)), 1, 127);
    }

    /** The transcribed notes as the seam writes them, with their times in the
        project's beats — the same beats `get_track_analysis` puts its onsets
        in, read on the thread the project is written on.

        `offsetSeconds` is where the stretch that was transcribed begins in the
        track, since a note's time is its own stretch's and a beat is the
        project's.
    */
    Json notesInBeats (const model::Session& session,
                       const ProjectReadMarshal& marshal,
                       double offsetSeconds,
                       const std::vector<transcription::Note>& notes)
    {
        auto written = Json::array();

        marshal (
            [&]
            {
                for (const auto& note : notes)
                {
                    const auto from = offsetSeconds + note.startSeconds;
                    const auto startBeats = session.beatsAtSeconds (from);
                    const auto endBeats = session.beatsAtSeconds (from + note.lengthSeconds);

                    written.push_back (Json { { "pitch", note.pitch },
                                              { "startBeats", startBeats },
                                              { "lengthBeats", endBeats - startBeats },
                                              { "velocity", velocityOf (note.strength) } });
                }
            });

        return written;
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

    /** Whether the run that asked is still waiting for the answer. */
    using StillWanted = std::function<bool()>;

    /** What a call asked for, read out of its arguments — or, when it asked for
        something this tool cannot make sense of, what to tell the model so that
        it can correct itself and call again.
    */
    struct Asked
    {
        std::string trackId;
        analysisCall::BarRange bars;
        Aspects aspects;

        /** Empty when the call was well formed, and the whole of the refusal
            when it was not.
        */
        std::string wrong;
    };

    Asked readCall (const ToolCall& call)
    {
        Asked read;
        read.trackId = analysisCall::argument (call, "trackId");

        if (read.trackId.empty())
        {
            read.wrong = "this tool needs a trackId";

            return read;
        }

        const auto bars = analysisCall::barRangeOf (call);

        if (! bars.has_value())
        {
            read.wrong = "barRange is the first and last bar to look at, counting from 1";

            return read;
        }

        read.bars = *bars;

        const auto aspects = aspectsOf (call);

        if (! aspects.has_value())
        {
            read.wrong = "aspects is a list of what to estimate";

            return read;
        }

        if (aspects->nothing())
        {
            read.wrong = "the aspects this Duet can estimate are " + estimable();

            return read;
        }

        read.aspects = *aspects;

        return read;
    }

    /** What chord each bar of the plan fits best, as one estimate over the
        whole list.

        Nothing when no bar could be named, and nothing when the run stopped
        waiting part-way through, which the caller tells apart by asking again.
    */
    std::optional<Estimate>
        chordsOf (const Waveform& whole, const Plan& plan, const StillWanted& keepGoing)
    {
        auto named = Json::array();
        double confidence = 0.0;

        for (std::size_t bar = 0; bar + 1 < plan.barStarts.size(); ++bar)
        {
            if (! keepGoing())
                return {};

            const auto chord = analysis::estimatedChord (barOf (whole, plan, bar));

            if (chord.name.empty())
                continue;

            named.push_back (Json { { "bar", plan.firstBar + static_cast<int> (bar) },
                                    { "chord", chord.name } });
            confidence += chord.confidence;
        }

        if (named.empty())
            return {};

        // One confidence for the list, because the list is one estimate: what
        // it says is how well the bars it named fit the chords it named them,
        // on average.
        confidence /= static_cast<double> (named.size());

        return Estimate { std::move (named), std::string { analysis::chordMethod }, confidence };
    }

    /** The two aspects the transcription model answers, each as its own
        estimate and each absent when there was nothing to say.
    */
    struct Heard
    {
        std::optional<Estimate> notes;
        std::optional<Estimate> instrument;
    };

    /** One reading of the model, for both of them, because the instrument is a
        reading of the notes and not a second thing to hear.

        The model is asked between its windows whether anyone is still waiting,
        which is what makes the one part of this tool that takes real time
        cancelable.
    */
    Heard heardIn (const model::Session& session,
                   const ProjectReadMarshal& marshal,
                   const Waveform& stretch,
                   double offsetSeconds,
                   const Aspects& asked,
                   const StillWanted& keepGoing)
    {
        Heard made;
        const auto read = transcription::transcribed (stretch, keepGoing);

        if (! read.has_value())
            return made;

        if (asked.notes && ! read->notes.empty())
            made.notes = Estimate { notesInBeats (session, marshal, offsetSeconds, read->notes),
                                    std::string { transcription::notesMethod },
                                    read->confidence };

        if (asked.instrument)
            if (const auto named = transcription::instrumentOf (stretch, read->notes);
                ! named.name.empty())
                made.instrument = Estimate { named.name,
                                             std::string { transcription::instrumentMethod },
                                             named.confidence };

        return made;
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
    const auto asked = readCall (call);

    if (! asked.wrong.empty())
        return RpcOutcome::failure (rpcError::invalidParams, asked.wrong);

    const auto& id = asked.trackId;
    const auto plan = analysisCall::readPlan (session, marshal, id, asked.bars);

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
    const StillWanted keepGoing = [this, &call] { return renders.wanted (call.runId); };

    if (asked.aspects.key)
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

    if (! keepGoing())
        return analysisCall::abandoned();

    if (asked.aspects.chords)
        if (const auto chords = chordsOf (whole, *plan, keepGoing); chords.has_value())
            result["chords"] = ledger.record (call.runId, toolName, chordsAspect, *chords);

    if (! keepGoing())
        return analysisCall::abandoned();

    if (asked.aspects.transcribing())
    {
        const auto heard = heardIn (session,
                                    marshal,
                                    stretch,
                                    plan->wholeTrack ? 0.0 : plan->fromSeconds,
                                    asked.aspects,
                                    keepGoing);

        if (! keepGoing())
            return analysisCall::abandoned();

        if (heard.notes.has_value())
            result["notes"] = ledger.record (call.runId, toolName, notesAspect, *heard.notes);

        if (heard.instrument.has_value())
            result["instrument"] =
                ledger.record (call.runId, toolName, instrumentAspect, *heard.instrument);
    }

    return RpcOutcome::success (std::move (result));
}
} // namespace duet::collab
