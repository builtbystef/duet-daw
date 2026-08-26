#pragma once

#include <duet/collab/ToolDispatch.h>

#include <duet/model/Session.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace duet::collab
{
/** How the Tool Vocabulary writes the identity of a thing in the project.

    A ref is an integer the model hands out and the seam carries text, so an id
    is that integer with the kind of thing in front of it: `track-12`,
    `clip-13`, `plugin-14`, `note-1`. The kind is there for two reasons. A note's
    handle is Duet's own and counts from one, so it would otherwise collide with
    a track's; and an error about `track-99` says what was being looked for,
    while an error about `99` does not.

    The master is `track-master` rather than its ref, which is the largest number
    a 64-bit integer holds and says nothing to anyone reading a trace.
*/
namespace toolId
{
    [[nodiscard]] std::string forTrack (model::TrackRef track);
    [[nodiscard]] std::string forClip (model::ClipRef clip);
    [[nodiscard]] std::string forPlugin (model::PluginRef plugin);
    [[nodiscard]] std::string forNote (model::NoteRef note);

    /** The ref an id names, or nothing when the text is not an id of that kind.
        Whether the project holds such a thing is a separate question.
    */
    [[nodiscard]] std::optional<model::TrackRef> toTrack (std::string_view id);
    [[nodiscard]] std::optional<model::ClipRef> toClip (std::string_view id);
    [[nodiscard]] std::optional<model::PluginRef> toPlugin (std::string_view id);
} // namespace toolId

/** Runs one project read on the thread that owns the project model, and returns
    only once it has run.

    The message thread is the sole writer of the project model, so a tool reads
    the authoritative state by going there rather than by keeping a copy of it
    (spec js437t). The service thread marshals and waits; this is what it waits
    on, and whoever wires the Collaborator into the DAW supplies it, because this
    module links no JUCE and cannot reach a message thread on its own.
*/
using ProjectReadMarshal = std::function<void (const std::function<void()>&)>;

/** The five project-read tools of the Tool Vocabulary, answered from the live
    project model.

    `list_tracks`, `get_arrangement`, `get_midi`, `get_automation` and
    `get_plugin_chain` — everything the Collaborator can know about a project
    without measuring or guessing. Every value in a result is a bare scalar,
    because everything read from the project model is by construction a fact
    (ADR 0002); the one exception the contract names is a scanned plugin's own
    display text, whose meaning is the plugin's rather than Duet's, and which
    therefore crosses wrapped as an estimate.

    Buses are tracks: the master and every group are read through these same
    tools and are accepted wherever a track id is.

    Results are written stable content first and the content an edit moves last,
    which with this module's ordered JSON is the prompt-cache discipline the
    spec asks for: a fader change invalidates the tail of a cached tool result
    rather than its middle. Nothing in a result is a timestamp, and the same
    project state produces the same bytes however long apart the two calls are.

    The session and the marshal must both outlive this object, and this object
    must outlive the registry it was added to.
*/
class ProjectTools
{
public:
    ProjectTools (model::Session& projectSession, ProjectReadMarshal readMarshal);

    ~ProjectTools() = default;

    ProjectTools (const ProjectTools&) = delete;
    ProjectTools (ProjectTools&&) = delete;
    ProjectTools& operator= (const ProjectTools&) = delete;
    ProjectTools& operator= (ProjectTools&&) = delete;

    /** Adds the five tools to a registry, replacing tools of the same names. */
    void addTo (ToolRegistry& registry);

private:
    /** Answers one tool call by reading the project on the message thread. */
    [[nodiscard]] RpcOutcome
        read (const std::function<RpcOutcome (const model::Session&)>& body) const;

    model::Session& session;
    ProjectReadMarshal marshal;
};
} // namespace duet::collab
