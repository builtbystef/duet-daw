#pragma once

#include <duet/collab/TaskRun.h>

#include <duet/gui/ArrangementView.h>

namespace duet::app
{
/** The producer at the moment they asked, in the interface's own terms: what
    the next message is about, where the playhead is, and whether the transport
    is rolling.

    The shell answers all four and this module turns them into what a Task Run
    carries, because the ids the seam speaks are the Collaborator's and the
    interface cannot name them (spec js437t).
*/
struct ProducerMoment
{
    /** The clips or the track the message is about: the producer's selection,
        or what they asked about from a context menu.
    */
    duet::gui::AskContext asked;

    /** Where the playhead is, in beats from the start of the project, and how
        many beats a bar of this project holds.
    */
    double playheadBeats = 0.0;
    double beatsPerBar = 4.0;

    bool transportPlaying = false;
};

/** What a Task Run carries about the producer at the moment it starts.

    Asked once per run and frozen into it: a selection the producer changes
    afterwards is a fact about a moment that has already passed (spec js437t).
*/
[[nodiscard]] duet::collab::OpeningContext openingContextOf (const ProducerMoment& moment);
} // namespace duet::app
