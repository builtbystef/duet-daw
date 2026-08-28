#pragma once

#include <duet/collab/ProjectTools.h>
#include <duet/collab/ToolDispatch.h>

#include <duet/model/Session.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

/** What the two analysis tools have in common: reading a call that names a
    track and a stretch of it, and asking the project where that stretch is.

    Both `get_track_analysis` and `estimate_audio_content` take the same two
    arguments and answer about the same rendered audio, so what a call means is
    settled in one place — a bar range that means one thing to a measurement and
    another to an estimate would be worse than either.
*/
namespace duet::collab::analysisCall
{
/** What a tool answers a run that has stopped waiting for it. */
[[nodiscard]] RpcOutcome abandoned();

/** One of a call's arguments as text, and empty when it is not there or is not
    text. A number where an id belongs is a call this project cannot answer, and
    it goes down the same path as one naming a track that is not there.
*/
[[nodiscard]] std::string argument (const ToolCall& call, std::string_view name);

/** The bars a call asked about, first and last and both included, or the whole
    track when it asked about no bars in particular.
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
[[nodiscard]] std::optional<BarRange> barRangeOf (const ToolCall& call);

/** How many bars of music the project holds. */
[[nodiscard]] int barCountOf (const model::Session& session);

/** What the project has to say before anything is rendered: which track, what
    state it is in, and which stretch of the timeline was asked about — as a
    time, for what is read out of a waveform, and as bars, for what is answered
    bar by bar.
*/
struct Plan
{
    model::TrackRef track = model::noTrack;
    std::string digest;
    double fromSeconds = 0.0;
    double toSeconds = 0.0;
    bool wholeTrack = true;

    /** The bars the call covers, both included: what it asked for, or every bar
        the project holds when it asked for no bars in particular.
    */
    int firstBar = 1;
    int lastBar = 0;

    /** When each of those bars starts, in seconds, and last the moment the last
        of them ends — one more entry than there are bars.
    */
    std::vector<double> barStarts;
};

/** That, read on the thread the project is written on. Nothing when the project
    holds no such track.
*/
[[nodiscard]] std::optional<Plan> readPlan (const model::Session& session,
                                            const ProjectReadMarshal& marshal,
                                            const std::string& id,
                                            const BarRange& bars);
} // namespace duet::collab::analysisCall
