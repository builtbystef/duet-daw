#include "SessionImpl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace duet::model
{
namespace
{
    constexpr double demoPhraseSeconds = 8.0;
    constexpr int demoNoteVelocity = 100;

    /** ADR 0004: undo goes 200 Actions deep, in memory, for the session. */
    constexpr int undoDepth = 200;

    /** How often the model asks a transport that is not rolling to play, and
        how many of those asks it makes before it accepts the answer.

        Hazard 6 costs one ask, a few seconds into the first playback of a
        session; ten seconds of asking covers that with room to spare. The
        asking ends because a machine with no working output would otherwise be
        asked forever, and every ask allocates a playback context.
    */
    constexpr int playRetryIntervalMs = 100;
    constexpr int playRetryAttempts = 100;

    /** What playing with no audio device runs at. The rate and the block size
        are ordinary ones and nothing depends on them: they only decide how many
        blocks a stretch of seconds is cut into. The CPU limit is what stops the
        engine muting blocks that arrive faster than real time, which every one
        of them does here.
    */
    constexpr double measuringSampleRate = 44100.0;
    constexpr int measuringBlockSize = 512;
    constexpr double measuringCpuLimit = 1000.0;

    /** How many channels go in and come out with no audio device. A stereo pair
        each way: the inputs are there so that a take can be recorded, and one
        stereo input is what an ordinary interface offers a producer.
    */
    constexpr int measuringChannels = 2;
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

bool Session::startPluginScanChild (std::string_view commandLine)
{
    // Do not initialise JUCE for an ordinary process. The scan UID is private to
    // Tracktion's coordinator and is the cheap answer to whether this process
    // was launched as its worker.
    if (commandLine.find ("PluginScan") == std::string_view::npos)
        return false;

    // A JUCEApplication already owns this initialisation. The headless Catch
    // executable does not, and it is also a valid scan worker, so keep one alive
    // for the worker process's whole lifetime.
    static const auto scanChildJuce = std::make_unique<juce::ScopedJuceInitialiser_GUI>();

    return te::PluginManager::startChildProcessPluginScan (toJuceString (commandLine));
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

    // External parameters are stored by Duet because the engine otherwise
    // states them only during a flush. On open, those stored explicit values
    // are the ones the VST3 instance must start at.
    impl->refreshParametersFromState();

    // Opening a project is not an Action; the history starts clean.
    impl->undoManager().clearUndoHistory();
}

tracktion::engine::Edit& EngineAccess::editOf (Session& session) { return *session.impl->edit; }

Session::~Session()
{
    impl->playbackKeeper.stopTimer();
    impl->takeStarter.stopTimer();

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

    // The project's undo history directly, and not Edit::undo(): the engine's
    // own undo stops a running recording before it reverts anything, and an
    // undo that could end a take is exactly what the transport being written
    // with no undo history is there to prevent (ADR 0004). Nothing else that
    // Edit::undo() does applies here — the rest of it refreshes the engine's
    // SelectionManagers, and Duet registers none.
    impl->undoManager().undo();

    // An undo can remove a sounding MIDI note and the note-off that would have
    // ended it. The replacement graph can leave the plugin voice running even
    // though the project no longer contains its note, so end every MIDI voice
    // before another block can play.
    if (impl->edit->getTransport().isPlaying())
        te::midiPanic (*impl->edit, false);

    impl->refreshParametersFromState();
    impl->applyLoopRange();
    impl->announceChange();
    return true;
}

bool Session::redo()
{
    if (! impl->undoManager().canRedo())
        return false;

    // The project's undo history directly, for the reason undo() gives.
    impl->undoManager().redo();

    // Redo can remove a sounding note just as undo can, so it has the same
    // obligation to silence a plugin voice whose note-off no longer exists.
    if (impl->edit->getTransport().isPlaying())
        te::midiPanic (*impl->edit, false);

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

        if (auto* external = dynamic_cast<te::ExternalPlugin*> (&plugin))
        {
            info.externalIdentifier =
                plugin.state[juce::Identifier { externalPluginIdentifierProperty }]
                    .toString()
                    .toStdString();

            if (info.externalIdentifier.empty())
                info.externalIdentifier = external->desc.createIdentifierString().toStdString();

            info.missing = external->isMissing();
        }

        return info;
    }

    ClipInfo describe (te::Clip& clip)
    {
        ClipInfo info;
        info.clip = toRef<ClipRef> (clip.itemID);
        info.name = clip.getName().toStdString();
        info.startSeconds = clip.getPosition().getStart().inSeconds();
        info.lengthSeconds = clip.getPosition().getLength().inSeconds();
        info.contentOffsetSeconds = clip.getPosition().getOffset().inSeconds();
        const juce::Identifier colourProperty { "duetClipColour" };
        if (clip.state.hasProperty (colourProperty))
            info.colour = static_cast<TrackColour> (
                juce::jlimit (0, 7, static_cast<int> (clip.state[colourProperty])));
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
        trackInfo.colour = static_cast<TrackColour> (
            juce::jlimit (0,
                          7,
                          static_cast<int> (track->state.getProperty (
                              juce::Identifier { "duetTrackColour" }, 0))));

        if (const auto destination = impl->destinationStateFor (trackInfo.track);
            destination.isValid())
        {
            trackInfo.input = impl->inputOfDestination (destination);
            trackInfo.recordArmed = destination[te::IDs::armed];
        }

        if (auto* destination = track->getOutput().getDestinationTrack())
            trackInfo.output = toRef<TrackRef> (destination->itemID);

        if (auto* parameter =
                impl->parameterFor (AutomationTarget::trackVolumeOf (trackInfo.track)))
            trackInfo.volumeDb = te::volumeFaderPositionToDB (parameter->getCurrentExplicitValue());

        if (auto* parameter = impl->parameterFor (AutomationTarget::trackPanOf (trackInfo.track)))
            trackInfo.pan = parameter->getCurrentExplicitValue();

        for (auto* plugin : track->pluginList.getPlugins())
        {
            if (isProducersPlugin (*plugin))
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
        out.push_back (
            { parameter->paramID.toStdString(),
              parameter->getParameterName().toStdString(),
              parameter->getCurrentExplicitValue(),
              parameter->getValueRange().getStart(),
              parameter->getValueRange().getEnd(),
              parameter->valueToString (parameter->getCurrentExplicitValue()).toStdString() });

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
bool Session::canHostVst3() const
{
    auto& formats = impl->engine.getPluginManager().pluginFormatManager;

    for (int index = 0; index < formats.getNumFormats(); ++index)
        if (formats.getFormat (index)->getName() == "VST3")
            return true;

    return false;
}

bool Session::scansPluginsOutOfProcess() const
{
    return impl->engine.getPluginManager().usesSeparateProcessForScanning();
}

PluginScanResult Session::scanVst3Plugins (const std::filesystem::path& directory)
{
    PluginScanResult result;

    if (! std::filesystem::is_directory (directory))
        return result;

    auto& manager = impl->engine.getPluginManager();
    juce::AudioPluginFormat* vst3 = nullptr;

    for (int index = 0; index < manager.pluginFormatManager.getNumFormats(); ++index)
        if (auto* format = manager.pluginFormatManager.getFormat (index);
            format->getName() == "VST3")
        {
            vst3 = format;
            break;
        }

    if (vst3 == nullptr)
        return result;

    {
        const auto deadMansPedal =
            impl->engine.getPropertyStorage().getAppPrefsFolder().getChildFile (
                "PluginScanDeadMansPedal.txt");
        juce::PluginDirectoryScanner scanner { manager.knownPluginList,
                                               *vst3,
                                               juce::FileSearchPath {
                                                   toJuceFile (directory).getFullPathName() },
                                               true,
                                               deadMansPedal };
        juce::String scanning;

        while (scanner.scanNextFile (true, scanning))
        {
        }

        for (const auto& failed : scanner.getFailedFiles())
            result.failedFiles.push_back (toPath (juce::File { failed }));
    }

    const auto normalDirectory = directory.lexically_normal();

    for (const auto& bad : manager.knownPluginList.getBlacklistedFiles())
    {
        const auto path = toPath (juce::File { bad }).lexically_normal();
        const auto relative = path.lexically_relative (normalDirectory);

        if (! relative.empty() && *relative.begin() != "..")
            result.badFiles.push_back (path);
    }

    // The manager normally persists this through its asynchronous change
    // listener. A scan is synchronous, so put the completed list on disk before
    // returning: a restart immediately after Scan must not scan known-good
    // plugins again.
    if (const auto xml = manager.knownPluginList.createXml())
    {
#if JUCE_64BIT
        constexpr auto knownPluginsSetting = te::SettingID::knownPluginList64;
#else
        constexpr auto knownPluginsSetting = te::SettingID::knownPluginList;
#endif

        auto& storage = impl->engine.getPropertyStorage();
        storage.setXmlProperty (knownPluginsSetting, *xml);
        storage.getPropertiesFile().saveIfNeeded();
    }

    result.completed = true;
    return result;
}

std::vector<KnownPluginInfo> Session::knownVst3Plugins() const
{
    std::vector<KnownPluginInfo> known;

    for (const auto& description : impl->engine.getPluginManager().knownPluginList.getTypes())
        if (description.pluginFormatName == "VST3")
            known.push_back ({ description.createIdentifierString().toStdString(),
                               description.name.toStdString(),
                               description.manufacturerName.toStdString(),
                               toPath (juce::File { description.fileOrIdentifier }),
                               description.isInstrument,
                               juce::File { description.fileOrIdentifier }.exists() });

    return known;
}

//==============================================================================
double Session::outputPeakDb() { return impl->outputMeter.readPeakDb(); }

double Session::trackPeakDb (TrackRef track)
{
    const auto meter = impl->trackMeters.find (track);

    return meter != impl->trackMeters.end() ? meter->second->readPeakDb() : silentDb;
}

void Session::Impl::useHostedAudioDevice() const
{
    auto& deviceManager = engine.getDeviceManager();

    if (deviceManager.isHostedAudioDeviceInterfaceInUse())
        return;

    // Blocks go through as fast as the machine will take them, so the engine's
    // wall-clock measure of how hard it is working means nothing here; left
    // alone it would mute the blocks it thinks arrived late.
    deviceManager.setCpuLimitBeforeMuting (measuringCpuLimit);

    te::HostedAudioDeviceInterface::Parameters parameters;
    parameters.sampleRate = measuringSampleRate;
    parameters.blockSize = measuringBlockSize;
    parameters.inputChannels = measuringChannels;
    parameters.outputChannels = measuringChannels;

    auto& audioInterface = deviceManager.getHostedAudioDeviceInterface();
    audioInterface.initialise (parameters);
    audioInterface.prepareToPlay (parameters.sampleRate, parameters.blockSize);
    deviceManager.dispatchPendingUpdates();

    // Initialising the hosted device applies its MIDI list synchronously, but
    // settling the defaults schedules one more apply on the device manager's
    // timer. That apply frees every playback graph: a headless take can push
    // its blocks before the message loop delivers it, then be ended by the
    // delivery. Hosted MIDI cannot change underneath us, so cancel that pending
    // scan. As suppressDeviceRebuild does, restore the production setting in
    // PropertyStorage without restarting this session's timer.
    deviceManager.setMidiDeviceScanIntervalSeconds (0);
    engine.getPropertyStorage().setProperty (te::SettingID::midiScanIntervalSeconds, 4);
}

void Session::Impl::pushBlocks (double seconds, const InputSignal& playedIn) const
{
    auto& audioInterface = engine.getDeviceManager().getHostedAudioDeviceInterface();

    const auto toSample = [] (double atSeconds)
    { return static_cast<int> (std::llround (std::max (0.0, atSeconds) * measuringSampleRate)); };

    // The notes as the wire carries them: one message on, one message off.
    std::vector<std::pair<int, juce::MidiMessage>> messages;

    for (const auto& note : playedIn.notes)
    {
        const auto velocity = static_cast<juce::uint8> (std::clamp (note.velocity, 0, 127));

        messages.emplace_back (toSample (note.atSeconds),
                               juce::MidiMessage::noteOn (1, note.pitch, velocity));
        messages.emplace_back (toSample (note.atSeconds + note.lengthSeconds),
                               juce::MidiMessage::noteOff (1, note.pitch));
    }

    std::stable_sort (messages.begin(),
                      messages.end(),
                      [] (const auto& first, const auto& second)
                      { return first.first < second.first; });

    juce::AudioBuffer<float> block { measuringChannels, measuringBlockSize };

    const auto blocks = static_cast<int> (
        std::ceil (std::max (0.0, seconds) * measuringSampleRate / measuringBlockSize));

    const auto radiansPerSample =
        2.0 * juce::MathConstants<double>::pi * playedIn.toneFrequencyHz / measuringSampleRate;

    std::size_t nextMessage = 0;

    for (int played = 0; played < blocks; ++played)
    {
        const auto blockStart = played * measuringBlockSize;

        block.clear();

        if (playedIn.toneFrequencyHz > 0.0)
            for (int sample = 0; sample < measuringBlockSize; ++sample)
            {
                const auto value = static_cast<float> (
                    playedIn.toneLevel * std::sin (radiansPerSample * (blockStart + sample)));

                for (int channel = 0; channel < measuringChannels; ++channel)
                    block.setSample (channel, sample, value);
            }

        juce::MidiBuffer midiIn;

        for (; nextMessage < messages.size(); ++nextMessage)
        {
            const auto& [at, message] = messages[nextMessage];

            if (at >= blockStart + measuringBlockSize)
                break;

            midiIn.addEvent (message, std::max (0, at - blockStart));
        }

        audioInterface.processBlock (block, midiIn);
    }
}

void Session::useNoAudioDevice() { impl->useHostedAudioDevice(); }

void Session::suppressDeviceRebuild()
{
    auto& devices = impl->engine.getDeviceManager();
    auto& storage = impl->engine.getPropertyStorage();

    // The setter writes the interval into PropertyStorage, which lives in a
    // Settings.xml the next Engine (and the app) would inherit. Stop this
    // session's timer, then put the production default back so nobody else
    // sees the zero.
    devices.setMidiDeviceScanIntervalSeconds (0);
    storage.setProperty (te::SettingID::midiScanIntervalSeconds, 4);
}

void Session::rebuildDevices()
{
    impl->askForTheDeviceList();

    // rescanMidiDeviceList applies on a 5 ms timer; checkDefaultDevicesAreValid
    // then settles the defaults and applies again ~5 ms later. Both have to
    // land before this returns, or a caller that asked for the rebuild to be
    // over would still be waiting on the engine.
    constexpr int deviceApplyMs = 20;
    juce::MessageManager::getInstance()->runDispatchLoopUntil (deviceApplyMs);

    // A second ask does not change the list, so the engine does not free the
    // graphs. The first one does. This is that effect, on command: the
    // playback context is the graph, and without it the transport is stopped
    // the way hazard 6 stops it.
    impl->edit->getTransport().freePlaybackContext();
}

void Session::setDeviceWait (int quietMilliseconds, int pollMilliseconds, int attempts)
{
    impl->deviceQuietMs = static_cast<std::uint32_t> (std::max (0, quietMilliseconds));
    impl->devicePollMs = std::max (1, pollMilliseconds);
    impl->deviceWaitAttempts = std::max (0, attempts);
}

void Session::runWithoutAudioDevice (double seconds, const InputSignal& playedIn)
{
    impl->useHostedAudioDevice();

    // Every edit made since the last block has to be in the graph before the
    // next one is asked for: nothing pumps the message loop while the blocks go
    // through, and an edit still waiting to land would be inaudible.
    impl->edit->dispatchPendingUpdatesSynchronously();

    impl->pushBlocks (seconds, playedIn);
}

bool Session::playWithoutAudioDevice (double seconds)
{
    impl->useHostedAudioDevice();
    impl->edit->dispatchPendingUpdatesSynchronously();

    startPlayback();

    if (! isPlaying())
        return false;

    impl->pushBlocks (seconds, {});
    stopPlayback();

    return true;
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
void Session::Impl::askTransportToPlay()
{
    auto& transport = edit->getTransport();
    transport.ensureContextAllocated();
    transport.play (false);

    // The context that has just been allocated owns the master's measurer, and
    // a measurer with nothing listening to it measures nothing — so the meters
    // are pointed at it before any audio can reach it.
    syncMeters();
}

void Session::Impl::syncMeters()
{
    auto* context = edit->getCurrentPlaybackContext();
    outputMeter.attachTo (context != nullptr ? &context->masterLevels : nullptr);

    std::unordered_set<TrackRef> present;

    for (auto* track : te::getAudioTracks (*edit))
    {
        const auto ref = toRef<TrackRef> (track->itemID);
        present.insert (ref);

        auto& meter = trackMeters[ref];

        if (meter == nullptr)
            meter = std::make_unique<Meter>();

        auto* levelMeter = track->getLevelMeterPlugin();
        meter->attachTo (levelMeter != nullptr ? &levelMeter->measurer : nullptr);
    }

    std::erase_if (trackMeters,
                   [&present] (const auto& entry) { return ! present.contains (entry.first); });
}

void Session::Impl::keepPlaybackRolling()
{
    if (edit->getTransport().isPlaying())
    {
        askedWithoutRolling = 0;

        // A graph rebuilt under a rolling transport — an edit landing, or the
        // device rebuild of hazard 6 — leaves the meters reading a measurer
        // that has gone.
        syncMeters();
        return;
    }

    if (++askedWithoutRolling > playRetryAttempts)
    {
        playbackKeeper.stopTimer();
        return;
    }

    askTransportToPlay();
}

void Session::startPlayback()
{
    // A take still waiting for the engine's devices is not what was asked for
    // any more: this is a Play.
    impl->takeStarter.stopTimer();

    // One ask is not enough. The engine rebuilds its device list once, a few
    // seconds into the first playback of a session, and the rebuild frees the
    // playback graph and stops the transport with it (hazard 6) — so the model
    // remembers that playback was asked for, and keeps asking for as long as
    // that is true and the transport is not rolling. It lives here and not in
    // the app because the model is where the engine's quirks are absorbed, and
    // every caller of startPlayback has this problem.
    impl->askedWithoutRolling = 0;
    impl->askTransportToPlay();
    impl->playbackKeeper.startTimer (playRetryIntervalMs);
}

void Session::stopPlayback()
{
    // Stopping is the last word, and it reaches a take that has not begun: both
    // ways into this — a Stop, and stopRecording finding no take rolling — pass
    // through here, so cancelling the pre-roll once covers both.
    impl->takeStarter.stopTimer();

    // A take is stopped the way a take has to be stopped, whoever asked: the
    // clips it made are an Action, and the engine writes them as the transport
    // stops.
    if (isRecording())
    {
        stopRecording();
        return;
    }

    // Stopping is the last word: nothing asks for playback again until the
    // producer does.
    impl->playbackKeeper.stopTimer();
    impl->edit->getTransport().stop (false, false);
}

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
