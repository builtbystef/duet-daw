#include <duet/gui/TrackIo.h>

#include <algorithm>

namespace duet::gui
{
namespace
{
    constexpr const char* noneLabel = "None";

    std::string unavailableLabel (const std::string& name) { return "Unavailable — " + name; }

    bool alreadySelected (const TrackIoSnapshot& snap, duet::model::InputRef input)
    {
        return std::any_of (snap.inputs.begin(),
                            snap.inputs.end(),
                            [input] (const auto& choice)
                            { return choice.selected && choice.input == input; });
    }
} // namespace

void TrackIo::setSession (duet::model::Session* openProject)
{
    session = openProject;
    mixer.setSession (openProject);
}

TrackIoSnapshot TrackIo::snapshot (duet::model::TrackRef track) const
{
    TrackIoSnapshot snap;

    if (session == nullptr)
        return snap;

    const auto info = session->track (track);

    if (info.track == duet::model::noTrack)
        return snap;

    snap.track = info.track;
    snap.kind = info.kind;
    snap.output = info.output;
    snap.outputs = mixer.routingDestinations (track);
    snap.armed = info.recordArmed;
    snap.recording = snap.armed && session->isRecording();

    const auto assigned = session->assignedInput (track);
    const auto isGroup = info.kind == duet::model::TrackKind::group;
    const auto selectedIsAvailable = assigned.input != duet::model::noInput && assigned.available;

    snap.inputs.push_back (
        { duet::model::noInput, noneLabel, ! isGroup, assigned.input == duet::model::noInput });

    if (! isGroup && assigned.input != duet::model::noInput && ! assigned.available)
        snap.inputs.push_back ({ assigned.input, unavailableLabel (assigned.name), false, true });

    if (! isGroup)
    {
        const auto wanted = info.kind == duet::model::TrackKind::midi
                                ? duet::model::InputKind::midi
                                : duet::model::InputKind::audio;

        for (const auto& input : session->availableInputs())
        {
            if (input.kind != wanted)
                continue;

            snap.inputs.push_back ({ input.input,
                                     input.name,
                                     true,
                                     selectedIsAvailable && assigned.input == input.input });
        }
    }

    snap.monitoring.mode = selectedIsAvailable ? session->inputMonitoring (assigned.input)
                                               : duet::model::InputMonitoring::whileArmed;
    snap.monitoring.enabled = selectedIsAvailable;
    snap.armAvailable = selectedIsAvailable;

    return snap;
}

void TrackIo::setInput (duet::model::TrackRef track, duet::model::InputRef input)
{
    if (session == nullptr || alreadySelected (snapshot (track), input))
        return;

    session->setTrackInput (track, input);
}

void TrackIo::setMonitoring (duet::model::TrackRef track, duet::model::InputMonitoring mode)
{
    if (session == nullptr)
        return;

    const auto snap = snapshot (track);

    if (! snap.monitoring.enabled || snap.monitoring.mode == mode)
        return;

    session->setInputMonitoring (session->assignedInput (track).input, mode);
}

void TrackIo::setArmed (duet::model::TrackRef track, bool armed)
{
    if (session == nullptr)
        return;

    const auto snap = snapshot (track);

    if (snap.armed == armed)
        return;

    session->setTrackRecordArmed (track, armed);
}

void TrackIo::setOutput (duet::model::TrackRef track, duet::model::TrackRef destination)
{
    mixer.setOutput (track, destination);
}
} // namespace duet::gui
