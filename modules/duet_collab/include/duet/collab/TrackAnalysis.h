#pragma once

#include <duet/collab/ProjectTools.h>
#include <duet/collab/ToolDispatch.h>

#include <duet/model/Session.h>

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace duet::collab
{
/** Renders one track of the project offline into a file, and says whether it
    did.

    The one thing measured analysis needs of the engine, and the reason it is a
    function rather than a call: what a track puts out is the offline render
    path's to produce, and a run that has been abandoned asks it to stop through
    `keepGoing`, which it is asked between blocks.
*/
using TrackRenderer = std::function<bool (model::TrackRef track,
                                          const std::filesystem::path& destination,
                                          const std::function<bool()>& keepGoing)>;

/** The project's own offline render path, as the tool calls it: one track on
    its own, and for the master the whole project through the master chain,
    because that is what the master is.
*/
[[nodiscard]] TrackRenderer offlineTrackRenderer (model::Session& session);

/** Whether the run that asked for a measurement is still waiting for it.

    A render takes seconds and a producer may cancel inside them, so this is
    asked before a render starts, between its blocks, and between the routines
    that measure what it produced. Left unset, every run is still wanted.
*/
using RunStillWanted = std::function<bool (const std::string& runId)>;

/** `get_track_analysis`: what a track actually puts out, measured over its
    rendered audio.

    Everything it answers with is a bare scalar, because everything here is
    measured by a documented routine rather than guessed (ADR 0002): peak and
    true peak, RMS, integrated and short-term loudness, crest factor, the energy
    in each of the seven named bands, the spectral centroid and flatness, the
    stereo correlation and width, and where the track starts something, in
    beats.

    The measurement is made over the track's rendered output, so the first call
    on a track costs a render — seconds, sometimes — and the ones after it do
    not: the render is kept per track and keyed on that track's own edit state,
    so an edit to the track is what throws it away and an edit to another track
    is not. Rendering and measuring both happen on the thread the call arrived
    on, which is the Collaborator service's own; the project is read on the
    message thread and nothing else here touches it, so a producer keeps
    playing, editing and recording throughout.

    A bar range narrows what is measured, first bar and last, counting from one
    and both included. Onsets are found over the whole render and then read for
    the part that was asked about, so a note held across the start of a range is
    not mistaken for one struck there.

    Tools are called one at a time on the service thread, which is what lets the
    cache below hold no lock. The session, the marshal and the renderer must all
    outlive this object, and this object must outlive the registry it was added
    to.
*/
class TrackAnalysis
{
public:
    /** The renders are kept in `renderFolder`, and removed from it again when
        this object goes away.
    */
    TrackAnalysis (model::Session& projectSession,
                   ProjectReadMarshal readMarshal,
                   TrackRenderer renderTrack,
                   std::filesystem::path renderFolder,
                   RunStillWanted stillWanted = {});

    ~TrackAnalysis();

    TrackAnalysis (const TrackAnalysis&) = delete;
    TrackAnalysis (TrackAnalysis&&) = delete;
    TrackAnalysis& operator= (const TrackAnalysis&) = delete;
    TrackAnalysis& operator= (TrackAnalysis&&) = delete;

    /** Adds `get_track_analysis` to a registry, replacing a tool of that name. */
    void addTo (ToolRegistry& registry);

private:
    /** What a track's last render was, and what the project said when it was
        made.
    */
    struct Rendered
    {
        std::string digest;
        std::filesystem::path file;
    };

    [[nodiscard]] RpcOutcome analyse (const ToolCall& call);

    /** The file holding this track's render, rendering it first when nothing is
        kept for the track or what is kept is of a state it has since left.

        Nothing when the render did not happen, which is either a render that
        failed or one the run stopped waiting for; the caller asks `wanted`
        again to tell those apart.
    */
    [[nodiscard]] std::optional<std::filesystem::path>
        renderFor (model::TrackRef track, const std::string& digest, const std::string& runId);

    [[nodiscard]] bool wanted (const std::string& runId) const;

    model::Session& session;
    ProjectReadMarshal marshal;
    TrackRenderer render;
    std::filesystem::path folder;
    RunStillWanted stillWanted;

    std::map<model::TrackRef, Rendered> cache;
    int renders = 0;
};
} // namespace duet::collab
