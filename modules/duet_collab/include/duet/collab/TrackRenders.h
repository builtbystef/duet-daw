#pragma once

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

    The one thing the analysis layer needs of the engine, and the reason it is a
    function rather than a call: what a track puts out is the offline render
    path's to produce, and a run that has been abandoned asks it to stop through
    `keepGoing`, which it is asked between blocks.
*/
using TrackRenderer = std::function<bool (model::TrackRef track,
                                          const std::filesystem::path& destination,
                                          const std::function<bool()>& keepGoing)>;

/** The project's own offline render path, as the analysis layer calls it: one
    track on its own, and for the master the whole project through the master
    chain, because that is what the master is.

    Off a detached copy of the project, because a measurement is asked for while
    the producer works: an Edit that is rendering is an Edit that is not
    playing, and the project is the Edit the producer is playing.
*/
[[nodiscard]] TrackRenderer offlineTrackRenderer (model::Session& session);

/** Whether the run that asked for a render is still waiting for it.

    A render takes seconds and a producer may cancel inside them, so this is
    asked before a render starts, between its blocks, and between the routines
    that read what it produced. Left unset, every run is still wanted.
*/
using RunStillWanted = std::function<bool (const std::string& runId)>;

/** What each track last rendered to, kept for as long as the track has not
    moved.

    The analysis layer's expensive half: a render costs seconds and every tool
    that reads audio needs the same one, so it is made once per track and kept,
    keyed on that track's own edit state. An edit to a track is what throws its
    render away; an edit to another track is not. What is kept is the file and
    not the numbers read off it, so a call about any part of a track is answered
    from one render of the whole of it, and the memory this costs is the same
    however long the project is.

    One object serves every tool that reads rendered audio, which is what makes
    a measured and an estimated answer about the same track cost one render
    between them.

    Tools are called one at a time on the service thread, which is what lets
    this hold no lock. The renderer must outlive this object.
*/
class TrackRenders
{
public:
    /** The renders are kept in `renderFolder`, and removed from it again when
        this object goes away.
    */
    TrackRenders (TrackRenderer renderTrack,
                  std::filesystem::path renderFolder,
                  RunStillWanted runStillWanted = {});

    ~TrackRenders();

    TrackRenders (const TrackRenders&) = delete;
    TrackRenders (TrackRenders&&) = delete;
    TrackRenders& operator= (const TrackRenders&) = delete;
    TrackRenders& operator= (TrackRenders&&) = delete;

    /** The file holding this track's render, rendering it first when nothing is
        kept for the track or what is kept is of a state it has since left.

        Nothing when the render did not happen, which is either a render that
        failed or one the run stopped waiting for; the caller asks `wanted`
        again to tell those apart.
    */
    [[nodiscard]] std::optional<std::filesystem::path>
        fileFor (model::TrackRef track, const std::string& digest, const std::string& runId);

    /** Whether that run is still waiting for what it asked for. */
    [[nodiscard]] bool wanted (const std::string& runId) const;

private:
    /** What a track's last render was, and what the project said when it was
        made.
    */
    struct Rendered
    {
        std::string digest;
        std::filesystem::path file;
    };

    TrackRenderer render;
    std::filesystem::path folder;
    RunStillWanted stillWanted;

    std::map<model::TrackRef, Rendered> cache;
    int renders = 0;
};
} // namespace duet::collab
