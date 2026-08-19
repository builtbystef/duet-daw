#include "SessionImpl.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace duet::model
{
namespace
{
    constexpr double demoPhraseSeconds = 8.0;
    constexpr int demoNoteVelocity = 100;

    /** ADR 0004: undo goes 200 Actions deep, in memory, for the session. */
    constexpr int undoDepth = 200;
} // namespace

//==============================================================================
Session::Session (std::filesystem::path editFile)
    : impl (std::make_unique<Impl> (std::move (editFile)))
{
    impl->edit = te::createEmptyEdit (impl->engine, toJuceFile (impl->editFile));
    startUndoHistory();
}

Session::Session (std::filesystem::path editFile, FromFile /*readIt*/)
    : impl (std::make_unique<Impl> (std::move (editFile)))
{
    impl->edit = te::loadEditFromFile (impl->engine, toJuceFile (impl->editFile));

    if (impl->edit != nullptr)
        startUndoHistory();
}

std::unique_ptr<Session> Session::openExisting (std::filesystem::path editFile)
{
    if (! std::filesystem::exists (editFile))
        return nullptr;

    std::unique_ptr<Session> session { new Session { std::move (editFile), FromFile {} } };

    return session->impl->edit != nullptr ? std::move (session) : nullptr;
}

void Session::startUndoHistory()
{
    // JUCE's UndoManager drops the oldest transactions once the stored units
    // pass their budget, but never below the minimum number of transactions. A
    // budget of one unit is what turns "at least 200 Actions" into "the newest
    // 200 Actions", which is the depth ADR 0004 asks for.
    impl->undoManager().setMaxNumberOfStoredUnits (1, undoDepth);

    // The engine creates its scene list lazily, the first time a track is added,
    // and that node outlives the undo that removes the track again. Creating it
    // up front is what makes two states of the same project comparable.
    impl->edit->getSceneList();

    // Every plugin already in the project says what its parameters are, so that
    // an undo can put one back without having to create the property that holds
    // it. Nothing here is an edit; a project that has just been opened has
    // nothing to undo.
    for (auto* plugin : te::getAllPlugins (*impl->edit, true))
        stateParametersExplicitly (*plugin);

    // Opening a project is not an Action; the history starts clean.
    impl->undoManager().clearUndoHistory();
}

tracktion::engine::Edit& EngineAccess::editOf (Session& session) { return *session.impl->edit; }

Session::~Session()
{
    if (impl->edit != nullptr)
        impl->edit->getTransport().stop (false, true);
}

//==============================================================================
void Session::performAction (std::string_view name, const std::function<void (EditOps&)>& ops)
{
    if (! juce::MessageManager::existsAndIsCurrentThread())
        throw std::logic_error { "duet::model::Session: the message thread is the sole writer of "
                                 "the project model, and performAction ran on another thread" };

    // An Action that runs long enough for the engine's transaction timer to see
    // a quiet message loop would otherwise be split in two.
    const te::Edit::UndoTransactionInhibitor keepTheActionWhole { *impl->edit };

    impl->undoManager().beginNewTransaction (toJuceString (name));
    EditOps editOps { *this };
    ops (editOps);

    // No sealing call, deliberately: the engine's deferred undo-tracked writes
    // land in the transaction that is still open, which is the Action that
    // caused them. The next Action opens its own.

    // An Action may have moved the tempo map, and the loop is over the music
    // and not over a stretch of seconds.
    impl->applyLoopRange();
    impl->announceChange();
}

bool Session::undo()
{
    if (! impl->undoManager().canUndo())
        return false;

    impl->edit->undo();
    impl->refreshParametersFromState();
    impl->applyLoopRange();
    impl->announceChange();
    return true;
}

bool Session::redo()
{
    if (! impl->undoManager().canRedo())
        return false;

    impl->edit->redo();
    impl->refreshParametersFromState();
    impl->applyLoopRange();
    impl->announceChange();
    return true;
}

void Session::onProjectChanged (std::function<void()> callback)
{
    impl->projectChanged = std::move (callback);
}

std::vector<std::string> Session::undoNames() const
{
    return Impl::toStrings (impl->undoManager().getUndoDescriptions());
}

std::vector<std::string> Session::redoNames() const
{
    return Impl::toStrings (impl->undoManager().getRedoDescriptions());
}

//==============================================================================
namespace
{
    /** Which track is the bus a send's number leads to. */
    TrackRef busTrackFor (te::Edit& edit, int busNumber)
    {
        for (auto* track : te::getAudioTracks (edit))
            if (auto* auxReturn = returnOn (*track))
                if (auxReturn->busNumber == busNumber)
                    return toRef<TrackRef> (track->itemID);

        return noTrack;
    }

    PluginInfo describe (te::Plugin& plugin)
    {
        PluginInfo info;
        info.plugin = toRef<PluginRef> (plugin.itemID);
        info.name = plugin.getName().toStdString();
        info.builtin = builtinOf (plugin);
        info.sidechainSource = toRef<TrackRef> (plugin.getSidechainSourceID());
        return info;
    }

    ClipInfo describe (te::Clip& clip)
    {
        ClipInfo info;
        info.clip = toRef<ClipRef> (clip.itemID);
        info.name = clip.getName().toStdString();
        info.startSeconds = clip.getPosition().getStart().inSeconds();
        info.lengthSeconds = clip.getPosition().getLength().inSeconds();
        info.looped = clip.isLooping();
        info.loopLengthBeats = info.looped ? clip.getLoopLengthBeats().inBeats() : 0.0;
        info.holdsMidi = dynamic_cast<te::MidiClip*> (&clip) != nullptr;

        if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (&clip))
        {
            info.sourceReference = audioClip->getSourceFileReference().source.get().toStdString();
            info.sourceFile = toPath (audioClip->getSourceFileReference().getFile());
        }

        return info;
    }
} // namespace

std::vector<TrackInfo> Session::tracks() const
{
    std::vector<TrackInfo> out;

    for (auto* track : te::getAudioTracks (*impl->edit))
    {
        TrackInfo trackInfo;
        trackInfo.track = toRef<TrackRef> (track->itemID);
        trackInfo.name = track->getName().toStdString();
        trackInfo.kind = trackKindOf (*track);
        trackInfo.muted = track->isMuted (false);
        trackInfo.soloed = track->isSolo (false);

        if (auto* destination = track->getOutput().getDestinationTrack())
            trackInfo.output = toRef<TrackRef> (destination->itemID);

        if (auto* parameter =
                impl->parameterFor (AutomationTarget::trackVolumeOf (trackInfo.track)))
            trackInfo.volumeDb = te::volumeFaderPositionToDB (parameter->getCurrentExplicitValue());

        if (auto* parameter = impl->parameterFor (AutomationTarget::trackPanOf (trackInfo.track)))
            trackInfo.pan = parameter->getCurrentExplicitValue();

        for (auto* plugin : track->pluginList.getPlugins())
        {
            trackInfo.plugins.push_back (describe (*plugin));

            if (auto* send = dynamic_cast<te::AuxSendPlugin*> (plugin))
                if (const auto bus = busTrackFor (*impl->edit, send->busNumber); bus != noTrack)
                    trackInfo.sends.push_back ({ bus, send->getGainDb() });
        }

        for (auto* clip : track->getClips())
            trackInfo.clips.push_back (describe (*clip));

        out.push_back (std::move (trackInfo));
    }

    return out;
}

TrackInfo Session::track (TrackRef ref) const
{
    for (auto& trackInfo : tracks())
        if (trackInfo.track == ref)
            return std::move (trackInfo);

    return {};
}

int Session::audioTrackCount() const { return te::getAudioTracks (*impl->edit).size(); }

std::vector<NoteInfo> Session::notes (ClipRef clip) const
{
    std::vector<NoteInfo> out;

    auto* midiClip = impl->midiClipFor (clip);

    if (midiClip == nullptr)
        return out;

    for (auto* note : midiClip->getSequence().getNotes())
        out.push_back ({ impl->refForNote (clip, note->state),
                         note->getNoteNumber(),
                         note->getStartBeat().inBeats(),
                         note->getLengthBeats().inBeats(),
                         note->getVelocity() });

    std::stable_sort (out.begin(),
                      out.end(),
                      [] (const NoteInfo& first, const NoteInfo& second)
                      { return first.startBeats < second.startBeats; });

    return out;
}

std::vector<PluginParameterInfo> Session::pluginParameters (PluginRef plugin) const
{
    std::vector<PluginParameterInfo> out;

    auto* p = impl->pluginFor (plugin);

    if (p == nullptr)
        return out;

    for (auto* parameter : p->getAutomatableParameters())
        out.push_back ({ parameter->paramID.toStdString(),
                         parameter->getParameterName().toStdString(),
                         parameter->getCurrentExplicitValue(),
                         parameter->getValueRange().getStart(),
                         parameter->getValueRange().getEnd() });

    return out;
}

std::vector<AutomationPoint> Session::automationPoints (const AutomationTarget& target) const
{
    std::vector<AutomationPoint> out;

    auto* parameter = impl->parameterFor (target);

    if (parameter == nullptr)
        return out;

    const auto& curve = parameter->getCurve();
    const auto& tempoSequence = impl->edit->tempoSequence;

    for (int point = 0; point < curve.getNumPoints(); ++point)
    {
        const auto value = curve.getPointValue (point);

        out.push_back ({ te::toTime (curve.getPointPosition (point), tempoSequence).inSeconds(),
                         target.kind == AutomationTarget::Kind::trackVolume
                             ? te::volumeFaderPositionToDB (value)
                             : value });
    }

    return out;
}

double Session::tempoBpm() const { return impl->edit->tempoSequence.getBpmAt (te::TimePosition()); }

TimeSignature Session::timeSignature() const
{
    if (auto* timeSig = impl->edit->tempoSequence.getTimeSig (0))
        return { timeSig->numerator, timeSig->denominator };

    return {};
}

double Session::editLengthSeconds() const { return impl->edit->getLength().inSeconds(); }

double Session::barStartSeconds (int bar) const
{
    // Bars count from one for the producer, and from zero for the engine, which
    // counts whole bars elapsed.
    return impl->edit->tempoSequence
        .toTime (tracktion::tempo::BarsAndBeats { std::max (0, bar - 1) })
        .inSeconds();
}

double Session::liveTrackVolumeDb (TrackRef track) const
{
    if (auto* fader = impl->faderFor (track))
        return fader->getVolumeDb();

    return 0.0;
}

//==============================================================================
namespace
{
    /** True when a node says something about the project: it carries a property,
        or a descendant does.

        The engine builds an edit out of scaffolding nodes that are empty until
        something needs them, and fills in more of them when it reads the edit
        back off disk. An empty node states nothing, so counting one is how a
        comparison reports a difference between a project and itself.
    */
    bool saysSomething (const juce::ValueTree& node)
    {
        std::vector<juce::ValueTree> remaining { node };

        while (! remaining.empty())
        {
            const auto next = remaining.back();
            remaining.pop_back();

            if (next.getNumProperties() > 0)
                return true;

            for (const auto& child : next)
                remaining.push_back (child);
        }

        return false;
    }

    /** Writes a tree as what it states, rather than as it happens to be stored.

        Properties go in name order, empty nodes go away, and children go in type
        order — stably, so that the run of children of one type keeps the order it
        is in. A track list and a plugin chain both mean their order; which of a
        track's differently-shaped children comes first means nothing, and the
        engine writes those in one order and reads them back in another.
    */
    void appendCanonicalised (const juce::ValueTree& tree, std::string& out)
    {
        struct Pending
        {
            juce::ValueTree node;
            int depth;
        };

        std::vector<Pending> remaining { { tree, 0 } };

        while (! remaining.empty())
        {
            const auto [node, depth] = remaining.back();
            remaining.pop_back();

            const auto indent = static_cast<std::size_t> (depth) * 2;

            out.append (indent, ' ');
            out += node.getType().toString().toStdString();
            out += '\n';

            std::vector<std::string> properties;
            properties.reserve (static_cast<std::size_t> (node.getNumProperties()));

            for (int i = 0; i < node.getNumProperties(); ++i)
            {
                const auto name = node.getPropertyName (i);
                properties.push_back (name.toString().toStdString() + "="
                                      + node.getProperty (name).toString().toStdString());
            }

            std::sort (properties.begin(), properties.end());

            for (const auto& property : properties)
            {
                out.append (indent + 2, ' ');
                out += property;
                out += '\n';
            }

            std::vector<juce::ValueTree> children;

            for (const auto& child : node)
                if (saysSomething (child))
                    children.push_back (child);

            std::stable_sort (children.begin(),
                              children.end(),
                              [] (const juce::ValueTree& first, const juce::ValueTree& second)
                              { return first.getType().toString() < second.getType().toString(); });

            // This stack runs backwards, so the children go on to it that way round.
            std::reverse (children.begin(), children.end());

            for (const auto& child : children)
                remaining.push_back ({ child, depth + 1 });
        }
    }
} // namespace

std::string Session::stateDigest() const
{
    // Not a flush: Edit::flushState() writes the plugins' parameter blobs
    // through the undo history, so asking what the state is would change it
    // (hazard 3). Every vocabulary operation writes through to the tree anyway.
    auto state = impl->edit->state.createCopy();

    // What moves without an edit having happened. The project ID is the engine's
    // note of which ProjectManager item an edit belongs to: it stamps a fresh
    // one on an edit it creates and drops it again when it reads one back, and
    // Duet keeps no ProjectManager for it to mean anything to.
    state.removeChild (state.getChildWithName (te::IDs::TRANSPORT), nullptr);
    state.removeProperty ("lastSignificantChange", nullptr);
    state.removeProperty ("modifiedBy", nullptr);
    state.removeProperty (te::IDs::projectID, nullptr);

    std::string canonical;
    appendCanonicalised (state, canonical);

    std::ostringstream digest;
    digest << std::hex << std::hash<std::string> {}(canonical) << "/" << std::dec
           << canonical.size() << "B";

    return std::move (digest).str();
}

bool Session::renderToFile (const std::filesystem::path& destination)
{
    const auto file = toJuceFile (destination);
    file.deleteFile();

    // Rendered on this thread: a headless test has no message loop to wait on,
    // and the engine's threaded path reports progress through a UI it has none of.
    return te::Renderer::renderToFile (*impl->edit, file, false);
}

//==============================================================================
void Session::loadDemoContent()
{
    const auto tracks = te::getAudioTracks (*impl->edit);

    if (tracks.isEmpty())
        return;

    const auto track = toRef<TrackRef> (tracks.getFirst()->itemID);

    performAction (
        "Add the demo phrase",
        [track] (auto& ops)
        {
            ops.renameTrack (track, "Demo");
            ops.addPlugin (track, BuiltinPlugin::synth, 0);

            const auto clip = ops.insertMidiClip (track, "Demo", 0.0, demoPhraseSeconds);

            // An A-minor arpeggio, two notes per beat, so that what comes out of
            // the speakers is unmistakably the app's own audio and not a click.
            static constexpr std::array<int, 8> pitches { 57, 60, 64, 69, 72, 69, 64, 60 };

            for (int note = 0; note < 32; ++note)
                ops.addNote (clip,
                             pitches.at (static_cast<std::size_t> (note % 8)),
                             note * 0.5,
                             0.45,
                             demoNoteVelocity);
        });

    // The phrase is where the project starts, not an edit made to it, so the
    // history begins here. Otherwise one undo too many empties the project and
    // leaves the producer with nothing to play — an undo of something they
    // never did.
    startUndoHistory();

    // The transport, not the project: written with no undo history at all.
    setLoopRangeSeconds (0.0, demoPhraseSeconds);
    setLooping (true);
}

//==============================================================================
void Session::startPlayback()
{
    auto& transport = impl->edit->getTransport();
    transport.ensureContextAllocated();
    transport.play (false);
}

void Session::stopPlayback() { impl->edit->getTransport().stop (false, false); }

bool Session::isPlaying() const { return impl->edit->getTransport().isPlaying(); }

double Session::playbackPositionSeconds() const
{
    return impl->edit->getTransport().getPosition().inSeconds();
}

void Session::setPlaybackPositionSeconds (double seconds)
{
    impl->edit->getTransport().setPosition (te::TimePosition::fromSeconds (seconds));
}

void Session::Impl::applyLoopRange() const
{
    if (! loopBeats.has_value())
        return;

    const auto& tempoSequence = edit->tempoSequence;

    const auto toTime = [&tempoSequence] (double beats)
    { return tempoSequence.toTime (te::BeatPosition::fromBeats (beats)); };

    edit->getTransport().setLoopRange ({ toTime (loopBeats->first), toTime (loopBeats->second) });
}

void Session::setLoopRangeSeconds (double startSeconds, double endSeconds)
{
    const auto& tempoSequence = impl->edit->tempoSequence;

    const auto toBeats = [&tempoSequence] (double seconds)
    { return tempoSequence.toBeats (te::TimePosition::fromSeconds (seconds)).inBeats(); };

    impl->loopBeats = std::pair { toBeats (startSeconds), toBeats (endSeconds) };
    impl->applyLoopRange();
}

LoopRange Session::loopRangeSeconds() const
{
    const auto range = impl->edit->getTransport().getLoopRange();
    return { range.getStart().inSeconds(), range.getEnd().inSeconds() };
}

void Session::setLooping (bool shouldLoop) { impl->edit->getTransport().looping = shouldLoop; }

bool Session::isLooping() const { return impl->edit->getTransport().looping; }

std::string Session::audioDeviceDescription() const
{
    auto* device = impl->engine.getDeviceManager().deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return {};

    const double sampleRate = device->getCurrentSampleRate();
    const int blockSize = device->getCurrentBufferSizeSamples();
    const double latencyMs =
        sampleRate > 0.0 ? 1000.0 * device->getOutputLatencyInSamples() / sampleRate : 0.0;

    std::ostringstream description;
    description.precision (1);
    description << device->getTypeName() << ": " << device->getName() << ", " << std::fixed
                << sampleRate << " Hz, " << blockSize << " samples, " << latencyMs << " ms out";

    return std::move (description).str();
}
} // namespace duet::model
