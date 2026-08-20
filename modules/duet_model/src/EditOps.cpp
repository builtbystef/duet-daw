#include "SessionImpl.h"

#include <algorithm>

namespace duet::model
{
namespace
{
    /** Where a track's kind is stored, and the one value worth storing: the
        other two kinds are legible from the track itself.
    */
    constexpr const char* trackKindProperty = "duetTrackKind";
    constexpr const char* groupKindName = "group";

    /** The engine names the fader's two parameters this, and Duet's are them. */
    constexpr const char* volumeParameterID = "volume";
    constexpr const char* panParameterID = "pan";

    /** The engine's aux buses are numbered, and Duet's are tracks, so a bus a
        track has never returned from has to be given a number. Sixteen is what
        the engine's own bus naming stops at.
    */
    constexpr int maxAuxBuses = 16;

    te::TimeRange timeRange (double startSeconds, double lengthSeconds)
    {
        const auto start = te::TimePosition::fromSeconds (startSeconds);
        return { start, start + te::TimeDuration::fromSeconds (lengthSeconds) };
    }

    /** The value an automation curve stores for a target, given the value the
        facade speaks: a fader curve is written in decibels and stored as a
        fader position, and everything else stores what it is given.
    */
    float toCurveValue (const AutomationTarget& target, double value)
    {
        if (target.kind == AutomationTarget::Kind::trackVolume)
            return te::decibelsToVolumeFaderPosition (static_cast<float> (value));

        return static_cast<float> (value);
    }

    te::TimePosition pointTime (const te::AutomationCurve& curve,
                                int index,
                                const te::TempoSequence& tempoSequence)
    {
        return te::toTime (curve.getPointPosition (index), tempoSequence);
    }

    /** Where a curve already has a point at a time, or −1 when it has none.

        The times are the ones the facade wrote, and a position goes in and comes
        back the same, so this asks for the same time and not a nearby one.
    */
    int indexOfPointAt (const te::AutomationCurve& curve,
                        double timeSeconds,
                        const te::TempoSequence& tempoSequence)
    {
        const auto wanted = te::TimePosition::fromSeconds (timeSeconds);

        for (int point = 0; point < curve.getNumPoints(); ++point)
            if (pointTime (curve, point, tempoSequence) == wanted)
                return point;

        return -1;
    }

    /** Finds the send a track already has into a bus number, if any. */
    te::AuxSendPlugin* sendOn (te::AudioTrack& track, int busNumber)
    {
        for (auto* plugin : track.pluginList.getPlugins())
            if (auto* send = dynamic_cast<te::AuxSendPlugin*> (plugin))
                if (send->busNumber == busNumber)
                    return send;

        return nullptr;
    }

    /** Says how long an audio clip's source is in musical time, when nothing has
        said yet.

        The engine refuses to loop a source whose musical length it does not
        know, and a recording or a plain wave file carries no such length. This
        is the same claim the producer makes by switching a clip to loop: the
        file runs for this many beats of the project's meter.
    */
    void declareSourceLengthInBeats (te::AudioClipBase& clip, te::TempoSequence& tempoSequence)
    {
        auto& loopInfo = clip.getLoopInfo();

        if (loopInfo.isLoopable())
            return;

        const auto bpm = tempoSequence.getBpmAt (te::TimePosition());
        const auto beats = clip.getSourceLength().inSeconds() * bpm / 60.0;

        if (beats <= 0.0)
            return;

        if (auto* timeSig = tempoSequence.getTimeSig (0))
        {
            loopInfo.setNumerator (timeSig->numerator);
            loopInfo.setDenominator (timeSig->denominator);
        }

        loopInfo.setNumBeats (beats);
    }

    /** A bus number nothing returns from yet. */
    int unusedBusNumber (te::Edit& edit)
    {
        std::vector<bool> taken (maxAuxBuses, false);

        for (auto* track : te::getAudioTracks (edit))
            if (auto* auxReturn = returnOn (*track))
                if (const int bus = auxReturn->busNumber; bus >= 0 && bus < maxAuxBuses)
                    taken[static_cast<std::size_t> (bus)] = true;

        for (int bus = 0; bus < maxAuxBuses; ++bus)
            if (! taken[static_cast<std::size_t> (bus)])
                return bus;

        return -1;
    }
} // namespace

//==============================================================================
std::string projectReferenceTo (const std::filesystem::path& projectFolder,
                                const std::filesystem::path& sourceFile)
{
    const auto relative =
        sourceFile.lexically_normal().lexically_relative (projectFolder.lexically_normal());
    const bool insideProject = ! relative.empty() && *relative.begin() != "..";

    return insideProject ? relative.generic_string() : sourceFile.generic_string();
}

TrackKind trackKindOf (te::AudioTrack& track)
{
    if (track.state[juce::Identifier { trackKindProperty }].toString() == groupKindName)
        return TrackKind::group;

    // The other two kinds the track itself tells: a midi track is one with an
    // instrument at the head of its chain to drive. That derivation is what
    // lets a track the engine made — the one a new edit starts with — answer
    // without ever having been through createTrack.
    for (auto* plugin : track.pluginList.getPlugins())
        if (const auto builtin = builtinOf (*plugin))
            if (*builtin == BuiltinPlugin::synth || *builtin == BuiltinPlugin::sampler)
                return TrackKind::midi;

    return TrackKind::audio;
}

const char* engineTypeOf (BuiltinPlugin plugin)
{
    switch (plugin)
    {
        case BuiltinPlugin::eq:
            return te::EqualiserPlugin::xmlTypeName;
        case BuiltinPlugin::compressor:
            return te::CompressorPlugin::xmlTypeName;
        case BuiltinPlugin::reverb:
            return te::ReverbPlugin::xmlTypeName;
        case BuiltinPlugin::synth:
            return te::FourOscPlugin::xmlTypeName;
        case BuiltinPlugin::sampler:
            return te::SamplerPlugin::xmlTypeName;
    }

    return nullptr;
}

std::optional<BuiltinPlugin> builtinOf (te::Plugin& plugin)
{
    const auto type = plugin.getPluginType();

    for (const auto builtin : { BuiltinPlugin::eq,
                                BuiltinPlugin::compressor,
                                BuiltinPlugin::reverb,
                                BuiltinPlugin::synth,
                                BuiltinPlugin::sampler })
        if (type == engineTypeOf (builtin))
            return builtin;

    return {};
}

te::AutomatableParameter* Session::Impl::parameterFor (const AutomationTarget& target) const
{
    switch (target.kind)
    {
        case AutomationTarget::Kind::trackVolume:
            if (auto* fader = faderFor (target.track))
                return fader->getAutomatableParameterByID (volumeParameterID).get();
            break;

        case AutomationTarget::Kind::trackPan:
            if (auto* fader = faderFor (target.track))
                return fader->getAutomatableParameterByID (panParameterID).get();
            break;

        case AutomationTarget::Kind::pluginParameter:
            if (auto* plugin = pluginFor (target.plugin))
                return plugin->getAutomatableParameterByID (toJuceString (target.parameterId))
                    .get();
            break;
    }

    return nullptr;
}

void stateParametersExplicitly (te::Plugin& plugin)
{
    // Notifying is the point: the engine writes the value out through the
    // parameter's stored value only on the notifying path, and writes it with no
    // undo manager, which is what this has to be — the state gains no meaning it
    // did not already have, so there is nothing here for an undo to revert.
    // The explicit value, never the current one: a project that has just been
    // read has its parameters already sitting where their automation curves put
    // them at time zero, and the value that has to be stated is the one the
    // producer set.
    for (auto* parameter : plugin.getAutomatableParameters())
        parameter->setParameter (parameter->getCurrentExplicitValue(), juce::sendNotification);
}

void Session::Impl::refreshParametersFromState() const
{
    for (auto* plugin : te::getAllPlugins (*edit, true))
        for (auto* parameter : plugin->getAutomatableParameters())
            parameter->updateFromAttachedValue();
}

NoteRef Session::Impl::refForNote (ClipRef clip, const juce::ValueTree& noteState) const
{
    for (const auto& [ref, handle] : notesByRef)
        if (handle.state == noteState)
            return ref;

    const auto ref = nextNoteRef++;
    notesByRef.emplace (ref, NoteHandle { clip, noteState });
    return ref;
}

te::MidiNote* Session::Impl::noteFor (NoteRef ref) const
{
    const auto found = notesByRef.find (ref);

    if (found == notesByRef.end())
        return nullptr;

    if (auto* clip = midiClipFor (found->second.clip))
        return clip->getSequence().getNoteFor (found->second.state);

    return nullptr;
}

//==============================================================================
AutomationTarget AutomationTarget::trackVolumeOf (TrackRef track)
{
    return { Kind::trackVolume, track, noPlugin, {} };
}

AutomationTarget AutomationTarget::trackPanOf (TrackRef track)
{
    return { Kind::trackPan, track, noPlugin, {} };
}

AutomationTarget AutomationTarget::parameterOf (PluginRef plugin, std::string_view parameterId)
{
    return { Kind::pluginParameter, noTrack, plugin, std::string { parameterId } };
}

//==============================================================================
EditOps::EditOps (Session& owner) noexcept : session (owner) {}

TrackRef EditOps::createTrack (TrackKind kind,
                               std::string_view name,
                               std::optional<BuiltinPlugin> instrument)
{
    auto& edit = *session.impl->edit;
    auto track = edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr);

    if (track == nullptr)
        return noTrack;

    track->setName (toJuceString (name));

    // A group is a bus: what comes out of it is what the tracks routed into it
    // put in. Its own output still goes to the device, like any other track's —
    // a track with no output at all is one nobody can hear, because the
    // playback graph wraps such a track in a node that blocks its audio. Only
    // the designation is Duet's to store.
    if (kind == TrackKind::group)
        track->state.setProperty (
            juce::Identifier { trackKindProperty }, groupKindName, &session.impl->undoManager());

    for (auto* plugin : track->pluginList.getPlugins())
        stateParametersExplicitly (*plugin);

    const auto ref = toRef<TrackRef> (track->itemID);

    if (kind == TrackKind::midi && instrument.has_value())
        addPlugin (ref, *instrument, 0);

    return ref;
}

void EditOps::removeTrack (TrackRef track)
{
    if (auto* audioTrack = session.impl->trackFor (track))
        session.impl->edit->deleteTrack (audioTrack);
}

void EditOps::renameTrack (TrackRef track, std::string_view newName)
{
    if (auto* audioTrack = session.impl->trackFor (track))
        audioTrack->setName (toJuceString (newName));
}

void EditOps::moveTrack (TrackRef track, int newIndex)
{
    auto* audioTrack = session.impl->trackFor (track);

    if (audioTrack == nullptr)
        return;

    auto& edit = *session.impl->edit;
    auto order = te::getAudioTracks (edit);
    order.removeAllInstancesOf (audioTrack);

    // The engine places a track after the one it is told to follow, so the track
    // that ends up before this one is what an index means here.
    const auto placeAfter = juce::jlimit (0, order.size(), newIndex) - 1;
    const auto preceding = placeAfter >= 0 ? order[placeAfter]->itemID : te::EditItemID();

    edit.moveTrack (audioTrack, te::TrackInsertPoint { te::EditItemID(), preceding });
}

void EditOps::setTrackOutput (TrackRef track, TrackRef bus)
{
    auto* audioTrack = session.impl->trackFor (track);

    if (audioTrack == nullptr)
        return;

    if (bus == noTrack)
    {
        audioTrack->getOutput().setOutputToDefaultDevice (false);
        return;
    }

    if (auto* destination = session.impl->trackFor (bus))
        audioTrack->getOutput().setOutputToTrack (destination);
}

//==============================================================================
ClipRef EditOps::insertAudioClip (TrackRef track,
                                  std::string_view name,
                                  const std::filesystem::path& sourceFile,
                                  double startSeconds,
                                  double lengthSeconds)
{
    auto* audioTrack = session.impl->trackFor (track);

    if (audioTrack == nullptr)
        return noClip;

    const te::ClipPosition position { timeRange (startSeconds, lengthSeconds) };

    if (auto clip = audioTrack->insertWaveClip (
            toJuceString (name), toJuceFile (sourceFile), position, false))
    {
        // Hazard 5: the reference the engine stores by default resolves against
        // a temporary directory, and the clip plays silence. Pin it here, to the
        // path the project keeps.
        clip->getSourceFileReference().source =
            juce::String { projectReferenceTo (session.impl->projectFolder, sourceFile) };
        return toRef<ClipRef> (clip->itemID);
    }

    return noClip;
}

ClipRef EditOps::insertMidiClip (TrackRef track,
                                 std::string_view name,
                                 double startSeconds,
                                 double lengthSeconds)
{
    auto* audioTrack = session.impl->trackFor (track);

    if (audioTrack == nullptr)
        return noClip;

    if (auto clip = audioTrack->insertMIDIClip (
            toJuceString (name), timeRange (startSeconds, lengthSeconds), nullptr))
        return toRef<ClipRef> (clip->itemID);

    return noClip;
}

void EditOps::moveClip (ClipRef clip, double newStartSeconds)
{
    if (auto* c = session.impl->clipFor (clip))
        c->setStart (te::TimePosition::fromSeconds (newStartSeconds), false, true);
}

void EditOps::trimClip (ClipRef clip, double newLengthSeconds)
{
    if (auto* c = session.impl->clipFor (clip))
        c->setLength (te::TimeDuration::fromSeconds (newLengthSeconds), true);
}

void EditOps::deleteClip (ClipRef clip)
{
    if (auto* c = session.impl->clipFor (clip))
        c->removeFromParent();
}

void EditOps::setClipLoop (ClipRef clip, bool looped, double loopLengthBeats)
{
    auto* c = session.impl->clipFor (clip);

    if (c == nullptr)
        return;

    if (! looped)
    {
        c->disableLooping();
        return;
    }

    if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (c))
        declareSourceLengthInBeats (*audioClip, session.impl->edit->tempoSequence);

    if (! c->canLoop())
        return;

    // In beats, so that a loop keeps its musical length when the tempo moves.
    c->setLoopRangeBeats ({ te::BeatPosition(), te::BeatDuration::fromBeats (loopLengthBeats) });
}

ClipRef EditOps::duplicateClip (ClipRef clip, TrackRef toTrack, double startSeconds)
{
    auto* source = session.impl->clipFor (clip);

    if (source == nullptr)
        return noClip;

    auto* destination = toTrack == noTrack ? dynamic_cast<te::ClipTrack*> (source->getTrack())
                                           : session.impl->trackFor (toTrack);

    if (destination == nullptr)
        return noClip;

    const te::ClipPosition position { timeRange (startSeconds,
                                                 source->getPosition().getLength().inSeconds()),
                                      source->getPosition().getOffset() };

    // A copy, not the clip's own state: the engine inserts the tree it is given
    // and stamps a fresh item ID into it. The copy carries the source reference
    // with it, which the original already pinned relative to the project.
    if (auto* copy = destination->insertClipWithState (
            source->state.createCopy(), source->getName(), source->type, position, false, false))
        return toRef<ClipRef> (copy->itemID);

    return noClip;
}

//==============================================================================
NoteRef
    EditOps::addNote (ClipRef clip, int pitch, double startBeats, double lengthBeats, int velocity)
{
    auto* midiClip = session.impl->midiClipFor (clip);

    if (midiClip == nullptr)
        return noNote;

    if (auto* note = midiClip->getSequence().addNote (pitch,
                                                      te::BeatPosition::fromBeats (startBeats),
                                                      te::BeatDuration::fromBeats (lengthBeats),
                                                      velocity,
                                                      0,
                                                      &session.impl->undoManager()))
        return session.impl->refForNote (clip, note->state);

    return noNote;
}

void EditOps::removeNote (NoteRef note)
{
    const auto found = session.impl->notesByRef.find (note);

    if (found == session.impl->notesByRef.end())
        return;

    if (auto* midiClip = session.impl->midiClipFor (found->second.clip))
        if (auto* midiNote = midiClip->getSequence().getNoteFor (found->second.state))
            midiClip->getSequence().removeNote (*midiNote, &session.impl->undoManager());
}

void EditOps::moveNote (NoteRef note, int newPitch, double newStartBeats)
{
    if (auto* midiNote = session.impl->noteFor (note))
    {
        midiNote->setNoteNumber (newPitch, &session.impl->undoManager());
        midiNote->setStartAndLength (te::BeatPosition::fromBeats (newStartBeats),
                                     midiNote->getLengthBeats(),
                                     &session.impl->undoManager());
    }
}

void EditOps::resizeNote (NoteRef note, double newLengthBeats)
{
    if (auto* midiNote = session.impl->noteFor (note))
        midiNote->setStartAndLength (midiNote->getStartBeat(),
                                     te::BeatDuration::fromBeats (newLengthBeats),
                                     &session.impl->undoManager());
}

void EditOps::setNoteVelocity (NoteRef note, int velocity)
{
    if (auto* midiNote = session.impl->noteFor (note))
        midiNote->setVelocity (velocity, &session.impl->undoManager());
}

//==============================================================================
void EditOps::setTrackVolumeDb (TrackRef track, double db)
{
    if (auto* fader = session.impl->faderFor (track))
        fader->setVolumeDb (static_cast<float> (db));
}

void EditOps::setTrackPan (TrackRef track, double pan)
{
    if (auto* fader = session.impl->faderFor (track))
        fader->setPan (static_cast<float> (pan));
}

void EditOps::setTrackMute (TrackRef track, bool muted)
{
    // Written straight to the state, not through the engine's setter: the engine
    // binds mute and solo with no undo manager, because in its model they are
    // monitoring controls rather than edits. In Duet they are mixer values a
    // Suggestion can set, so they undo like every other edit. The property is
    // the same one the engine reads, so nothing else changes.
    if (auto* audioTrack = session.impl->trackFor (track))
        audioTrack->state.setProperty (te::IDs::mute, muted, &session.impl->undoManager());
}

void EditOps::setTrackSolo (TrackRef track, bool soloed)
{
    if (auto* audioTrack = session.impl->trackFor (track))
        audioTrack->state.setProperty (te::IDs::solo, soloed, &session.impl->undoManager());
}

void EditOps::setSend (TrackRef track, TrackRef bus, double levelDb)
{
    auto* from = session.impl->trackFor (track);
    auto* to = session.impl->trackFor (bus);

    if (from == nullptr || to == nullptr || from == to)
        return;

    // The bus needs a return before anything can be sent to it, and a number
    // that no other bus is using: the engine routes sends by number, and Duet
    // routes them by track, so the number is made here and never surfaces.
    auto* auxReturn = returnOn (*to);

    if (auxReturn == nullptr)
    {
        const auto busNumber = unusedBusNumber (*session.impl->edit);

        if (busNumber < 0)
            return;

        auto created = session.impl->edit->getPluginCache().createNewPlugin (
            te::AuxReturnPlugin::xmlTypeName, {});
        auxReturn = dynamic_cast<te::AuxReturnPlugin*> (created.get());

        if (auxReturn == nullptr)
            return;

        auxReturn->busNumber = busNumber;
        to->pluginList.insertPlugin (created, 0, nullptr);
        stateParametersExplicitly (*auxReturn);
    }

    auto* send = sendOn (*from, auxReturn->busNumber);

    if (send == nullptr)
    {
        auto created = session.impl->edit->getPluginCache().createNewPlugin (
            te::AuxSendPlugin::xmlTypeName, {});
        send = dynamic_cast<te::AuxSendPlugin*> (created.get());

        if (send == nullptr)
            return;

        send->busNumber = auxReturn->busNumber.get();
        from->pluginList.insertPlugin (created, -1, nullptr);
        stateParametersExplicitly (*send);
    }

    send->setGainDb (static_cast<float> (levelDb));
}

//==============================================================================
PluginRef EditOps::addPlugin (TrackRef track, BuiltinPlugin plugin, int position)
{
    auto* audioTrack = session.impl->trackFor (track);

    if (audioTrack == nullptr)
        return noPlugin;

    auto created = session.impl->edit->getPluginCache().createNewPlugin (engineTypeOf (plugin), {});

    if (created == nullptr)
        return noPlugin;

    audioTrack->pluginList.insertPlugin (
        created, rawPositionFor (audioTrack->pluginList, position), nullptr);
    stateParametersExplicitly (*created);
    return toRef<PluginRef> (created->itemID);
}

void EditOps::removePlugin (PluginRef plugin)
{
    if (auto* p = session.impl->pluginFor (plugin))
        p->deleteFromParent();
}

void EditOps::reorderPlugin (PluginRef plugin, int newPosition)
{
    auto* p = session.impl->pluginFor (plugin);

    if (p == nullptr)
        return;

    auto* list = p->getOwnerList();

    if (list == nullptr)
        return;

    // Held while it is out of the chain: the list owns the only reference to it.
    const te::Plugin::Ptr held { p };

    held->removeFromParent();
    list->insertPlugin (held, rawPositionFor (*list, newPosition), nullptr);
}

void EditOps::setPluginParameter (PluginRef plugin, std::string_view parameterId, double value)
{
    if (auto* p = session.impl->pluginFor (plugin))
        if (auto parameter = p->getAutomatableParameterByID (toJuceString (parameterId)))
            parameter->setParameter (static_cast<float> (value), juce::sendNotification);
}

void EditOps::setPluginSidechainSource (PluginRef plugin, TrackRef source)
{
    auto* p = session.impl->pluginFor (plugin);

    if (p == nullptr)
        return;

    if (source == noTrack)
    {
        p->setSidechainSourceID ({});
        return;
    }

    auto* sourceTrack = session.impl->trackFor (source);

    if (sourceTrack == nullptr)
        return;

    p->setSidechainSourceID (sourceTrack->itemID);

    // The engine's own by-name setter wires the channels the first time a source
    // is chosen; without it the routing is named but carries no audio.
    if (p->getNumWires() == 0)
        p->guessSidechainRouting();
}

//==============================================================================
void EditOps::setAutomationPoints (const AutomationTarget& target,
                                   const std::vector<AutomationPoint>& points)
{
    auto* parameter = session.impl->parameterFor (target);

    if (parameter == nullptr)
        return;

    auto& curve = parameter->getCurve();
    const auto& tempoSequence = session.impl->edit->tempoSequence;

    for (const auto& point : points)
    {
        const auto value = toCurveValue (target, point.value);
        const auto existing = indexOfPointAt (curve, point.timeSeconds, tempoSequence);

        // A time the curve already has a point at takes the new value rather
        // than gaining a second point on top of the first.
        if (existing >= 0)
            curve.setPointValue (existing, value, &session.impl->undoManager());
        else
            curve.addPoint (te::EditPosition { te::TimePosition::fromSeconds (point.timeSeconds) },
                            value,
                            0.0F,
                            &session.impl->undoManager());
    }
}

void EditOps::removeAutomationPoints (const AutomationTarget& target,
                                      double fromSeconds,
                                      double toSeconds)
{
    auto* parameter = session.impl->parameterFor (target);

    if (parameter == nullptr)
        return;

    auto& curve = parameter->getCurve();
    const auto& tempoSequence = session.impl->edit->tempoSequence;

    // Both ends counted in, and not the engine's half-open range: a producer
    // clearing a stretch of a curve means the points at its edges too, and a
    // stretch with the same time at both ends means the one point there.
    const auto from = te::TimePosition::fromSeconds (fromSeconds);
    const auto to = te::TimePosition::fromSeconds (toSeconds);

    for (int point = curve.getNumPoints(); --point >= 0;)
    {
        const auto time = pointTime (curve, point, tempoSequence);

        if (time >= from && time <= to)
            curve.removePoint (point, &session.impl->undoManager());
    }
}

//==============================================================================
void EditOps::setTempo (double bpm)
{
    if (auto* tempo = session.impl->edit->tempoSequence.getTempo (0))
        tempo->setBpm (bpm);
}

void EditOps::setTimeSignature (int numerator, int denominator)
{
    if (auto* timeSig = session.impl->edit->tempoSequence.getTimeSig (0))
    {
        timeSig->numerator = numerator;
        timeSig->denominator = denominator;
    }
}
} // namespace duet::model
