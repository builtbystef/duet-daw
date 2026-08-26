#pragma once

#include <duet/collab/JsonRpc.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace duet::collab
{
/** What the producer had selected when a Task Run began. */
enum class SelectionKind : std::uint8_t
{
    none,
    clips,
    tracks
};

/** What was true of the producer at the moment a Task Run started, and nowhere
    else: what they had selected, where the playhead was, and whether the
    transport was running.

    This is opening context rather than a tool because it describes the producer
    and not the project (spec js437t). A tool answering "what is selected" would
    be answering about a moment that has already passed by the time the model
    thinks to ask.
*/
struct OpeningContext
{
    SelectionKind selection = SelectionKind::none;

    /** What is selected: clip ids, or track ids, according to `selection`. */
    std::vector<std::string> selectionIds;

    int playheadBar = 1;
    double playheadBeat = 1.0;
    bool transportPlaying = false;
};

/** Whether a tool call is beginning or ending. */
enum class ToolPhase : std::uint8_t
{
    start,
    end
};

/** How a Task Run ended. Every run ends exactly once, in one of these three. */
enum class RunStatus : std::uint8_t
{
    completed,
    canceled,
    failed
};

/** The DAW side of a Task Run: what the Collaborator says while it works, and
    how the run ended.

    Every call arrives on the Collaborator service's own thread, in the order the
    sidecar sent it, and a run that has ended sends nothing more — exactly one
    terminal event per run, and never a word after it. A listener that wants to
    touch the interface marshals to the message thread itself; this module links
    no JUCE and cannot do it on their behalf.

    The service reads the listener and never owns it, so it must outlive the
    service or be cleared before it goes. A callback must not throw: it is called
    from the service thread with nothing above it to answer to, so an exception
    that leaves one ends the process.
*/
class TaskRunListener
{
public:
    virtual ~TaskRunListener() = default;

    TaskRunListener (const TaskRunListener&) = delete;
    TaskRunListener& operator= (const TaskRunListener&) = delete;

    /** One more piece of the Collaborator's commentary. A run's whole commentary
        is its deltas concatenated in the order they arrived.
    */
    virtual void commentaryDelta (const std::string& runId, const std::string& delta) = 0;

    /** A named tool call beginning or ending. */
    virtual void
        toolActivity (const std::string& runId, const std::string& tool, ToolPhase phase) = 0;

    /** The one ending a run has. `error` carries the reason when the status is
        `failed`, and is empty otherwise.
    */
    virtual void
        runFinished (const std::string& runId, RunStatus status, const std::string& error) = 0;

protected:
    TaskRunListener() = default;
};

/** What asking for a Task Run came back as: the run's id, or why there is none.

    There is no third answer. A run that was accepted has an id from this moment
    on and ends with a terminal event; a run that was not accepted never existed
    and no event will ever name it.
*/
struct RunStart
{
    bool started = false;
    std::string runId;
    RpcError error;

    [[nodiscard]] static RunStart accepted (std::string id) { return { true, std::move (id), {} }; }

    [[nodiscard]] static RunStart rejected (int code, std::string message)
    {
        return { false, {}, { code, std::move (message) } };
    }
};

/** What a run that never reached the backend fails with: no sidecar, a sidecar
    that died, or one that refused the request.

    The service supplies this one string, and every other failure carries the
    sidecar's own error text through unchanged, so a surface can show what it is
    given without deciding which kind of trouble it was looking at.
*/
inline constexpr const char* backendUnavailableMessage =
    "The Collaborator isn't working right now — try again later.";
} // namespace duet::collab
