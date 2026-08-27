#pragma once

#include <duet/model/Session.h>

#include <duet/model/EngineAccess.h>

#include "AppSettingsStore.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

/** The engine side of the model, shared by the two files that make it up.

    Session.h names no engine type, so everything engine-shaped is here, off the
    module's public include path. Session.cpp holds the Session, EditOps.cpp the
    vocabulary; both need the same handle onto the Edit.
*/
namespace duet::model
{
namespace te = tracktion;

/** Duet's external-plugin facts retained on the plugin's project node. */
inline constexpr const char* externalPluginIdentifierProperty = "duetExternalPluginIdentifier";
inline constexpr const char* externalParametersNode = "DUET_EXTERNAL_PARAMETERS";
inline constexpr const char* externalParameterNode = "DUET_EXTERNAL_PARAMETER";
inline constexpr const char* externalParameterIdProperty = "parameterId";
inline constexpr const char* externalParameterValueProperty = "value";
inline constexpr const char* masterMutedProperty = "duetMasterMuted";
inline constexpr const char* masterVolumeDbProperty = "duetMasterVolumeDb";

/** The arrangement's named sections, and the key the project declares.

    Duet's own facts about the project, kept on the Edit's own state tree beside
    the master's, because that is where the model already keeps what the engine
    has no place for and because a save copies that tree whole (ADR 0005). The
    engine has an ArrangerTrack that would hold sections, and Duet does not use
    it: it is a track, so it would join every track list, every render bit set
    and every graph the engine builds, to carry three strings.
*/
inline constexpr const char* sectionsNode = "DUETSECTIONS";
inline constexpr const char* sectionNode = "SECTION";
inline constexpr const char* sectionNameProperty = "name";
inline constexpr const char* sectionStartBarProperty = "startBar";
inline constexpr const char* sectionEndBarProperty = "endBar";
inline constexpr const char* projectKeyProperty = "duetProjectKey";

inline juce::File toJuceFile (const std::filesystem::path& path)
{
    return juce::File { juce::String { path.string() } };
}

inline std::filesystem::path toPath (const juce::File& file)
{
    return std::filesystem::path { file.getFullPathName().toStdString() };
}

inline juce::String toJuceString (std::string_view text)
{
    return juce::String { std::string { text } };
}

/** The aux return a bus track carries, if any: the engine routes sends by bus
    number, and this is what ties one of those numbers to a Duet bus.
*/
inline te::AuxReturnPlugin* returnOn (te::AudioTrack& bus)
{
    for (auto* plugin : bus.pluginList.getPlugins())
        if (auto* auxReturn = dynamic_cast<te::AuxReturnPlugin*> (plugin))
            return auxReturn;

    return nullptr;
}

/** Whether a plugin in a track's chain is one the producer put there.

    The rest are Duet's own, and the producer never asked for any of them: the
    volume-and-pan fader and the level meter every track is born with, the return
    that puts a send into a bus, and the send that feeds it.
*/
inline bool isProducersPlugin (te::Plugin& plugin)
{
    return dynamic_cast<te::VolumeAndPanPlugin*> (&plugin) == nullptr
           && dynamic_cast<te::LevelMeterPlugin*> (&plugin) == nullptr
           && dynamic_cast<te::AuxReturnPlugin*> (&plugin) == nullptr
           && dynamic_cast<te::AuxSendPlugin*> (&plugin) == nullptr;
}

/** Where in the whole chain a plugin at a producer-chain position belongs.

    The producer's plugins occupy one stretch of the chain: after the return that
    feeds a bus, because a plugin in front of the return processes silence, and
    before the fader, the meter and any send, because those are what the chain
    ends with. A position counts inside that stretch and is clamped to it, so
    that position zero is first among the producer's effects and no position can
    put one of them where it would never be heard.
*/
inline int rawPositionFor (const te::PluginList& chain, int producerPosition)
{
    const auto wanted = std::max (0, producerPosition);
    int raw = 0;
    int seen = 0;

    for (int index = 0; index < chain.size(); ++index)
    {
        auto* plugin = chain[index];

        if (plugin == nullptr)
            continue;

        if (isProducersPlugin (*plugin))
        {
            if (seen == wanted)
                return index;

            ++seen;
            raw = index + 1;
            continue;
        }

        // The return is the one thing the effects go after; everything else Duet
        // owns ends the stretch.
        if (dynamic_cast<te::AuxReturnPlugin*> (plugin) != nullptr)
        {
            raw = index + 1;
            continue;
        }

        break;
    }

    return raw;
}

inline te::EditItemID toItemID (std::uint64_t ref)
{
    return te::EditItemID::fromRawID (static_cast<juce::uint64> (ref));
}

template <typename Ref>
Ref toRef (te::EditItemID id)
{
    return static_cast<Ref> (id.getRawID());
}

/** How the project refers to a file: relative to the project folder when the
    file is inside it, absolute when it is not.

    The engine's own relative paths are written against the edit file and read
    against the folder that holds it, one level apart, which is how a clip ends
    up pointing at a file that does not exist and playing silence (hazard 5).
    Duet writes the reference the project reads.
*/
std::string projectReferenceTo (const std::filesystem::path& projectFolder,
                                const std::filesystem::path& sourceFile);

/** One meter: a client kept attached to whichever measurer is currently the
    right one to read.

    A measurer only measures while something is listening to it, and the ones
    worth reading come and go — the master's belongs to the playback context,
    which is freed and rebuilt whenever the engine rebuilds its graph. So the
    client is the durable thing, and this moves it.
*/
class Meter
{
public:
    Meter() = default;
    ~Meter() { attachTo (nullptr); }

    Meter (const Meter&) = delete;
    Meter& operator= (const Meter&) = delete;

    void attachTo (te::LevelMeasurer* newMeasurer)
    {
        if (newMeasurer == measurer.get())
            return;

        if (auto* previous = measurer.get())
            previous->removeClient (client);

        measurer = newMeasurer;

        if (newMeasurer != nullptr)
            newMeasurer->addClient (client);
    }

    /** The loudest of the channels since the last read, which this clears. */
    [[nodiscard]] double readPeakDb()
    {
        auto peakDb = silentDb;

        for (int channel = 0; channel < client.getNumChannelsUsed(); ++channel)
            peakDb =
                std::max (peakDb, static_cast<double> (client.getAndClearAudioLevel (channel).dB));

        return peakDb;
    }

private:
    juce::WeakReference<te::LevelMeasurer> measurer;
    te::LevelMeasurer::Client client;
};

/** What Duet tells the engine about itself: scans run in a child process, and
    recordings land in the project folder rather than the engine's default.
*/
struct DuetBehaviour final : te::EngineBehaviour
{
    explicit DuetBehaviour (const std::filesystem::path& directory)
        : recordingDirectory (&directory)
    {
    }

    bool canScanPluginsOutOfProcess() override { return true; }

    juce::File getFileForNewAudioRecording (te::Track& track,
                                            const juce::String& fileExtension) override;

    /** The session's own, which it may be told to change: the behaviour reads
        it and does not hold a second copy to keep in step.
    */
    const std::filesystem::path* recordingDirectory = nullptr;
};

/** Everything engine-shaped lives here, so that Session.h can name no engine or
    JUCE type. The initialiser is declared first so that it outlives the engine:
    the engine's managers start timers and background threads that need a
    message manager, which is also what makes a Session usable headlessly.
*/
struct Session::Impl
{
    explicit Impl (std::filesystem::path file)
        : editFile (std::move (file)), projectFolder (editFile.parent_path())
    {
        // The behaviour makes the engine's child-process scanner available;
        // Duet always uses it. Hosting remains in-process.
        engine.getPluginManager().setUsesSeparateProcessForScanning (true);
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    // Before the engine, because the engine is told where the project folder is
    // as it is made.
    std::filesystem::path editFile;
    std::filesystem::path projectFolder;

    /** Where takes are written: the project folder, until whoever opened the
        project says which directory inside it recordings go into.
    */
    std::filesystem::path recordingDirectory { projectFolder };

    /** The one app-global store, lent to this session's Engine: the shell's
        settings and the engine's are the same file, and a second holder of it
        would write the set it read over the set the other holds.
    */
    te::Engine engine { std::make_unique<SharedPropertyStorage>(),
                        nullptr,
                        std::make_unique<DuetBehaviour> (recordingDirectory) };
    std::unique_ptr<te::Edit> edit;
    std::function<void()> projectChanged;
    mutable std::uint64_t revision = 0;

    /** The canonical project state before the live Audition, and which pending
        data produced it. The transport is kept outside both states.
    */
    juce::ValueTree stateBeforeAudition;
    const Suggestion* auditionedSuggestion = nullptr;

    /** The detached Edit that turns operation data into a complete suggested
        state. It shares this session's Engine so Audition cannot create a
        second app-global settings owner. Reused so its in-memory item-ID
        allocator never reuses an ID from an earlier A/B cycle while the live
        graph may still retain that item.
    */
    std::unique_ptr<te::Edit> suggestionEdit;

    juce::UndoManager& undoManager() const { return edit->getUndoManager(); }

    /** Does the bookkeeping the engine would otherwise do on the next turn of
        the message loop, here, where it belongs (hazard 9).

        The engine answers a project change with writes of its own — the track
        list sorted by kind, each track's children sorted, the mute and solo
        statuses re-derived — and it makes them through the Edit's UndoManager,
        asynchronously. An Action's transaction stays open for exactly that
        (ADR 0004), but an undo closes it, and an answer that lands after that
        opens a transaction the producer never asked for and takes their redo
        with it. Doing the work while the Action is still open leaves the
        engine's own pass with nothing to write, whenever it gets to run.
    */
    void settleEngineBookkeeping() const;

    //==============================================================================
    // Keeping an asked-for playback rolling (hazard 6).

    /** How many times running the transport has been asked to play without it
        rolling. Reset by every tick that finds it rolling, so the rebuild —
        which arrives after playback has started — gets the whole window again.
    */
    int askedWithoutRolling = 0;

    /** Asks the transport to play, allocating the playback context first: after
        the device rebuild there is no context to play through.
    */
    void askTransportToPlay();

    /** One tick of the retry: gives up, does nothing, or asks again. */
    void keepPlaybackRolling();

    //==============================================================================
    // The meters.

    Meter outputMeter;
    std::unordered_map<TrackRef, std::unique_ptr<Meter>> trackMeters;

    /** Points every meter at the measurer it should be reading now, and drops
        the ones whose track has gone.

        Called wherever playback is (re)started or found still going, because
        that is when the master's measurer can have been replaced under it.
    */
    void syncMeters();

    /** Runs for exactly as long as playback is wanted — its running is that
        memory, and there is no second copy of it to keep in step — and drives
        keepPlaybackRolling.
    */
    struct PlaybackKeeper final : juce::Timer
    {
        explicit PlaybackKeeper (Impl& owner) : impl (&owner) {}
        void timerCallback() override { impl->keepPlaybackRolling(); }

        Impl* impl = nullptr;
    };

    /** The stretch the transport loops over, in beats.

        Kept in musical time and not in seconds, because the tempo map moves
        under it: a tempo change rescales every clip, and a loop that stayed
        where it was in seconds would stop being the loop over the phrase the
        producer set it over. The engine rescales its own loop range when the
        tempo is set and does not when that change is undone, so the range it
        holds is only ever a cached answer — this is the question.
    */
    std::optional<std::pair<double, double>> loopBeats;

    /** Puts the engine's loop range back in step with the musical one. */
    void applyLoopRange() const;

    void announceChange() const
    {
        ++revision;
        if (projectChanged)
            projectChanged();
    }

    //==============================================================================
    // Finding the things a ref names.

    te::AudioTrack* trackFor (TrackRef ref) const
    {
        return dynamic_cast<te::AudioTrack*> (te::findTrackForID (*edit, toItemID (ref)));
    }

    te::Clip* clipFor (ClipRef ref) const { return te::findClipForID (*edit, toItemID (ref)); }

    te::MidiClip* midiClipFor (ClipRef ref) const
    {
        return dynamic_cast<te::MidiClip*> (clipFor (ref));
    }

    te::Plugin* pluginFor (PluginRef ref) const
    {
        return te::findPluginForID (*edit, toItemID (ref)).get();
    }

    te::VolumeAndPanPlugin* faderFor (TrackRef ref) const
    {
        if (ref == masterChannel)
            return edit->getMasterVolumePlugin().get();

        if (auto* track = trackFor (ref))
            return track->getVolumePlugin();

        return nullptr;
    }

    /** The parameter an automation target names, or null when the thing that
        would own the curve is not there.
    */
    te::AutomatableParameter* parameterFor (const AutomationTarget& target) const;

    /** Puts every plugin parameter back in step with the state it is stored in.

        A plugin keeps each parameter's value in the plugin as well as in the
        state, and the engine deliberately does not follow a state change back
        into the plugin: a change there may be automation or a modifier
        speaking, and neither is the value the producer set. An undo is neither
        of those — it is the producer's own value returning — so the model says
        so. Without this a read after an undo answers with the value the undo
        took away, and the fader does not move.
    */
    void refreshParametersFromState() const;

    //==============================================================================
    // Inputs. An input is a device of the machine and not a part of the project,
    // and the engine names one with a string, so the model hands out a handle
    // for it the way it does for a note.

    mutable std::unordered_map<InputRef, std::string> inputsByRef;
    mutable InputRef nextInputRef = 1;

    /** The handle for an input, made on first sight of it. */
    InputRef refForInput (const juce::String& deviceID) const;

    /** The input a handle names, or null when this machine has no such input. */
    te::InputDevice* inputDeviceFor (InputRef ref) const;

    /** The instance of an input in this Edit's playback context, making the
        context first if there is none: an input has nothing to be assigned to a
        track until there is a context for it to play through.
    */
    te::InputDeviceInstance* instanceFor (InputRef ref) const;

    /** What the project stores about where a track records from, read straight
        out of the Edit's state.

        The engine keeps the assignment there, and reads it back through the
        playback context — but a question about a track should not be what opens
        an audio device, so this asks the state instead.
    */
    juce::ValueTree destinationStateFor (TrackRef track) const;

    /** Which input owns a stored destination. */
    InputRef inputOfDestination (const juce::ValueTree& destination) const;

    //==============================================================================
    // Running the audio with no audio device at all.

    /** Puts the session on inputs and outputs that go nowhere, once, and
        returns with the engine's answer to that switch over.
    */
    void useHostedAudioDevice();

    /** True once the session is on them. */
    bool onHostedAudioDevice = false;

    /** Pushes blocks through the hosted device for a stretch of seconds,
        playing a signal into its inputs.
    */
    void pushBlocks (double seconds, const InputSignal& playedIn) const;

    //==============================================================================
    // Recording.

    /** Whether the engine has built its device list yet.

        Before the build the engine has no MIDI inputs at all; after it, it has
        at least its own "All MIDI Ins", whatever this machine is plugged into.
        So the MIDI inputs being there is the build having happened — the only
        sign of it the engine offers.
    */
    bool deviceListIsBuilt() const { return engine.getDeviceManager().getNumMidiInDevices() > 0; }

    /** Whether the engine's devices are still enough to start a take on.

        Built, and then unchanged for long enough to believe it. The build is
        not one event: it settles its defaults afterwards and rebuilds again
        over what it settled, and any of those rebuilds frees the playback graph
        and ends a take rolling through it (hazard 6). So a take waits for the
        churn to stop and not merely to start.
    */
    bool devicesAreSettled() const;

    /** Asks the engine to build its device list now, rather than four seconds
        into the session, which is when its own timer would.
    */
    void askForTheDeviceList() const { engine.getDeviceManager().rescanMidiDeviceList(); }

    /** Runs the message loop until the engine has nothing more to say about the
        devices it was last asked about at `since`.

        The engine answers a device change with more device changes — an apply
        settles the defaults, and settling them rescans — so one flush of its
        pending work does not end the answer. What ends it is the engine going
        quiet: the list built, and no change broadcast for long enough to
        believe it. Waited for rather than counted out in milliseconds, so a
        machine that cannot deliver two timer ticks in twenty of them still gets
        both applies — and bounded, so one that never goes quiet still returns.
    */
    void waitForTheDevicesToGoQuiet (std::uint32_t since);

    /** Starts the take: from here on the armed tracks take what their inputs
        carry, from the playhead on.
    */
    void beginTake();

    /** One tick of the pre-roll: waits, or gives up waiting and starts the
        take.
    */
    void startTakeWhenDevicesAreSettled();

    /** Runs for exactly as long as a take is waiting for the engine's devices —
        its running is that memory, and there is no second copy of it to keep in
        step — and drives startTakeWhenDevicesAreSettled.
    */
    struct TakeStarter final : juce::Timer
    {
        explicit TakeStarter (Impl& owner) : impl (&owner) {}
        void timerCallback() override { impl->startTakeWhenDevicesAreSettled(); }

        Impl* impl = nullptr;
    };

    /** How long the engine's devices have to have been unchanged before a
        take starts on them, how often a waiting take looks, and how many of
        those looks it takes before the take starts regardless.
    */
    std::uint32_t deviceQuietMs = 100;
    int devicePollMs = 20;
    int deviceWaitAttempts = 100;

    /** What the settle-wait reads instead of the wall clock. */
    std::function<std::uint32_t()> deviceNow { [] { return juce::Time::getMillisecondCounter(); } };

    std::uint32_t nowMs() const { return deviceNow(); }

    /** How many ticks the take has been waiting for the devices. */
    int waitedForTheDevices = 0;

    /** When the engine last said it had changed its devices, on the counter
        juce::Time keeps — nothing until it says so, which is a session whose
        devices have never moved and so have nothing to settle from.
    */
    std::optional<juce::uint32> lastDeviceChangeMs;

    /** The same moment, counted no earlier than `since`.

        A wait that asked the engine for a rebuild measures its quiet from the
        ask: an engine that has not answered yet must not read as one that has
        finished, and each apply that lands afterwards moves the moment on
        again. Signed on the difference, so the counter's wrap is a difference
        like any other.
    */
    std::uint32_t devicesLastMovedSince (std::uint32_t since) const
    {
        if (lastDeviceChangeMs.has_value()
            && static_cast<std::int32_t> (static_cast<std::uint32_t> (*lastDeviceChangeMs) - since)
                   > 0)
            return static_cast<std::uint32_t> (*lastDeviceChangeMs);

        return since;
    }

    /** Listens for the engine saying it has changed its devices, which is the
        only announcement it makes of the rebuilds that end takes.
    */
    struct DeviceWatcher final : juce::ChangeListener
    {
        explicit DeviceWatcher (Impl& owner) : impl (&owner)
        {
            impl->engine.getDeviceManager().addChangeListener (this);
        }

        ~DeviceWatcher() override { impl->engine.getDeviceManager().removeChangeListener (this); }

        DeviceWatcher (const DeviceWatcher&) = delete;
        DeviceWatcher& operator= (const DeviceWatcher&) = delete;

        void changeListenerCallback (juce::ChangeBroadcaster* /*deviceManager*/) override
        {
            impl->lastDeviceChangeMs = impl->nowMs();
        }

        Impl* impl = nullptr;
    };

    /** The file each armed track is recording into, taken before the take is
        stopped: afterwards the clip holds a reference the engine wrote, and that
        reference is the thing that has to be corrected.
    */
    std::unordered_map<TrackRef, std::filesystem::path> recordingFiles() const;

    /** Writes the reference the project reads onto every clip a take just made
        (hazard 5, again): the engine writes a recorded clip's path relative to
        the edit file and reads it relative to the folder holding that file.
    */
    void
        pinRecordedSources (const std::unordered_set<ClipRef>& clipsBefore,
                            const std::unordered_map<TrackRef, std::filesystem::path>& files) const;

    /** Every clip in the edit, by ref. */
    std::unordered_set<ClipRef> allClips() const;

    //==============================================================================
    // Notes. The engine gives a note no durable identity of its own, so the
    // model keeps one: a note is its ValueTree, and JUCE's undo of a removed
    // child puts back the very object it took away, so a handle stays pointed at
    // the same note across undo and redo.

    struct NoteHandle
    {
        ClipRef clip = noClip;
        juce::ValueTree state;
    };

    mutable std::unordered_map<NoteRef, NoteHandle> notesByRef;
    mutable NoteRef nextNoteRef = 1;

    /** The corresponding session-only handles while suggestionEdit is the Edit
        operations are being applied to.
    */
    std::unordered_map<NoteRef, NoteHandle> suggestionNotesByRef;
    NoteRef nextSuggestionNoteRef = 1;

    /** The handle for a note, made on first sight of it. */
    NoteRef refForNote (ClipRef clip, const juce::ValueTree& noteState) const;

    /** The note a handle names, or null once it is gone for good. */
    te::MidiNote* noteFor (NoteRef ref) const;

    //==============================================================================
    // Declared last, so that they stop and unsubscribe before anything they
    // touch goes away.
    DeviceWatcher deviceWatcher { *this };
    PlaybackKeeper playbackKeeper { *this };
    TakeStarter takeStarter { *this };

    static std::vector<std::string> toStrings (const juce::StringArray& strings)
    {
        std::vector<std::string> out;
        out.reserve (static_cast<std::size_t> (strings.size()));

        for (const auto& string : strings)
            out.push_back (string.toStdString());

        return out;
    }
};

/** What a track is for.

    Two of the three kinds are legible from the track itself, and the third is
    not: a Duet group bus is an ordinary engine track that the producer
    designated as a bus, and nothing in the engine's own state says so. The
    designation is stored on the track's own tree rather than in the DUET tree,
    so that it travels with the track — deleting the track takes it away, and
    undoing the deletion brings it back, with no code of ours involved.
*/
TrackKind trackKindOf (te::AudioTrack& track);

/** The engine plugin a Duet built-in is, as the plugin cache names it. */
const char* engineTypeOf (BuiltinPlugin plugin);

/** Which built-in a plugin is, or nothing for one Duet does not ship. */
std::optional<BuiltinPlugin> builtinOf (te::Plugin& plugin);

/** What one of a built-in's parameters is in the producer's terms.

    Duet ships the engine's own plugins, so it owns what their numbers mean, and
    several of them hold a number the producer never sees: the compressor keeps
    its ratio as one over the ratio and its threshold as a gain, the reverb keeps
    a level the producer reads in decibels as a plain fraction. The facade speaks
    the producer's number in both directions and this is the conversion, one
    entry per parameter.
*/
struct ParameterUnits
{
    /** What the producer's number is measured in, and empty for a parameter
        that is a plain number or a named choice.
    */
    std::string_view unit;

    /** The producer's number, given the one the engine holds. Null for the
        parameters the engine already holds in the producer's terms.
    */
    double (*toReal) (double engineValue) = nullptr;

    /** The engine's number, given the producer's. Null alongside a null
        `toReal`, and never null beside a set one.
    */
    double (*fromReal) (double realValue) = nullptr;

    /** How the producer's number is spread over the range it moves in, as a
        control that draws the parameter would spread it: the proportion of the
        range raised to this is the position, so a skew below one lifts the
        small values away from the floor.

        One is linear, which is what almost every parameter wants — a frequency
        in hertz and a level in decibels are already even in the producer's ear.
        It is Duet's own statement about that scale and not the engine's:
        `AutomatableParameter::valueRange` carries a skew for the *raw* number,
        and where a conversion turns the range end for end that skew describes
        the wrong direction.
    */
    double skew = 1.0;
};

/** How a built-in's parameter crosses the facade, or nothing for a parameter no
    built-in Duet ships owns — a plugin from an engine newer than this one.
*/
std::optional<ParameterUnits> unitsOfBuiltinParameter (BuiltinPlugin plugin,
                                                       std::string_view parameterId);

/** The producer's number for a parameter, given the one the engine holds.

    An external plugin's number is its own and crosses untouched: Duet does not
    know a third party's mapping and does not invent one (ADR 0002).
*/
double realParameterValue (te::AutomatableParameter& parameter, double engineValue);

/** How a parameter's producer-facing range is spread over a control that draws
    it, and one — linear — for every parameter Duet has no better statement
    about, an external plugin's included.
*/
double parameterSkew (te::AutomatableParameter& parameter);

/** The engine's number for a parameter, given the producer's, held inside the
    range the parameter accepts.

    The clamp is the facade's, not the engine's: `setParameter` remembers what it
    was handed and processes with the value clipped into range, so a number
    outside it would read back as written while the plugin used another one.
*/
double engineParameterValue (te::AutomatableParameter& parameter, double realValue);

/** Writes every parameter of a plugin into its state, at the value it already
    has.

    A parameter sitting at its default has no property in the state at all, and
    the engine writes one the first time the value changes — which means the
    undo of that first change takes the property away again, and the next thing
    to put the parameter back in step with the state has to create it, an edit
    of its own that would clear the redo stack. A plugin that states all of its
    parameters from the moment it is made never gets into that position: an undo
    only ever puts an existing property back to an earlier value.
*/
void stateParametersExplicitly (te::Plugin& plugin);

/** Writes an external parameter's explicit value through an Action's undo
    manager. The engine keeps external values in the instance until a flush, so
    Duet states them as they are changed instead.
*/
void stateExternalParameter (te::ExternalPlugin& plugin,
                             te::AutomatableParameter& parameter,
                             juce::UndoManager& undoManager);
} // namespace duet::model
