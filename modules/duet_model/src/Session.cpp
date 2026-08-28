#include "SessionImpl.h"

#include <duet/model/PluginEditorAccess.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string_view>
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

    /** How long the engine has to say nothing about its devices before a
        commanded rebuild counts as over, and how long the model waits for that
        at most.

        The engine applies a rescan on a 5 ms timer and schedules the apply that
        settles its defaults about five milliseconds behind it, so fifty is ten
        times the gap a machine too busy to deliver a tick on time has to make
        up — and an apply that does land pushes the window out again, which is
        what makes the wait a count of the engine's ticks rather than a stretch
        of wall clock. The bound is under the engine's own four-second rebuild
        timer, so a rebuild that never lands cannot be answered by that timer
        instead of by what was asked for.
    */
    constexpr std::uint32_t deviceApplyQuietMs = 50;
    constexpr int deviceRebuildBoundMs = 2000;

    /** How much message loop one look of such a wait is worth: short enough
        that an answer is seen as it arrives, long enough that a timer on a
        millisecond gets its tick.
    */
    constexpr int msPerLook = 5;

    /** Runs the message loop in short looks until the condition holds, or until
        the bound runs out.
    */
    template <typename Condition>
    void runTheMessageLoopUntil (const Condition& condition, int boundMs)
    {
        const auto startedAt = juce::Time::getMillisecondCounter();
        const auto bound = static_cast<juce::uint32> (std::max (0, boundMs));

        while (! condition())
        {
            // Unsigned throughout, so the counter's wrap is a difference like
            // any other rather than a wait that never ends.
            if (juce::Time::getMillisecondCounter() - startedAt >= bound)
                return;

            juce::MessageManager::getInstance()->runDispatchLoopUntil (msPerLook);
        }
    }

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

    /** The shape every offline render has, whatever machine it runs on.

        ADR 0006 asserts a render's measured features, and a feature is only
        measurable against a known shape: the rate the samples are in, the block
        the engine cuts the timeline into — which is also how early a note may
        start, and which the public header states because what measures a render
        needs it — and a bit depth deep enough that nothing is rounded on the
        way out. Dithering is the noise a shallower depth would need, and it is
        off because it is noise and because it would make a render differ from
        itself.
    */
    constexpr int renderBitDepth = 32;

    constexpr bool withMasterPlugins = true;
    constexpr bool withoutMasterPlugins = false;

    constexpr bool ignoringMuteAndSolo = true;
    constexpr bool honouringMuteAndSolo = false;

    /** Whether a render stops every transport the engine is running, or only
        takes the Edit it renders out of playback.

        The engine's own render stops them all, and a render of the project the
        producer is playing is a render of the Edit that transport belongs to
        anyway. A render of a detached copy is not: the copy has no transport,
        and the project's own must go on rolling.
    */
    constexpr bool stoppingEveryTransport = true;
    constexpr bool leavingOtherTransportsRolling = false;

    /** What the engine's own render puts up around one, and takes down after.

        A render frees the playback context and puts it back, leaves clip slots
        out of it, and starts from plugins that are not half-way through a
        playback.

        One more is optional, and it is the one that decides what mute and solo
        mean for a render. The engine's solo isolator solo-isolates every track
        it is given and unmutes it as well, so a render under it holds each of
        those tracks whatever the project says about them. That is what a render
        of one track wants: the track asked for, and nothing another track's
        solo has to say about it. A render of the whole project wants the
        opposite — mute and solo are the project, and the file is what the
        producer hears (sh2dkg).

        Both ends are the message thread's work — the engine asserts as much,
        and freeing a playback context from anywhere else is why — so both ends
        are asked of it, and the caller may be a worker thread. Down again
        whatever happens on the way out: a guard left standing leaves the edit
        believing it is still rendering, and it never plays again.
    */
    class RenderGuards
    {
    public:
        RenderGuards (te::Edit& e,
                      te::Track::Array& tracks,
                      bool ignoreMuteAndSolo,
                      bool stopEveryTransport)
            : edit (e)
        {
            te::callBlockingCatching (
                [&] {
                    guards = std::make_unique<Guards> (
                        edit, tracks, ignoreMuteAndSolo, stopEveryTransport);
                });
        }

        ~RenderGuards()
        {
            te::callBlockingCatching (
                [this]
                {
                    te::Renderer::turnOffAllPlugins (edit);
                    guards.reset();
                });
        }

        RenderGuards (const RenderGuards&) = delete;
        RenderGuards (RenderGuards&&) = delete;
        RenderGuards& operator= (const RenderGuards&) = delete;
        RenderGuards& operator= (RenderGuards&&) = delete;

        [[nodiscard]] bool areUp() const { return guards != nullptr; }

    private:
        struct Guards
        {
            Guards (te::Edit& edit,
                    te::Track::Array& tracks,
                    bool ignoreMuteAndSolo,
                    bool stopEveryTransport)
                : renderStatus { edit, true }, slotDisabler { edit, tracks }
            {
                if (ignoreMuteAndSolo)
                    soloIsolator =
                        std::make_unique<te::FreezePointPlugin::ScopedTrackSoloIsolator> (edit,
                                                                                          tracks);

                // Engine-wide, so it is asked for only where every transport is
                // this render's to stop.
                if (stopEveryTransport)
                    te::TransportControl::stopAllTransports (edit.engine, false, true);

                te::Renderer::turnOffAllPlugins (edit);
            }

            te::Edit::ScopedRenderStatus renderStatus;
            te::Renderer::ScopedClipSlotDisabler slotDisabler;

            // Declared last, so that it is undone first: the isolator gives the
            // edit back the mute and the solo isolation it took away, and the
            // edit should be the project again before it stops rendering.
            std::unique_ptr<te::FreezePointPlugin::ScopedTrackSoloIsolator> soloIsolator;
        };

        te::Edit& edit;
        std::unique_ptr<Guards> guards;
    };

    /** Renders a set of the edit's tracks to one file, on whichever thread asks.

        The engine's own `Renderer::renderToFile` takes the rate and the block
        size from the audio device, which a headless machine does not have, and
        drives the render through a progress window that has no UI to appear in.
        This is that function with the shape stated instead and the task driven
        here, and the guards it puts up kept: a render that leaves clip slots
        alone, starts from plugins that are not mid-playback, and takes mute and
        solo the way the caller says.

        The caller may be a worker thread — offline renders belong on one — as
        long as the message loop is running, because the guards and the render
        graph are the message thread's and this waits for it to make them. The
        blocks in between are the worker's, and they are what a render costs.
    */
    bool renderTracksToFile (te::Edit& edit,
                             const juce::File& file,
                             const juce::BigInteger& tracksToRender,
                             bool useMasterPlugins,
                             bool ignoreMuteAndSolo,
                             bool stopEveryTransport,
                             const std::function<bool()>& keepGoing)
    {
        if (tracksToRender.isZero())
            return false;

        // A render the engine has made before is answered from its audio-file
        // cache, keyed on the destination. Removing the file is what makes this
        // render this edit rather than the last one.
        file.deleteFile();

        te::Track::Array tracks;
        const auto allTracks = te::getAllTracks (edit);

        for (int index = 0; index < allTracks.size(); ++index)
            if (tracksToRender[index])
                tracks.add (allTracks[index]);

        const RenderGuards guards { edit, tracks, ignoreMuteAndSolo, stopEveryTransport };

        if (! guards.areUp())
            return false;

        te::Renderer::Parameters parameters { edit };
        parameters.destFile = file;
        parameters.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
        parameters.bitDepth = renderBitDepth;
        parameters.sampleRateForAudio = renderSampleRate;
        parameters.blockSizeForAudio = renderBlockSize;
        parameters.time = { te::TimePosition(), edit.getLength() };
        parameters.tracksToDo = tracksToRender;
        parameters.usePlugins = true;
        parameters.useMasterPlugins = useMasterPlugins;
        parameters.canRenderInMono = false;
        parameters.ditheringEnabled = false;

        te::Renderer::RenderTask task { "Duet offline render", parameters, nullptr, nullptr };

        while (task.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
        {
            // Asked between blocks, so that a render nobody is waiting for any
            // more stops rather than finishes. What it leaves behind is nothing:
            // a half-written file would be read as a whole one.
            if (keepGoing && ! keepGoing())
            {
                task.signalJobShouldExit();
                file.deleteFile();

                return false;
            }
        }

        return file.existsAsFile();
    }

    /** Every track of an edit, as the bit set a render is asked for. */
    juce::BigInteger allTracksOf (te::Edit& edit)
    {
        juce::BigInteger everyTrack;
        everyTrack.setRange (0, te::getAllTracks (edit).size(), true);

        return everyTrack;
    }

    /** One track of an edit, the same way, and nothing at all when the edit
        holds no such track.

        The bit is set by hand: the engine's own toBitSet answers with every
        track whatever it is asked about, which is why the whole-edit set above
        is the only thing it was ever right for.
    */
    juce::BigInteger oneTrackOf (te::Edit& edit, TrackRef track)
    {
        juce::BigInteger thisTrack;
        auto* audioTrack =
            dynamic_cast<te::AudioTrack*> (te::findTrackForID (edit, toItemID (track)));

        if (audioTrack == nullptr)
            return thisTrack;

        const auto index = te::getAllTracks (edit).indexOf (audioTrack);

        if (index >= 0)
            thisTrack.setBit (index);

        return thisTrack;
    }

    /** A copy of the project, made so that a render need not be a gap in what
        the producer is hearing.

        An offline render frees the playback context of the Edit it renders and
        keeps it freed until the render ends (engine notes), so one Edit cannot
        render and play at the same time. Two can: the engine renders the copy
        while the producer plays the project, and neither knows about the other.
        The copy is opened `forRendering`, which is the engine's own word for an
        Edit that never asks for an output device, so it has no playback context
        to free and no transport to stop.

        It shares this session's Engine, the way the Audition's detached Edit
        does, because a second Engine would be a second owner of the app-global
        settings store. Built and destroyed on the message thread — an Edit is
        the message thread's to make and to take down — so a worker thread may
        own one for as long as the message loop is running.
    */
    class DetachedProject
    {
    public:
        DetachedProject (te::Engine& engine, te::Edit& project, const juce::File& editFile)
        {
            te::callBlockingCatching (
                [&]
                {
                    // The Edit is built with the retriever already in it, and
                    // not given one afterwards: a clip resolves its source file
                    // as the Edit loads, and one that resolved against nothing
                    // renders as nothing.
                    auto state = project.state.createCopy();
                    auto id = te::ProjectItemID::fromProperty (state, te::IDs::projectID);

                    if (! id.isValid())
                        id = te::ProjectItemID::createNewID (te::ProjectID {});

                    copy = te::Edit::createEdit (
                        te::Edit::Options { engine,
                                            state,
                                            id,
                                            te::Edit::EditRole::forRendering,
                                            nullptr,
                                            te::Edit::getDefaultNumUndoLevels(),
                                            [editFile] { return editFile; },
                                            {} });
                });
        }

        ~DetachedProject()
        {
            te::callBlockingCatching ([this] { copy.reset(); });
        }

        DetachedProject (const DetachedProject&) = delete;
        DetachedProject (DetachedProject&&) = delete;
        DetachedProject& operator= (const DetachedProject&) = delete;
        DetachedProject& operator= (DetachedProject&&) = delete;

        [[nodiscard]] te::Edit* get() const { return copy.get(); }

    private:
        std::unique_ptr<te::Edit> copy;
    };
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

    // Opening a project is not an Action; the history starts clean — and the
    // state it starts from is settled, so that the first Action does not have
    // to carry the engine's answer to how the project was opened.
    impl->settleEngineBookkeeping();
    impl->undoManager().clearUndoHistory();
}

tracktion::engine::Edit& EngineAccess::editOf (Session& session) { return *session.impl->edit; }

juce::AudioProcessor* PluginEditorAccess::processorOf (Session& session, PluginRef plugin)
{
    if (auto* external = dynamic_cast<te::ExternalPlugin*> (session.impl->pluginFor (plugin)))
        return external->getAudioPluginInstance();
    return nullptr;
}

Session::~Session()
{
    impl->playbackKeeper.stopTimer();
    impl->takeStarter.stopTimer();

    if (impl->edit != nullptr)
        impl->edit->getTransport().stop (false, true);
}

//==============================================================================
namespace
{
    /** Puts one track's children in the order the engine's own deferred re-sort
        puts them: its priority over the kinds of child a track holds, and start
        order among the clips.

        The engine keeps that rule private to `ClipOwner.cpp`, so this is the
        rule written out rather than a call to it, and the two agreeing is what
        `the redo outlives a re-sort an Action left pending behind it` asserts.
    */
    void sortTheChildrenOf (juce::ValueTree& owner, juce::UndoManager* undoManager)
    {
        struct Sorter
        {
            [[nodiscard]] static int priorityOf (const juce::ValueTree& child)
            {
                const auto type = child.getType();

                if (type == te::IDs::AUTOMATIONTRACK)
                    return 0;
                if (te::Clip::isClipState (child))
                    return 1;
                if (type == te::IDs::PLUGIN)
                    return 2;
                if (type == te::IDs::OUTPUTDEVICES)
                    return 3;
                if (type == te::IDs::LFOS)
                    return 4;

                return -1;
            }

            [[nodiscard]] static int compareElements (const juce::ValueTree& first,
                                                      const juce::ValueTree& second) noexcept
            {
                if (const auto byKind = priorityOf (first) - priorityOf (second); byKind != 0)
                    return byKind;

                if (! te::Clip::isClipState (first) || ! te::Clip::isClipState (second))
                    return 0;

                const double firstStart = first[te::IDs::start];
                const double secondStart = second[te::IDs::start];

                if (firstStart < secondStart)
                    return -1;

                return secondStart < firstStart ? 1 : 0;
            }
        };

        Sorter sorter;
        owner.sort (sorter, undoManager, true);
    }
} // namespace

void Session::Impl::settleEngineBookkeeping() const
{
    auto* history = &undoManager();

    te::TrackList::sortTracksByType (edit->state, history);

    // The clip tracks and no others: a clip track is what the engine gives a
    // clip list, and a clip list is what schedules the re-sort.
    for (auto* track : te::getClipTracks (*edit))
        sortTheChildrenOf (track->state, history);

    edit->updateMuteSoloStatuses();
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
    // caused them. The next Action opens its own — and an undo does not, so
    // what the engine still owes this Action is done here rather than left to
    // land after one.
    impl->settleEngineBookkeeping();

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

    /** What one of a plugin's parameters is measured in.

        Duet ships the engine's own plugins, so it states their units itself,
        from the table that also says which number crosses the facade. It does
        not state a scanned plugin's: that number is the vendor's normalised
        0..1 and the only thing that says what it means is the vendor's own
        display string, which crosses as an estimate instead (ADR 0002).
    */
    std::string unitOf (te::Plugin& plugin, const std::string& parameterId)
    {
        const auto builtin = builtinOf (plugin);

        if (! builtin.has_value())
            return {};

        const auto units = unitsOfBuiltinParameter (*builtin, parameterId);

        return units.has_value() ? std::string { units->unit } : std::string {};
    }

    /** The value the facade speaks for a curve point, given the one the curve
        stores — the mirror of `toCurveValue`, so that a point goes in and comes
        back the same however its target holds it.
    */
    double fromCurveValue (const AutomationTarget& target,
                           te::AutomatableParameter& parameter,
                           float value)
    {
        if (target.kind == AutomationTarget::Kind::trackVolume)
            return te::volumeFaderPositionToDB (value);

        if (target.kind == AutomationTarget::Kind::pluginParameter)
            return realParameterValue (parameter, value);

        return value;
    }

    PluginInfo describe (te::Plugin& plugin)
    {
        PluginInfo info;
        info.plugin = toRef<PluginRef> (plugin.itemID);
        info.name = plugin.getName().toStdString();
        info.builtin = builtinOf (plugin);
        info.sidechainSource = toRef<TrackRef> (plugin.getSidechainSourceID());
        info.bypassed = ! plugin.isEnabled();
        info.latencySeconds = plugin.getLatencySeconds();

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

MasterInfo Session::master() const
{
    MasterInfo info;

    if (auto* fader = impl->faderFor (masterChannel))
    {
        info.volumeDb =
            impl->edit->state.getProperty (masterVolumeDbProperty, fader->getVolumeDb());
        info.pan = fader->getPan();
        info.muted = impl->edit->state.getProperty (masterMutedProperty, false);
    }

    for (auto* plugin : impl->edit->getMasterPluginList().getPlugins())
        if (isProducersPlugin (*plugin))
            info.plugins.push_back (describe (*plugin));

    return info;
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
    {
        const auto held = parameter->getCurrentExplicitValue();

        // A reciprocal turns the range end for end, so the ends are sorted
        // after the conversion rather than converted in place.
        const auto oneEnd = realParameterValue (*parameter, parameter->getValueRange().getStart());
        const auto otherEnd = realParameterValue (*parameter, parameter->getValueRange().getEnd());

        out.push_back ({ parameter->paramID.toStdString(),
                         parameter->getParameterName().toStdString(),
                         realParameterValue (*parameter, held),
                         std::min (oneEnd, otherEnd),
                         std::max (oneEnd, otherEnd),
                         parameterSkew (*parameter),
                         parameter->valueToString (held).toStdString(),
                         unitOf (*p, parameter->paramID.toStdString()) });
    }

    return out;
}

std::string Session::pluginOpaqueState (PluginRef plugin) const
{
    auto* external = dynamic_cast<te::ExternalPlugin*> (impl->pluginFor (plugin));
    if (external == nullptr)
        return {};
    auto* processor = external->getAudioPluginInstance();
    if (processor == nullptr)
        return {};

    juce::MemoryBlock state;
    processor->getStateInformation (state);
    return { static_cast<const char*> (state.getData()), state.getSize() };
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
                         fromCurveValue (target, *parameter, value),
                         curve.getPointCurve (point) });
    }

    return out;
}

double Session::automationValueAt (const AutomationTarget& target, double timeSeconds) const
{
    auto* parameter = impl->parameterFor (target);

    if (parameter == nullptr)
        return 0.0;

    const auto value = te::getValueAt (*parameter, te::TimePosition::fromSeconds (timeSeconds));

    return fromCurveValue (target, *parameter, value);
}

double Session::tempoBpm() const { return impl->edit->tempoSequence.getBpmAt (te::TimePosition()); }

TimeSignature Session::timeSignature() const
{
    if (auto* timeSig = impl->edit->tempoSequence.getTimeSig (0))
        return { timeSig->numerator, timeSig->denominator };

    return {};
}

double Session::editLengthSeconds() const { return impl->edit->getLength().inSeconds(); }

double Session::barAtSeconds (double seconds) const
{
    // The engine counts whole bars elapsed and the producer counts bars, one
    // apart, which is the same offset barStartSeconds takes off on the way in.
    return impl->edit->tempoSequence.toBarsAndBeats (te::TimePosition::fromSeconds (seconds))
               .getTotalBars()
           + 1.0;
}

double Session::beatsAtSeconds (double seconds) const
{
    return impl->edit->tempoSequence.toBeats (te::TimePosition::fromSeconds (seconds)).inBeats();
}

double Session::secondsAtBeats (double beats) const
{
    return impl->edit->tempoSequence.toTime (tracktion::BeatPosition::fromBeats (beats))
        .inSeconds();
}

double Session::secondsAtBar (double bar) const
{
    const auto whole = static_cast<int> (std::floor (bar));
    const auto start = barStartSeconds (whole);

    return start + (bar - whole) * (barStartSeconds (whole + 1) - start);
}

std::vector<SectionInfo> Session::sections() const
{
    std::vector<SectionInfo> out;

    const auto list = impl->edit->state.getChildWithName (juce::Identifier { sectionsNode });

    for (const auto& entry : list)
        out.push_back ({ entry[juce::Identifier { sectionNameProperty }].toString().toStdString(),
                         static_cast<int> (entry[juce::Identifier { sectionStartBarProperty }]),
                         static_cast<int> (entry[juce::Identifier { sectionEndBarProperty }]) });

    return out;
}

std::string Session::key() const
{
    return impl->edit->state[juce::Identifier { projectKeyProperty }].toString().toStdString();
}

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

void Session::previewVolumeDb (TrackRef channel, double db)
{
    if (auto* fader = impl->faderFor (channel))
        fader->volParam->setParameter (te::decibelsToVolumeFaderPosition (static_cast<float> (db)),
                                       juce::dontSendNotification);
}

void Session::previewPan (TrackRef channel, double pan)
{
    if (auto* fader = impl->faderFor (channel))
        fader->panParam->setParameter (static_cast<float> (pan), juce::dontSendNotification);
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
    /** Takes off what an offline render leaves behind.

        The engine's render guards write `soloIsolate` and `playSlotClips` onto
        every track they render and then put back what was there before, which
        for a track that had neither is the property present and set to its
        default. Neither says anything about what the track renders — a track
        render ignores mute and solo outright — so a digest that counted them
        would say a track had changed because it had been measured, and nothing
        would ever be answered out of a cache twice (engine notes).
    */
    void forgetRenderLeftovers (const juce::ValueTree& tree)
    {
        static const juce::Identifier playSlotClips { "playSlotClips" };

        std::vector<juce::ValueTree> remaining { tree };

        while (! remaining.empty())
        {
            auto node = remaining.back();
            remaining.pop_back();

            node.removeProperty (te::IDs::soloIsolate, nullptr);
            node.removeProperty (playSlotClips, nullptr);

            for (const auto& child : node)
                remaining.push_back (child);
        }
    }

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

std::uint64_t Session::revision() const noexcept { return impl->revision; }

std::string Session::trackStateDigest (TrackRef track) const
{
    juce::ValueTree state;

    if (track == masterChannel)
    {
        // The master is the whole project through the master chain, so what it
        // renders is whatever anything in the project renders.
        state = impl->edit->state.createCopy();
        state.removeChild (state.getChildWithName (te::IDs::TRANSPORT), nullptr);
        state.removeProperty ("lastSignificantChange", nullptr);
        state.removeProperty ("modifiedBy", nullptr);
        state.removeProperty (te::IDs::projectID, nullptr);
    }
    else if (auto* audioTrack = impl->trackFor (track))
    {
        state = audioTrack->state.createCopy();
    }
    else
    {
        return {};
    }

    forgetRenderLeftovers (state);

    std::string canonical;
    appendCanonicalised (state, canonical);

    // The tempo map with it: the track's own state says where a clip is in
    // beats, and the tempo is what turns that into a moment. Read off the
    // edit's tree, the sequence keeping its own copy to itself.
    if (track != masterChannel)
        appendCanonicalised (impl->edit->state.getChildWithName (te::IDs::TEMPOSEQUENCE),
                             canonical);

    std::ostringstream digest;
    digest << std::hex << std::hash<std::string> {}(canonical) << "/" << std::dec
           << canonical.size() << "B";

    return std::move (digest).str();
}

bool Session::renderToFile (const std::filesystem::path& destination,
                            const std::function<bool()>& keepGoing)
{
    // What the producer hears: a muted track is silent in the file and a soloed
    // one is the only thing in it, because mute and solo are the project and a
    // render of the whole project is the project (sh2dkg).
    return renderTracksToFile (*impl->edit,
                               toJuceFile (destination),
                               allTracksOf (*impl->edit),
                               withMasterPlugins,
                               honouringMuteAndSolo,
                               stoppingEveryTransport,
                               keepGoing);
}

bool Session::renderTrackToFile (TrackRef track,
                                 const std::filesystem::path& destination,
                                 const std::function<bool()>& keepGoing)
{
    // Without the master chain: a track rendered on its own is what that track
    // puts out, and the master is what the whole project goes through after it.
    // Ignoring mute and solo for the same reason: the track asked for is the
    // track rendered, whatever the project has muted or soloed elsewhere.
    return renderTracksToFile (*impl->edit,
                               toJuceFile (destination),
                               oneTrackOf (*impl->edit, track),
                               withoutMasterPlugins,
                               ignoringMuteAndSolo,
                               stoppingEveryTransport,
                               keepGoing);
}

bool Session::renderDetachedToFile (const std::filesystem::path& destination,
                                    const std::function<bool()>& keepGoing)
{
    const DetachedProject copy { impl->engine, *impl->edit, toJuceFile (impl->editFile) };

    if (copy.get() == nullptr)
        return false;

    return renderTracksToFile (*copy.get(),
                               toJuceFile (destination),
                               allTracksOf (*copy.get()),
                               withMasterPlugins,
                               honouringMuteAndSolo,
                               leavingOtherTransportsRolling,
                               keepGoing);
}

bool Session::renderDetachedTrackToFile (TrackRef track,
                                         const std::filesystem::path& destination,
                                         const std::function<bool()>& keepGoing)
{
    const DetachedProject copy { impl->engine, *impl->edit, toJuceFile (impl->editFile) };

    if (copy.get() == nullptr)
        return false;

    return renderTracksToFile (*copy.get(),
                               toJuceFile (destination),
                               oneTrackOf (*copy.get(), track),
                               withoutMasterPlugins,
                               ignoringMuteAndSolo,
                               leavingOtherTransportsRolling,
                               keepGoing);
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

void Session::Impl::waitForTheDevicesToGoQuiet (std::uint32_t since)
{
    runTheMessageLoopUntil (
        [this, since] {
            return deviceListIsBuilt()
                   && nowMs() - devicesLastMovedSince (since) >= deviceApplyQuietMs;
        },
        deviceRebuildBoundMs);
}

void Session::Impl::useHostedAudioDevice()
{
    auto& deviceManager = engine.getDeviceManager();

    if (deviceManager.isHostedAudioDeviceInterfaceInUse())
        return;

    // The quiet the wait at the end is looking for starts here, before the
    // switch the engine is about to answer.
    const auto switchedAt = nowMs();

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

    // Giving up the audio device is a device change like any other, and the
    // session only ever hears about one when the message loop delivers the
    // engine's broadcast. Nothing has pumped it yet, so say it here: a switch
    // the session made itself is not a session whose devices have never moved,
    // and a take asked for straight afterwards takes the pre-roll it would take
    // on any other moved devices.
    lastDeviceChangeMs = switchedAt;

    // Then wait the switch out. The flush above ran the engine's pending update
    // exactly once, and that update settles the defaults, which rescans, which
    // can post another — so a caller that returned here would leave the rest to
    // land on whatever pump came next, over a take by then.
    waitForTheDevicesToGoQuiet (switchedAt);
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
    // The quiet the wait below is looking for starts here: an engine that has
    // not answered the ask yet must not read as one that has finished.
    const auto askedAt = impl->nowMs();
    impl->askForTheDeviceList();

    // rescanMidiDeviceList applies on a 5 ms timer; checkDefaultDevicesAreValid
    // then settles the defaults and applies again ~5 ms later. Both have to
    // land before this returns, or a caller that asked for the rebuild to be
    // over would still be waiting on the engine.
    impl->waitForTheDevicesToGoQuiet (askedAt);

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

void Session::setPlayRetry (int intervalMilliseconds, int attempts)
{
    impl->playRetryIntervalMs = std::max (1, intervalMilliseconds);
    impl->playRetryAttempts = std::max (0, attempts);
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

    // An offline render of this Edit keeps its playback context freed for as
    // long as it runs (engine notes), so an ask made now cannot be answered.
    // The render is waited out rather than asked through: nothing is counted
    // while it runs, and the first tick after it ends asks with the whole
    // window still in hand. That is what keeps a render longer than the window
    // from leaving the transport stopped with nothing asking it to roll.
    if (edit->isRendering())
        return;

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
    impl->playbackKeeper.startTimer (impl->playRetryIntervalMs);
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

void Session::setMetronomeEnabled (bool enabled) { impl->edit->clickTrackEnabled = enabled; }

bool Session::metronomeEnabled() const { return impl->edit->clickTrackEnabled; }

double Session::cpuLoad() const noexcept
{
    return static_cast<double> (impl->engine.getDeviceManager().getCpuUsage());
}

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
