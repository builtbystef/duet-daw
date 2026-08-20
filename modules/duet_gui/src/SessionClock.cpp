#include <duet/gui/SessionClock.h>

#include <duet/model/Session.h>

#include <algorithm>

namespace duet::gui
{
SessionClock::SessionClock (duet::model::Session& openProject) : session (openProject) {}

double SessionClock::beatsPerBar() const
{
    const auto signature = session.timeSignature();

    // A beat on the timeline is a quarter note, which is the unit the model
    // counts notes and clip loops in. A time signature whose beat is not a
    // quarter — 6/8 — is that many of its own beats, converted to quarters.
    const auto denominator = signature.denominator > 0 ? signature.denominator : 4;

    return static_cast<double> (std::max (1, signature.numerator)) * 4.0
           / static_cast<double> (denominator);
}

double SessionClock::playheadBeats() const
{
    return session.playbackPositionSeconds() * beatsPerSecond();
}

void SessionClock::setPlayheadBeats (double beats)
{
    session.setPlaybackPositionSeconds (std::max (0.0, beats) / beatsPerSecond());
}

bool SessionClock::isPlaying() const { return session.isPlaying(); }

double SessionClock::contentLengthBeats() const
{
    return session.editLengthSeconds() * beatsPerSecond();
}

double SessionClock::beatsPerSecond() const
{
    // A tempo of zero is not a project anyone can play, and dividing by it is
    // not a timeline anyone can draw.
    return std::max (1.0, session.tempoBpm()) / 60.0;
}
} // namespace duet::gui
