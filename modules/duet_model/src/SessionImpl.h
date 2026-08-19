#pragma once

#include <duet/model/Session.h>

#include <duet/model/EngineAccess.h>

#include <algorithm>
#include <memory>
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

/** What Duet tells the engine about itself.

    Only one thing so far, and it is recording's: left to itself the engine
    writes a take into the directory its filename pattern names, which is not
    the project folder, and a project is meant to travel with its recordings
    (ADR 0005). This is where a take goes instead.
*/
struct RecordingBehaviour final : te::EngineBehaviour
{
    explicit RecordingBehaviour (const std::filesystem::path& directory)
        : recordingDirectory (&directory)
    {
    }

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

    te::Engine engine { "Duet",
                        nullptr,
                        std::make_unique<RecordingBehaviour> (recordingDirectory) };
    std::unique_ptr<te::Edit> edit;
    std::function<void()> projectChanged;

    juce::UndoManager& undoManager() const { return edit->getUndoManager(); }

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

    /** Puts the session on inputs and outputs that go nowhere, once. */
    void useHostedAudioDevice() const;

    /** True once the session is on them. */
    bool onHostedAudioDevice = false;

    /** Pushes blocks through the hosted device for a stretch of seconds,
        playing a signal into its inputs.
    */
    void pushBlocks (double seconds, const InputSignal& playedIn) const;

    //==============================================================================
    // Recording.

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

    /** The handle for a note, made on first sight of it. */
    NoteRef refForNote (ClipRef clip, const juce::ValueTree& noteState) const;

    /** The note a handle names, or null once it is gone for good. */
    te::MidiNote* noteFor (NoteRef ref) const;

    //==============================================================================
    // Declared last, so that it stops before anything it touches goes away.
    PlaybackKeeper playbackKeeper { *this };

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
} // namespace duet::model
