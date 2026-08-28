#pragma once

#include <duet/collab/Estimate.h>
#include <duet/collab/ProjectTools.h>
#include <duet/collab/ToolDispatch.h>
#include <duet/collab/TrackRenders.h>

#include <duet/model/Session.h>

namespace duet::collab
{
/** `estimate_audio_content`: what is probably being played on a track, for the
    things no routine can measure outright.

    Tier 3 of the analysis layer (spec js437t). What it answers about is the
    track's rendered output, read as pitch classes: the key the whole stretch
    fits best, and the chord each bar of it fits best. Both are readings of the
    notes rather than properties of the waveform, so both are guesses, and
    nothing here ever crosses the seam bare — every value is wrapped with the
    routine that made it and a confidence from 0 to 1 (ADR 0002), and the same
    act that wraps it writes it into the run's estimate ledger, which is what
    marks everything the run says afterwards.

    `aspects` says what to work out — "key", "chords", or both — and omitting it
    asks for everything this build can estimate. An aspect that was not asked
    for is not computed and is absent from the result rather than present and
    empty, and so is one that could not be read: a silent track has no key, and
    saying nothing is the honest answer where naming the key silence fits least
    badly is not.

    A bar range narrows what is read, first bar and last, counting from one and
    both included; without one the whole track is read and every bar of it is
    named. The render behind it is `TrackRenders`, shared with the measured
    analysis, so a track measured and estimated in the same run is rendered once
    and both answers come out of that one render, on the thread the call arrived
    on — the Collaborator service's own, never the message thread and never the
    audio thread.

    The session, the marshal, the renders and the ledger must all outlive this
    object, and this object must outlive the registry it was added to.
*/
class ContentEstimates
{
public:
    ContentEstimates (model::Session& projectSession,
                      ProjectReadMarshal readMarshal,
                      TrackRenders& trackRenders,
                      EstimateLedger& estimateLedger);

    ~ContentEstimates() = default;

    ContentEstimates (const ContentEstimates&) = delete;
    ContentEstimates (ContentEstimates&&) = delete;
    ContentEstimates& operator= (const ContentEstimates&) = delete;
    ContentEstimates& operator= (ContentEstimates&&) = delete;

    /** Adds `estimate_audio_content` to a registry, replacing a tool of that
        name.
    */
    void addTo (ToolRegistry& registry);

private:
    [[nodiscard]] RpcOutcome estimate (const ToolCall& call);

    model::Session& session;
    ProjectReadMarshal marshal;
    TrackRenders& renders;
    EstimateLedger& ledger;
};
} // namespace duet::collab
