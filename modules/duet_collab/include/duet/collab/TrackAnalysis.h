#pragma once

#include <duet/collab/ProjectTools.h>
#include <duet/collab/ToolDispatch.h>
#include <duet/collab/TrackRenders.h>

#include <duet/model/Session.h>

namespace duet::collab
{
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
    not: the renders are `TrackRenders`, kept per track and keyed on that
    track's own edit state. Rendering and measuring both happen on the thread
    the call arrived on, which is the Collaborator service's own; the project is
    read on the message thread and nothing else here touches it, so a producer
    keeps playing, editing and recording throughout.

    A bar range narrows what is measured, first bar and last, counting from one
    and both included. Onsets are found over the whole render and then read for
    the part that was asked about, so a note held across the start of a range is
    not mistaken for one struck there.

    Tools are called one at a time on the service thread. The session, the
    marshal and the renders must all outlive this object, and this object must
    outlive the registry it was added to.
*/
class TrackAnalysis
{
public:
    TrackAnalysis (model::Session& projectSession,
                   ProjectReadMarshal readMarshal,
                   TrackRenders& trackRenders);

    ~TrackAnalysis() = default;

    TrackAnalysis (const TrackAnalysis&) = delete;
    TrackAnalysis (TrackAnalysis&&) = delete;
    TrackAnalysis& operator= (const TrackAnalysis&) = delete;
    TrackAnalysis& operator= (TrackAnalysis&&) = delete;

    /** Adds `get_track_analysis` to a registry, replacing a tool of that name. */
    void addTo (ToolRegistry& registry);

private:
    [[nodiscard]] RpcOutcome analyse (const ToolCall& call);

    model::Session& session;
    ProjectReadMarshal marshal;
    TrackRenders& renders;
};
} // namespace duet::collab
