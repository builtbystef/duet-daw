#include "SessionImpl.h"

#include <algorithm>
#include <cmath>
#include <utility>

/** The record path: where a take comes from, and what it leaves behind.

    Arming a track and choosing its input are written with no undo history, like
    the transport — they say where the next take comes from, and that is not
    something the project holds. The take itself is one Action, and it is
    stopRecording that opens it.
*/
namespace duet::model
{
namespace
{
    /** What a finished take is called in the undo history. */
    constexpr const char* recordTakeActionName = "Record Take";

    /** What the engine calls a take file: the track's name, and the first take
        number that is not taken yet.
    */
    juce::File takeFileIn (const juce::File& audioFolder,
                           const juce::String& trackName,
                           const juce::String& fileExtension)
    {
        const auto stem =
            juce::File::createLegalFileName (trackName.trim().isEmpty() ? "Take" : trackName);

        for (int take = 1;; ++take)
        {
            const auto file =
                audioFolder.getChildFile (stem + "_Take_" + juce::String { take } + fileExtension);

            if (! file.exists())
                return file;
        }
    }

    te::InputDevice::MonitorMode toEngineMonitorMode (InputMonitoring monitoring)
    {
        switch (monitoring)
        {
            case InputMonitoring::off:
                return te::InputDevice::MonitorMode::off;
            case InputMonitoring::on:
                return te::InputDevice::MonitorMode::on;
            case InputMonitoring::whileArmed:
                break;
        }

        return te::InputDevice::MonitorMode::automatic;
    }

    InputMonitoring toMonitoring (te::InputDevice::MonitorMode mode)
    {
        switch (mode)
        {
            case te::InputDevice::MonitorMode::off:
                return InputMonitoring::off;
            case te::InputDevice::MonitorMode::on:
                return InputMonitoring::on;
            case te::InputDevice::MonitorMode::automatic:
                break;
        }

        return InputMonitoring::whileArmed;
    }

    constexpr const char* incompatibleInputNotice = "This track cannot record from that input.";
    constexpr const char* missingInputNotice = "That input is not available.";
    constexpr const char* cannotArmNotice = "Choose an available input before arming this track.";
} // namespace

//==============================================================================
juce::File DuetBehaviour::getFileForNewAudioRecording (te::Track& track,
                                                       const juce::String& fileExtension)
{
    const auto directory = toJuceFile (*recordingDirectory);

    if (! directory.createDirectory())
        return {};

    return takeFileIn (directory, track.getName(), fileExtension);
}

void Session::setRecordingDirectory (std::filesystem::path directory)
{
    impl->recordingDirectory = std::move (directory);
}

//==============================================================================
InputRef Session::Impl::refForInput (const juce::String& deviceID) const
{
    const auto id = deviceID.toStdString();

    for (const auto& [ref, known] : inputsByRef)
        if (known.deviceID == id)
            return ref;

    const auto ref = nextInputRef++;
    inputsByRef.emplace (ref, KnownInput { id, {}, InputKind::audio });
    return ref;
}

InputRef Session::Impl::rememberInput (te::InputDevice& device) const
{
    const auto ref = refForInput (device.getDeviceID());
    auto& known = inputsByRef.at (ref);
    known.name = device.getName().toStdString();
    known.kind = device.isMidi() ? InputKind::midi : InputKind::audio;
    return ref;
}

te::InputDevice* Session::Impl::inputDeviceFor (InputRef ref) const
{
    const auto known = inputsByRef.find (ref);

    if (known == inputsByRef.end())
        return nullptr;

    return engine.getDeviceManager().findInputDeviceForID (juce::String { known->second.deviceID });
}

void Session::Impl::rememberTrackInput (TrackRef track, const AssignedInput& assigned)
{
    if (assigned.input == noInput)
    {
        rememberedTrackInputs.erase (track);
        return;
    }

    rememberedTrackInputs[track] = { assigned.input, assigned.name, assigned.kind };
}

AssignedInput Session::Impl::assignedInputOf (TrackRef track) const
{
    AssignedInput assigned;
    const auto remembered = rememberedTrackInputs.find (track);
    const auto destination = destinationStateFor (track);

    if (remembered != rememberedTrackInputs.end())
    {
        assigned.input = remembered->second.input;
        assigned.name = remembered->second.name;
        assigned.kind = remembered->second.kind;
    }
    else if (destination.isValid())
    {
        assigned.input = inputOfDestination (destination);
    }

    if (assigned.input == noInput)
        return {};

    if (auto* device = inputDeviceFor (assigned.input))
    {
        assigned.name = device->getName().toStdString();
        assigned.kind = device->isMidi() ? InputKind::midi : InputKind::audio;
        assigned.available = device->isEnabled();
        rememberInput (*device);
        return assigned;
    }

    if (const auto known = inputsByRef.find (assigned.input); known != inputsByRef.end())
    {
        if (assigned.name.empty())
            assigned.name = known->second.name;
        assigned.kind = known->second.kind;
    }

    assigned.available = false;
    return assigned;
}

void Session::Impl::disarmTrack (TrackRef track) const
{
    auto* audioTrack = trackFor (track);

    if (audioTrack == nullptr)
        return;

    auto destination = destinationStateFor (track);

    if (auto* instance = instanceFor (inputOfDestination (destination)))
        instance->setRecordingEnabled (audioTrack->itemID, false);
    else if (destination.isValid())
        destination.setProperty (te::IDs::armed, false, nullptr);
}

void Session::Impl::disarmTracksWithUnavailableInputs() const
{
    for (auto* track : te::getAudioTracks (*edit))
    {
        const auto ref = toRef<TrackRef> (track->itemID);
        const auto assigned = assignedInputOf (ref);

        if (assigned.input == noInput || assigned.available)
            continue;

        disarmTrack (ref);
    }
}

void Session::Impl::notifyProducer (const std::string& message) const
{
    if (engineMessage)
        engineMessage (message);
}

te::InputDeviceInstance* Session::Impl::instanceFor (InputRef ref) const
{
    auto* device = inputDeviceFor (ref);

    if (device == nullptr)
        return nullptr;

    edit->getTransport().ensureContextAllocated();
    auto* context = edit->getCurrentPlaybackContext();

    return context != nullptr ? context->getInputFor (device) : nullptr;
}

juce::ValueTree Session::Impl::destinationStateFor (TrackRef track) const
{
    const auto wanted = toItemID (track);
    const auto inputs = edit->state.getChildWithName (te::IDs::INPUTDEVICES);

    for (const auto& input : inputs)
        for (const auto& destination : input)
            if (te::EditItemID::fromProperty (destination, te::IDs::targetID) == wanted)
                return destination;

    return {};
}

InputRef Session::Impl::inputOfDestination (const juce::ValueTree& destination) const
{
    if (! destination.isValid())
        return noInput;

    return refForInput (destination.getParent()[te::IDs::deviceID].toString());
}

//==============================================================================
std::vector<InputInfo> Session::availableInputs() const
{
    std::vector<InputInfo> out;
    auto& deviceManager = impl->engine.getDeviceManager();

    for (int index = 0; index < deviceManager.getNumInputDevices(); ++index)
    {
        auto* device = deviceManager.getInputDevice (index);

        if (device == nullptr || ! device->isEnabled())
            continue;

        out.push_back ({ impl->rememberInput (*device),
                         device->getName().toStdString(),
                         device->isMidi() ? InputKind::midi : InputKind::audio });
    }

    return out;
}

AssignedInput Session::assignedInput (TrackRef track) const
{
    return impl->assignedInputOf (track);
}

void Session::setTrackInput (TrackRef track, InputRef input)
{
    auto* audioTrack = impl->trackFor (track);

    if (audioTrack == nullptr)
        return;

    const auto current = impl->assignedInputOf (track);

    if (current.input == input)
        return;

    if (input != noInput)
    {
        const auto kind = trackKindOf (*audioTrack);
        auto* device = impl->inputDeviceFor (input);

        if (kind == TrackKind::group)
        {
            impl->notifyProducer (incompatibleInputNotice);
            return;
        }

        if (device == nullptr || ! device->isEnabled())
        {
            impl->notifyProducer (missingInputNotice);
            return;
        }

        const auto inputKind = device->isMidi() ? InputKind::midi : InputKind::audio;

        if ((kind == TrackKind::midi) != (inputKind == InputKind::midi))
        {
            impl->notifyProducer (incompatibleInputNotice);
            return;
        }
    }

    // Whatever fed this track before stops feeding it, so that a track records
    // from one input and the read back is unambiguous.
    for (auto* instance : impl->edit->getAllInputDevices())
        if (instance->getTargets().contains (audioTrack->itemID)) [[maybe_unused]]
            const auto removed = instance->removeTarget (audioTrack->itemID, nullptr);

    if (input == noInput)
    {
        impl->rememberTrackInput (track, {});
        return;
    }

    if (auto* instance = impl->instanceFor (input))
    {
        // Moved, not shared: an input feeds one track, so it leaves whichever
        // track it fed before.
        [[maybe_unused]]
        const auto assigned = instance->setTarget (audioTrack->itemID, true, nullptr);
        auto* device = impl->inputDeviceFor (input);
        const AssignedInput remembered { impl->rememberInput (*device),
                                         device->getName().toStdString(),
                                         device->isMidi() ? InputKind::midi : InputKind::audio,
                                         true };
        impl->rememberTrackInput (track, remembered);

        // The input now feeds this track, so it no longer feeds the one it left.
        std::erase_if (impl->rememberedTrackInputs,
                       [track, input] (const auto& entry)
                       { return entry.first != track && entry.second.input == input; });
    }
}

void Session::setTrackRecordArmed (TrackRef track, bool armed)
{
    auto* audioTrack = impl->trackFor (track);

    if (audioTrack == nullptr)
        return;

    const auto current = impl->assignedInputOf (track);
    const auto destination = impl->destinationStateFor (track);
    const bool currentlyArmed =
        destination.isValid() && static_cast<bool> (destination[te::IDs::armed]);

    if (currentlyArmed == armed)
        return;

    if (armed && (trackKindOf (*audioTrack) == TrackKind::group || ! current.available))
    {
        impl->notifyProducer (cannotArmNotice);
        return;
    }

    if (auto* instance = impl->instanceFor (impl->inputOfDestination (destination)))
        instance->setRecordingEnabled (audioTrack->itemID, armed);
}

void Session::setInputMonitoring (InputRef input, InputMonitoring monitoring)
{
    if (auto* device = impl->inputDeviceFor (input))
        device->setMonitorMode (toEngineMonitorMode (monitoring));
}

InputMonitoring Session::inputMonitoring (InputRef input) const
{
    if (auto* device = impl->inputDeviceFor (input))
        return toMonitoring (device->getMonitorMode());

    return InputMonitoring::whileArmed;
}

//==============================================================================
std::unordered_set<ClipRef> Session::Impl::allClips() const
{
    std::unordered_set<ClipRef> out;

    for (auto* track : te::getAudioTracks (*edit))
        for (auto* clip : track->getClips())
            out.insert (toRef<ClipRef> (clip->itemID));

    return out;
}

std::unordered_map<TrackRef, std::filesystem::path> Session::Impl::recordingFiles() const
{
    std::unordered_map<TrackRef, std::filesystem::path> out;

    for (auto* instance : edit->getAllInputDevices())
        for (const auto target : instance->getTargets())
            if (const auto file = instance->getRecordingFile (target); file != juce::File())
                out.emplace (toRef<TrackRef> (target), toPath (file));

    return out;
}

void Session::Impl::pinRecordedSources (
    const std::unordered_set<ClipRef>& clipsBefore,
    const std::unordered_map<TrackRef, std::filesystem::path>& files) const
{
    for (auto* track : te::getAudioTracks (*edit))
    {
        const auto file = files.find (toRef<TrackRef> (track->itemID));

        if (file == files.end())
            continue;

        for (auto* clip : track->getClips())
        {
            if (clipsBefore.contains (toRef<ClipRef> (clip->itemID)))
                continue;

            // Hazard 5, on the record path: the engine writes a recorded clip's
            // path relative to the edit file and reads it back relative to the
            // folder that holds that file, one level apart. This is the
            // reference the project reads.
            if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip))
                audioClip->getSourceFileReference().source =
                    juce::String { projectReferenceTo (projectFolder, file->second) };
        }
    }
}

//==============================================================================
bool Session::Impl::devicesAreSettled() const
{
    if (! deviceListIsBuilt())
        return false;

    return ! lastDeviceChangeMs.has_value() || nowMs() - *lastDeviceChangeMs >= deviceQuietMs;
}

void Session::Impl::beginTake()
{
    auto& transport = edit->getTransport();
    transport.ensureContextAllocated();

    // Arming a track changes the graph, and the graph is what records.
    edit->dispatchPendingUpdatesSynchronously();

    transport.record (false);
    syncMeters();
}

void Session::Impl::startTakeWhenDevicesAreSettled()
{
    if (! devicesAreSettled() && ++waitedForTheDevices <= deviceWaitAttempts)
        return;

    takeStarter.stopTimer();
    beginTake();
}

void Session::startRecording()
{
    // Nothing asks for plain playback any more: a take is what was asked for,
    // and the keeper asking again would turn it back into a play.
    impl->playbackKeeper.stopTimer();

    // The pre-roll, and what this session does about hazard 6 on the record
    // path. A take the engine's device rebuild interrupts cannot be continued —
    // TransportControl::record starts a take at the playhead, so asking again
    // would land the first half as one clip and start a second, which is worse
    // than losing the take — so the rebuild happens before the take rather than
    // during it. The engine's own timer would build the device list four
    // seconds into the session; asked for here it takes milliseconds, and the
    // take starts once the engine has stopped changing what it built.
    if (! impl->devicesAreSettled())
    {
        impl->askForTheDeviceList();
        impl->waitedForTheDevices = 0;
        impl->takeStarter.startTimer (impl->devicePollMs);
        return;
    }

    impl->beginTake();
}

void Session::stopRecording()
{
    if (! isRecording())
    {
        stopPlayback();
        return;
    }

    const auto clipsBefore = impl->allClips();
    const auto files = impl->recordingFiles();

    // Where the take becomes an Action. Everything the engine writes as it lands
    // a take goes through the project's undo history, so a transaction opened
    // here — and, as everywhere, deliberately not sealed — collects all of it
    // into one named step (ADR 0004).
    const te::Edit::UndoTransactionInhibitor keepTheActionWhole { *impl->edit };
    impl->undoManager().beginNewTransaction (juce::String { recordTakeActionName });

    impl->playbackKeeper.stopTimer();
    impl->edit->getTransport().stop (false, false);

    impl->pinRecordedSources (clipsBefore, files);
    impl->settleEngineBookkeeping();
    impl->applyLoopRange();
    impl->announceChange();
}

bool Session::isRecording() const { return impl->edit->getTransport().isRecording(); }
} // namespace duet::model
