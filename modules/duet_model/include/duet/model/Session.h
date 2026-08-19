#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/** The edit vocabulary layer over the engine's Edit.

    This is the engine seam. Nothing in this module's public interface names an
    engine or JUCE type: refs are opaque integers, time is plain seconds and
    beats, paths are std::filesystem::path. A future engine swap replaces
    implementations, not callers.

    Which unit a number is in is part of its name. Positions on the timeline are
    seconds; positions inside a MIDI clip, and the lengths that have to stay put
    when the tempo moves, are beats.
*/
namespace duet::model
{
/** An opaque handle to a track, stable for as long as the track exists. */
using TrackRef = std::uint64_t;

/** An opaque handle to a clip, stable for as long as the clip exists. */
using ClipRef = std::uint64_t;

/** An opaque handle to a plugin in a track's chain. */
using PluginRef = std::uint64_t;

/** An opaque handle to a MIDI note.

    Unlike the others this one is Duet's own, and it lives for the session: the
    engine gives notes no durable identity, so the model hands out a handle the
    first time it is asked about a note and keeps it pointing at that note
    through every edit, including the undo that takes the note away and the redo
    that brings it back.
*/
using NoteRef = std::uint64_t;

/** No track: what an operation returns when it could not make one. */
inline constexpr TrackRef noTrack = 0;

/** No clip: what an operation returns when it could not make one. */
inline constexpr ClipRef noClip = 0;

/** No plugin: what an operation returns when it could not make one. */
inline constexpr PluginRef noPlugin = 0;

/** No note: what an operation returns when it could not make one. */
inline constexpr NoteRef noNote = 0;

class Session;

/** What a track is for.

    An audio track carries recordings and imports; a midi track carries MIDI
    clips into an instrument; a group is a bus other tracks are routed into, and
    is a track like any other, so that a mixer value or a plugin means the same
    thing on it.
*/
enum class TrackKind : std::uint8_t
{
    audio,
    midi,
    group
};

/** The plugins Duet ships: the engine's own devices under Duet's names.

    Milestone one's built-ins are engine-shipped (decision on spec b1j3me,
    2026-08-17), so no Duet-authored DSP runs in the audio callback yet.
*/
enum class BuiltinPlugin : std::uint8_t
{
    eq,
    compressor,
    reverb,
    synth,
    sampler
};

/** What a MIDI note looks like from outside the model. Beats count from the
    start of the clip that holds the note.
*/
struct NoteInfo
{
    NoteRef note = noNote;
    int pitch = 0;
    double startBeats = 0.0;
    double lengthBeats = 0.0;
    int velocity = 0;
};

/** What a clip looks like from outside the model. */
struct ClipInfo
{
    ClipRef clip = noClip;
    std::string name;
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;

    /** True when the clip repeats its content for its whole length. */
    bool looped = false;

    /** The length of one repeat, in beats. Zero when the clip is not looped. */
    double loopLengthBeats = 0.0;

    /** True for a clip that holds MIDI notes rather than an audio file. */
    bool holdsMidi = false;

    /** The clip's source reference exactly as the project stores it — relative
        to the project folder for a file inside it. Empty for a clip with no
        source file of its own, such as a MIDI clip.
    */
    std::string sourceReference;

    /** The file that source reference resolves to. */
    std::filesystem::path sourceFile;
};

/** What a plugin in a track's chain looks like from outside the model. */
struct PluginInfo
{
    PluginRef plugin = noPlugin;
    std::string name;

    /** Which built-in this is, or nothing for a plugin Duet does not ship —
        the engine's own fader and meter, and the external plugins to come.
    */
    std::optional<BuiltinPlugin> builtin;

    /** The track this plugin listens to for its sidechain, if it has one. */
    TrackRef sidechainSource = noTrack;
};

/** One of a plugin's parameters. The value is in the parameter's own units,
    which for a built-in are the real ones the producer sees.
*/
struct PluginParameterInfo
{
    std::string parameterId;
    std::string name;
    double value = 0.0;
    double minValue = 0.0;
    double maxValue = 0.0;
};

/** A track's send into a bus. */
struct SendInfo
{
    TrackRef bus = noTrack;
    double levelDb = 0.0;
};

/** What a track looks like from outside the model. */
struct TrackInfo
{
    TrackRef track = noTrack;
    std::string name;
    TrackKind kind = TrackKind::audio;

    /** The bus this track is routed into, or noTrack for the default output. */
    TrackRef output = noTrack;

    /** The fader level, in decibels: the explicit value, the one the producer
        chose and the one a save has to bring back.
    */
    double volumeDb = 0.0;

    /** The pan position: −1 hard left, 0 centre, +1 hard right. */
    double pan = 0.0;

    bool muted = false;
    bool soloed = false;

    std::vector<SendInfo> sends;
    std::vector<PluginInfo> plugins;
    std::vector<ClipInfo> clips;
};

/** What an automation curve belongs to.

    A curve is identified by what it drives, never by an index, because the
    curves a track owns come and go with its plugin chain.
*/
struct AutomationTarget
{
    enum class Kind : std::uint8_t
    {
        trackVolume,
        trackPan,
        pluginParameter
    };

    Kind kind = Kind::trackVolume;
    TrackRef track = noTrack;
    PluginRef plugin = noPlugin;
    std::string parameterId;

    [[nodiscard]] static AutomationTarget trackVolumeOf (TrackRef track);
    [[nodiscard]] static AutomationTarget trackPanOf (TrackRef track);
    [[nodiscard]] static AutomationTarget parameterOf (PluginRef plugin,
                                                       std::string_view parameterId);
};

/** One point on an automation curve.

    The value is in the same units the target reads back in — decibels for a
    track's volume, −1 to +1 for its pan, the parameter's own units for a
    plugin parameter — so that a point and the value it drives to are written
    the same way.
*/
struct AutomationPoint
{
    double timeSeconds = 0.0;
    double value = 0.0;
};

/** How many beats are in a bar, and what a beat is. */
/** A stretch of the timeline, in seconds. */
struct LoopRange
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
};

struct TimeSignature
{
    int numerator = 4;
    int denominator = 4;
};

/** The edit operations, callable only from inside an Action.

    Each operation writes through the project's undo history and never opens a
    transaction of its own, so a bare operation would merge into whatever
    transaction happens to be open, or land in an unnamed step that the engine's
    350 ms timer sealed. That is why the only way to hold an EditOps is to be
    handed one by Session::performAction: it has no public constructor, and it
    can be neither copied nor moved (ADR 0004).

    The transport is the deliberate exception and is not here: it lives on
    Session, is written with no undo history at all, and so can never be stopped
    or repositioned by an undo.
*/
class EditOps
{
public:
    EditOps (const EditOps&) = delete;
    EditOps (EditOps&&) = delete;
    EditOps& operator= (const EditOps&) = delete;
    EditOps& operator= (EditOps&&) = delete;

    //==============================================================================
    // Tracks and routing.

    /** Adds a track of a kind after the last one, and returns it.

        A midi track can be given one of the built-in instruments to play, which
        goes in at the head of its chain. An instrument asked for on an audio or
        group track is ignored: neither has anything to drive it.
    */
    TrackRef createTrack (TrackKind kind,
                          std::string_view name,
                          std::optional<BuiltinPlugin> instrument = {});

    /** Removes a track and everything on it. */
    void removeTrack (TrackRef track);

    void renameTrack (TrackRef track, std::string_view newName);

    /** Moves a track to an index among the audio tracks, counting from zero.
        An index past the last track moves it to the end.
    */
    void moveTrack (TrackRef track, int newIndex);

    /** Routes a track's output into a bus. noTrack sends it to the default
        output again.
    */
    void setTrackOutput (TrackRef track, TrackRef bus);

    //==============================================================================
    // Clips.

    /** Inserts an audio clip playing a source file, and returns it.

        The clip's source reference is pinned here, deliberately: left to the
        engine's default it resolves against a temporary directory and the clip
        plays silence. A file inside the project folder is stored relative to it.
    */
    ClipRef insertAudioClip (TrackRef track,
                             std::string_view name,
                             const std::filesystem::path& sourceFile,
                             double startSeconds,
                             double lengthSeconds);

    /** Inserts an empty MIDI clip, ready for notes, and returns it. */
    ClipRef insertMidiClip (TrackRef track,
                            std::string_view name,
                            double startSeconds,
                            double lengthSeconds);

    void moveClip (ClipRef clip, double newStartSeconds);
    void trimClip (ClipRef clip, double newLengthSeconds);
    void deleteClip (ClipRef clip);

    /** Makes a clip repeat one stretch of its content for its whole length, or
        stop repeating. The loop length is in beats, so a loop stays musical
        when the tempo moves.
    */
    void setClipLoop (ClipRef clip, bool looped, double loopLengthBeats);

    /** Copies a clip to a start time, on its own track or another one, and
        returns the copy. The copy keeps the source reference of the original,
        which is already relative to the project.
    */
    ClipRef duplicateClip (ClipRef clip, TrackRef toTrack, double startSeconds);

    //==============================================================================
    // MIDI notes. Beats count from the start of the clip that holds the note.

    NoteRef addNote (ClipRef clip, int pitch, double startBeats, double lengthBeats, int velocity);
    void removeNote (NoteRef note);

    /** Moves a note to another pitch and another place in its clip. */
    void moveNote (NoteRef note, int newPitch, double newStartBeats);

    void resizeNote (NoteRef note, double newLengthBeats);
    void setNoteVelocity (NoteRef note, int velocity);

    //==============================================================================
    // Mixer.

    /** Sets a track's fader level, in decibels: the explicit value, the one the
        producer chose and the one a save has to bring back.
    */
    void setTrackVolumeDb (TrackRef track, double db);

    /** Sets a track's pan: −1 hard left, 0 centre, +1 hard right. */
    void setTrackPan (TrackRef track, double pan);

    void setTrackMute (TrackRef track, bool muted);
    void setTrackSolo (TrackRef track, bool soloed);

    /** Sets how much of a track is sent into a bus, in decibels.

        The send and the bus's return are made on first use, so a Suggestion can
        name a bus that nothing has been sent to yet.
    */
    void setSend (TrackRef track, TrackRef bus, double levelDb);

    //==============================================================================
    // Plugin chains. A position is an index into the whole chain, the engine's
    // own fader and meter included: they are chain members like any other, and
    // hiding them would make a position mean one thing here and another in the
    // signal path.

    PluginRef addPlugin (TrackRef track, BuiltinPlugin plugin, int position);
    void removePlugin (PluginRef plugin);
    void reorderPlugin (PluginRef plugin, int newPosition);

    /** Sets a plugin parameter by value, in the parameter's own units. */
    void setPluginParameter (PluginRef plugin, std::string_view parameterId, double value);

    /** Points a plugin's sidechain at another track. noTrack clears it. */
    void setPluginSidechainSource (PluginRef plugin, TrackRef source);

    //==============================================================================
    // Automation.

    /** Puts points on a curve, one per time given: a time the curve already has
        a point at takes the new value, and every other time gains a point.
    */
    void setAutomationPoints (const AutomationTarget& target,
                              const std::vector<AutomationPoint>& points);

    /** Takes every point in a stretch of time off a curve, the points at both
        ends of the stretch included. The same time at both ends takes off the
        one point there.
    */
    void removeAutomationPoints (const AutomationTarget& target,
                                 double fromSeconds,
                                 double toSeconds);

    //==============================================================================
    // Project.

    void setTempo (double bpm);
    void setTimeSignature (int numerator, int denominator);

private:
    friend class Session;

    explicit EditOps (Session& owner) noexcept;
    ~EditOps() = default;

    Session& session;
};

/** One open project's model, and the transport that plays it.

    The message thread is the sole writer of the project model, so every member
    of this class is called from the message thread only.
*/
class Session
{
public:
    /** Opens an empty session on a project's edit file.

        Nothing is read and nothing is written: the path tells the model which
        file the project keeps its state in, and the folder that holds that file
        is the project folder, which clip source references inside it are stored
        relative to. Naming the file, and creating and opening real projects, is
        the persistence facade's work.
    */
    explicit Session (std::filesystem::path editFile);

    /** Opens a session on an edit file that is already there, and reads it.
        Null when there is nothing readable at that path.
    */
    [[nodiscard]] static std::unique_ptr<Session> openExisting (std::filesystem::path editFile);

    ~Session();

    Session (const Session&) = delete;
    Session& operator= (const Session&) = delete;

    /** Runs one Action: the only transaction boundary in the model.

        The operations run synchronously, under the given name, and land in the
        undo history as exactly one step no matter how many of them there are. A
        producer gesture and an accepted Suggestion both come through here, which
        is what makes their undo behaviour identical (ADR 0004).

        The Action stays open deliberately after the operations return, so that
        the engine's own deferred undo-tracked writes merge into the Action that
        caused them rather than into a step of their own.

        Throws std::logic_error when called from any thread but the message
        thread: the message thread is the sole writer of the project model.
    */
    void performAction (std::string_view name, const std::function<void (EditOps&)>& ops);

    /** Reverts the most recent Action. False when there is nothing to revert. */
    bool undo();

    /** Re-applies the most recently reverted Action. False when there is none. */
    bool redo();

    /** The names of the Actions that undo would revert, the next one first. */
    [[nodiscard]] std::vector<std::string> undoNames() const;

    /** The names of the Actions that redo would re-apply, the next one first. */
    [[nodiscard]] std::vector<std::string> redoNames() const;

    //==============================================================================
    /** The audio tracks of the project, in their running order. */
    [[nodiscard]] std::vector<TrackInfo> tracks() const;

    /** One track. A ref that names no track reads back as a track of nothing,
        whose own ref is noTrack.
    */
    [[nodiscard]] TrackInfo track (TrackRef ref) const;

    /** How many audio tracks the edit holds. */
    [[nodiscard]] int audioTrackCount() const;

    /** The notes in a MIDI clip, in the order they start. */
    [[nodiscard]] std::vector<NoteInfo> notes (ClipRef clip) const;

    /** A plugin's parameters, in the order the plugin declares them. */
    [[nodiscard]] std::vector<PluginParameterInfo> pluginParameters (PluginRef plugin) const;

    /** The points on an automation curve, in time order. Empty for a curve that
        does not exist, which is the same thing as one with no points on it.
    */
    [[nodiscard]] std::vector<AutomationPoint>
        automationPoints (const AutomationTarget& target) const;

    /** The edit's tempo, in beats per minute. */
    [[nodiscard]] double tempoBpm() const;

    /** The edit's time signature. */
    [[nodiscard]] TimeSignature timeSignature() const;

    /** How far the edit's content reaches, in seconds. Zero when it is empty. */
    [[nodiscard]] double editLengthSeconds() const;

    /** When a bar starts, in seconds. Bars count from one, as the producer
        counts them.
    */
    [[nodiscard]] double barStartSeconds (int bar) const;

    /** The fader level automation is driving a track to right now, in decibels.
        Equal to the track's volumeDb until playback hands the fader to a curve.
    */
    [[nodiscard]] double liveTrackVolumeDb (TrackRef track) const;

    /** Calls back after every change this session makes to the project — an
        Action, an undo, or a redo — so that the persistence facade can know the
        project has changes that are not on disk. One callback at a time.
    */
    void onProjectChanged (std::function<void()> callback);

    /** A digest of the whole project state, independent of the order in which
        its properties happen to be stored.

        Undo, redo and revert all preserve the state's meaning while permuting
        property order, so this — not the raw state — is how a test says two
        states are the same.
    */
    [[nodiscard]] std::string stateDigest() const;

    /** Renders the whole project to an audio file, offline and synchronously.
        False when nothing could be written.
    */
    bool renderToFile (const std::filesystem::path& destination);

    /** Fills the session with a short audible phrase, and makes it the state
        the project starts from.

        The walking skeleton's only content source: it exists so that the shell
        has something to play, and it goes away when the app can open a real
        project instead. The phrase is not undoable — it is where the project
        begins, and an undo that could take it away would be an undo of
        something the producer never did.
    */
    void loadDemoContent();

    //==============================================================================
    // The transport. Every one of these is written with no undo history at all,
    // so that an undo can never stop playback or move the playhead (ADR 0004).

    void startPlayback();
    void stopPlayback();
    [[nodiscard]] bool isPlaying() const;

    /** The transport position, in seconds from the start of the edit. */
    [[nodiscard]] double playbackPositionSeconds() const;

    /** Moves the playhead, whether or not the transport is rolling. */
    void setPlaybackPositionSeconds (double seconds);

    /** The stretch the transport loops over, in seconds.

        Seconds and not beats, because it is where the playhead goes and not
        anything musical — but a project's musical content moves in seconds when
        the tempo changes, so whoever sets a loop over a phrase has to set it
        again when the phrase moves under it.
    */
    void setLoopRangeSeconds (double startSeconds, double endSeconds);
    [[nodiscard]] LoopRange loopRangeSeconds() const;

    /** Whether the transport wraps at the end of the loop range. */
    void setLooping (bool shouldLoop);
    [[nodiscard]] bool isLooping() const;

    /** The current audio device, named with its sample rate, block size and
        output latency — empty when no device could be opened.
    */
    [[nodiscard]] std::string audioDeviceDescription() const;

private:
    friend class EditOps;

    // The one way through the engine seam, for the one module that needs it.
    // Its header is not on this module's public include path.
    friend struct EngineAccess;

    /** Tells the two constructors apart: one starts a project, one reads one. */
    struct FromFile
    {
    };

    Session (std::filesystem::path editFile, FromFile readIt);

    void startUndoHistory();

    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::model
