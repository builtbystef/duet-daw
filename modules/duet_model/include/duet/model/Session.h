#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
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

/** An opaque handle to an input the project can record from.

    Duet's own, like a note's, and for the same reason: an input is a device of
    the machine and not a part of the project, so nothing in the project state
    hands out an identity for one. The model hands out a handle the first time
    it is asked about an input and keeps it pointing at that input for the
    session.
*/
using InputRef = std::uint64_t;

/** No track: what an operation returns when it could not make one. */
inline constexpr TrackRef noTrack = 0;

/** The stable opaque identity of the specialised Master mixer channel. It is
    deliberately not returned by tracks(): Master is not an arrangement track.
*/
inline constexpr TrackRef masterChannel = UINT64_MAX;

/** No clip: what an operation returns when it could not make one. */
inline constexpr ClipRef noClip = 0;

/** No plugin: what an operation returns when it could not make one. */
inline constexpr PluginRef noPlugin = 0;

/** No note: what an operation returns when it could not make one. */
inline constexpr NoteRef noNote = 0;

/** No input: what a track that records from nothing records from. */
inline constexpr InputRef noInput = 0;

/** What a meter reads where there is no signal at all, in decibels of full
    scale.
*/
inline constexpr double silentDb = -100.0;

/** The shape every offline render has, whatever machine it runs on: the rate
    its samples are in, and the block the engine cuts the timeline into.

    Both are stated here because what measures a render needs them. The block is
    also how early a rendered sound may begin — the engine starts a sound at the
    beginning of the block that contains it — so it is the tolerance any
    statement about *when* something happened in a render carries (ADR 0006).
*/
inline constexpr double renderSampleRate = 44100.0;
inline constexpr int renderBlockSize = 512;
inline constexpr double renderBlockSeconds = renderBlockSize / renderSampleRate;

/** The two parameters the engine gives every plugin Duet hosts, ahead of the
    ones the plugin itself declares: a dry and a wet level, and these are the ids
    it names them by.

    They are the engine's own and not the vendor's, so Duet states what they
    mean — a level in decibels, from silence to unity — where a hosted plugin's
    own parameters carry the normalised number the vendor speaks.
*/
inline constexpr const char* hostedDryLevelParameterId = "dry level";
inline constexpr const char* hostedWetLevelParameterId = "wet level";

inline constexpr double hostedLevelMinimumDb = silentDb;
inline constexpr double hostedLevelMaximumDb = 0.0;

/** The two ends of a fader's travel, in decibels: what the producer can set a
    track's level or a send's level to, and so what a Suggestion may ask for.

    Below the bottom is not a quieter fader but silence, which mute is for.
*/
inline constexpr double faderMinimumDb = -60.0;
inline constexpr double faderMaximumDb = 6.0;

class Session;
class Suggestion;
struct PluginEditorAccess;

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

/** One of the eight colours the Target Producer can assign to a track. */
enum class TrackColour : std::uint8_t
{
    orange,
    coral,
    mint,
    cyan,
    yellow,
    red,
    purple,
    blue
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

/** What an input carries. */
enum class InputKind : std::uint8_t
{
    audio,
    midi
};

/** An input the project can record from: a channel group of the machine's audio
    device, or one of its MIDI inputs.
*/
struct InputInfo
{
    InputRef input = noInput;
    std::string name;
    InputKind kind = InputKind::audio;
};

/** One of the machine's MIDI inputs, as the MIDI tab lists it.

    An input the producer has switched off is still here, switched off, which is
    what makes the list something they can switch back on. Only an enabled one
    can feed a track: `availableInputs` above lists what a track can record from,
    and a disabled input is not in it.
*/
struct MidiInputInfo
{
    InputRef input = noInput;
    std::string name;
    bool enabled = false;
};

/** What the machine's audio device is doing, as the Audio tab shows it.

    Empty and zero throughout when no device is open, which is what a machine
    with no audio hardware looks like and what a session that gave its device up
    looks like.
*/
struct AudioDeviceState
{
    std::string outputDevice;
    std::string inputDevice;
    double sampleRate = 0.0;
    int bufferSize = 0;

    /** How long a sample takes to reach the machine's output and to arrive from
        its input, in milliseconds: what the tab reports as the latency, and
        what the producer is choosing a buffer size against.
    */
    double outputLatencyMs = 0.0;
    double inputLatencyMs = 0.0;
};

/** What the producer asked the audio device to become.

    Every field is what they chose, and an empty name or a zero leaves that part
    of the device where it was: a producer who changes only the buffer size is
    not also choosing the device again.
*/
struct AudioDeviceChoice
{
    std::string outputDevice;
    std::string inputDevice;
    double sampleRate = 0.0;
    int bufferSize = 0;
};

/** One note played into a session's MIDI input while it runs with no audio
    device. Seconds count from the start of that run.
*/
struct InputNote
{
    double atSeconds = 0.0;
    double lengthSeconds = 0.0;
    int pitch = 0;
    int velocity = 0;
};

/** What is played into a session's inputs while it runs with no audio device:
    notes into the MIDI input, and a steady tone into every audio input channel.
*/
struct InputSignal
{
    std::vector<InputNote> notes;

    /** The tone's frequency, in hertz. Zero is silence. */
    double toneFrequencyHz = 0.0;

    /** How loud the tone is, as a sample value between zero and one. */
    double toneLevel = 0.5;
};

/** How much of what an input carries the producer hears while it is live. */
enum class InputMonitoring : std::uint8_t
{
    /** Never audible. */
    off,

    /** Audible while a track the input feeds is armed to record. */
    whileArmed,

    /** Always audible. */
    on
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

    /** How far into its content the clip starts. Trimming the left edge raises
        this by the same amount, keeping the content aligned to the timeline. */
    double contentOffsetSeconds = 0.0;

    /** A clip colour explicitly chosen by the producer, or no override when it
        follows its track's colour. */
    std::optional<TrackColour> colour;

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

/** What one of the producer's plugins looks like from outside the model. */
struct PluginInfo
{
    PluginRef plugin = noPlugin;
    std::string name;

    /** Which built-in this is, or nothing for a scanned external plugin. */
    std::optional<BuiltinPlugin> builtin;

    /** The app-global identifier of a scanned external plugin. Empty for a
        built-in.
    */
    std::string externalIdentifier;

    /** True when the project names an external plugin that is not available. */
    bool missing = false;

    /** The track this plugin listens to for its sidechain, if it has one. */
    TrackRef sidechainSource = noTrack;

    /** True when the plugin remains in the chain but does not process audio. */
    bool bypassed = false;

    /** How far the plugin delays what passes through it, in seconds.

        Seconds and not samples, because a delay in samples is only a number
        once a sample rate is chosen, and the project does not choose one: the
        rate belongs to whatever device or render the project is played through.
    */
    double latencySeconds = 0.0;
};

/** One of a plugin's parameters. A value whose meaning is Duet's is in the real
    units the producer sees; a scanned VST3's own is the plugin's normalised 0..1
    value, with its own display string beside it.

    Duet ships the built-ins, so it owns what their numbers mean and converts
    where the engine underneath holds something else: the compressor's ratio is
    4 for four to one, not the 0.25 stored, and its threshold is in decibels,
    not the gain stored. The two the engine adds to every hosted plugin are its
    own the same way, and are decibels here. The conversion is the same in both
    directions, so `setPluginParameter` takes back exactly what was read here,
    and an automation point on the parameter is on this same scale. Duet does
    not own a scanned plugin's own mapping and does not invent one: that value
    crosses raw.
*/
struct PluginParameterInfo
{
    std::string parameterId;
    std::string name;

    /** The value, and the two ends of what it may be — in the units below, so a
        write outside them is one the producer can see coming. It is held inside
        them rather than refused.
    */
    double value = 0.0;
    double minValue = 0.0;
    double maxValue = 0.0;

    /** How the value is spread over that range on a control that draws it: the
        proportion of the range raised to this is the position, so one is even
        and a skew below one lifts the small values away from the floor.

        Almost every parameter is even in the producer's ear and says so with
        one. A compressor's ratio is the exception its range forces: it reaches
        1000 to one, and drawn evenly every ratio anyone uses would sit on the
        floor. A caller that only reads or writes the number can ignore this; it
        is for the ones that draw the range, and an automation lane is the one
        that does.
    */
    double skew = 1.0;

    /** The plugin's own display string for this value. */
    std::string displayValue;

    /** What the value is measured in — "dB", "Hz", "ms", ":1" for a ratio.

        Empty says plainly that the number is not measured in anything: a plain
        number such as a filter's Q, a named choice such as a reverb's freeze,
        or a scanned plugin's normalised value, whose meaning is the vendor's
        and reaches the Collaborator as an estimated display string instead.
    */
    std::string unit;

    /** Whether Duet states what this parameter means, which is what decides the
        shape it crosses the Tool Vocabulary in.

        True of every parameter of a plugin Duet ships, and of the dry and wet
        levels the engine adds to every plugin it hosts: those two are the
        engine's own, so their name, their unit and their number are facts about
        Duet's own dependency. False of a hosted plugin's own parameters, where
        the only thing that says what the number means is the vendor's display
        string, and that crosses as an Estimate.
    */
    bool duetOwnsMeaning = true;
};

/** What a plugin answered when it was asked about its parameters, and whether
    it answered at all.

    Asking a hosted plugin what one of its values means is asking the plugin,
    and a plugin is free to raise instead of answering (engine notes). This
    facade puts no engine type across it, and an exception thrown inside a
    hosted plugin is one, so the raise stops at the seam: `wereRead` is false,
    `held` is empty, and what to say about a plugin that would not answer is the
    caller's to decide.

    A plugin with no parameters at all reads back empty too, and `wereRead` is
    what tells the two apart. A ref that names no plugin is neither: nothing was
    asked, so nothing refused.
*/
struct PluginParameterRead
{
    std::vector<PluginParameterInfo> held;
    bool wereRead = true;
};

/** A track's send into a bus. */
struct SendInfo
{
    TrackRef bus = noTrack;
    double levelDb = 0.0;
};

/** What a track looks like from outside the model. */
struct MasterInfo
{
    TrackRef channel = masterChannel;
    double volumeDb = 0.0;
    double pan = 0.0;
    bool muted = false;
    std::vector<PluginInfo> plugins;
};

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
    TrackColour colour = TrackColour::orange;

    /** The input this track records from, or noInput for a track that records
        from nothing.
    */
    InputRef input = noInput;

    /** True when the next record takes what this track's input carries. */
    bool recordArmed = false;

    std::vector<SendInfo> sends;

    /** The producer's plugin chain, in order. The plugins Duet puts on a track
        for its own reasons are not in it — the fader and meter every track is
        born with, the return that puts a send into a bus, and the send that
        feeds it — so an index into this list is the position addPlugin and
        reorderPlugin count in.
    */
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

    /** How the segment leaving this point bends on its way to the next, from
        −1 to +1, and zero for the straight line milestone one draws.

        Every point states this rather than leaving it to be assumed, so that
        the curved segments of a later milestone are a value that changes and
        not a shape the stored points never had.
    */
    double curvature = 0.0;
};

/** A VST3 the app knows how to insert. The identifier is app-global and
    stable across a restart; the project itself persists the plugin's own VST3
    identity and state.
*/
struct KnownPluginInfo
{
    std::string identifier;
    std::string name;
    std::string manufacturer;
    std::filesystem::path file;
    bool isInstrument = false;
    bool isAvailable = false;
};

/** What an export is written as.

    The three the interface offers: two uncompressed, and one that compresses
    without losing anything. Nothing lossy is here — an export is what the
    producer takes away, and a lossy master is something they make from it.
*/
enum class ExportFormat : std::uint8_t
{
    wav,
    aiff,
    flac
};

/** How loud a normalised export is brought to, in decibels of full scale.

    A hair under full scale rather than at it: the peak sample is what
    normalising places, and a converter reconstructing the waveform between two
    samples can go above the loudest of them.
*/
inline constexpr double exportNormaliseTargetDb = -0.3;

/** What an export of the project is: where it goes, what it is written as, and
    what stretch of the timeline it holds.

    Every one of these is the producer's own choice, which is what makes an
    export different from the renders above: those have the one shape a
    measurement needs (ADR 0006), and this has the shape that was asked for.

    The stretch is in seconds, both ends counted from the start of the project,
    and an export whose end is not after its start writes nothing.
*/
struct ExportOptions
{
    std::filesystem::path destination;
    ExportFormat format = ExportFormat::wav;
    int bitDepth = 24;
    double sampleRate = renderSampleRate;
    double startSeconds = 0.0;
    double endSeconds = 0.0;

    /** Brings the loudest sample of the export to the target level. Off leaves
        the levels where the project put them.
    */
    bool normalise = false;
    double normaliseToDb = exportNormaliseTargetDb;
};

/** How far an export has got, from zero to one, and whether it should go on.

    Asked between blocks, so a producer who pressed Cancel waits for one block
    rather than for the export. What a cancelled export leaves behind is
    nothing: a half-written file would be read as a whole one.
*/
using ExportProgress = std::function<bool (double proportion)>;

/** The result of scanning one directory for VST3 plugins.

    A crashing plugin is a completed scan with that file in badFiles: the child
    scanner died, not the app. failedFiles are modules that opened without a
    crash but described no plugin.
*/
struct PluginScanResult
{
    bool completed = false;
    std::vector<std::filesystem::path> failedFiles;
    std::vector<std::filesystem::path> badFiles;
};

/** A scan of one directory for VST3 plugins, taken one plugin at a time.

    Scanning a plugin means launching it in a child process and waiting for what
    it says about itself, and what that costs is the plugin's business. So a scan
    is not one call: whoever is showing it steps it, draws what it says about the
    plugin it is about to look at, and steps it again — which is what makes the
    producer a watcher of a scan rather than the owner of a frozen window.

    The scan is the app-global plugin list being filled in, so it lives as long
    as the object does and what it found is kept whether or not it was run to the
    end.
*/
class Vst3Scan
{
public:
    ~Vst3Scan();

    Vst3Scan (const Vst3Scan& other) = delete;
    Vst3Scan& operator= (const Vst3Scan& other) = delete;

    /** The plugin the next step will look at, and empty when there is none
        left.
    */
    [[nodiscard]] std::filesystem::path nextPlugin() const;

    /** How much of the directory has been looked at, from zero to one. */
    [[nodiscard]] double progress() const;

    /** Scans one plugin. False when there was nothing left to scan, which is
        also what says the scan is over.
    */
    bool step();

    /** What the scan has found so far: the modules that described no plugin,
        and the ones the child scanner died on. Complete once the walk has
        ended.
    */
    [[nodiscard]] PluginScanResult result() const;

private:
    friend class Session;

    struct Impl;

    explicit Vst3Scan (std::unique_ptr<Impl> made);

    std::unique_ptr<Impl> impl;
};

/** A stretch of the timeline, in seconds. */
struct LoopRange
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
};

/** How many beats are in a bar, and what a beat is. */
struct TimeSignature
{
    int numerator = 4;
    int denominator = 4;
};

/** One named stretch of the arrangement — an intro, a drop, an outro.

    Bars count from one, and both ends are in it: an eight-bar intro at the
    start of the project runs from bar 1 to bar 8.
*/
struct SectionInfo
{
    std::string name;
    int startBar = 1;
    int endBar = 1;
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

    /** Copies a track and everything on it after the last track. */
    TrackRef duplicateTrack (TrackRef track);

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

    /** Moves a clip in time and, when given, to another track. */
    void moveClip (ClipRef clip, TrackRef toTrack, double newStartSeconds);

    void trimClip (ClipRef clip, double newLengthSeconds);

    /** Trims either edge while keeping the clip's content fixed to timeline
        time. */
    void trimClip (ClipRef clip, double newStartSeconds, double newLengthSeconds);

    void renameClip (ClipRef clip, std::string_view newName);
    void setClipColour (ClipRef clip, TrackColour colour);
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

    /** masterChannel addresses the specialised Master for volume, pan and mute. */
    void setTrackMute (TrackRef track, bool muted);
    void setTrackSolo (TrackRef track, bool soloed);
    void setTrackColour (TrackRef track, TrackColour colour);

    /** Sets how much of a track is sent into a bus, in decibels.

        The send and the bus's return are made on first use, so a Suggestion can
        name a bus that nothing has been sent to yet.
    */
    void setSend (TrackRef track, TrackRef bus, double levelDb);

    //==============================================================================
    // Plugin chains. A position counts the producer's plugins and nothing else,
    // so that position zero is first among the producer's effects rather than in
    // front of the return that feeds a bus. It is the same index space that
    // TrackInfo::plugins reads back in, and a position outside the chain is
    // clamped to its ends. The plugins Duet puts on a track for its own reasons
    // keep their places around the producer's, wherever a position lands.

    PluginRef addPlugin (TrackRef track, BuiltinPlugin plugin, int position);

    /** Adds a scanned VST3 by the identifier knownVst3Plugins returned. */
    PluginRef addPlugin (TrackRef track, std::string_view knownPluginIdentifier, int position);

    void removePlugin (PluginRef plugin);
    void reorderPlugin (PluginRef plugin, int newPosition);
    void setPluginBypassed (PluginRef plugin, bool bypassed);

    /** Restores a hosted plugin's opaque state. Invalid data is ignored. */
    void setPluginOpaqueState (PluginRef plugin, std::string_view opaqueState);

    /** Sets a plugin parameter by the value `pluginParameters` reads back: real
        units where Duet owns the parameter's meaning, and a scanned VST3's own
        normalised 0..1 range where it does not.
    */
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

    /** States the arrangement's named sections, replacing whatever it had.

        The whole list at once, like an automation curve's points: a section is
        only meaningful beside the others, and stating them together is what
        keeps a rename and a resize from being two Actions.
    */
    void setSections (const std::vector<SectionInfo>& sections);

    /** States the key the project is in — "F minor", "C", whatever the
        producer calls it — and an empty key is a project that declares none.

        Duet does not parse it. A key is what the producer says the music is in,
        and a tool that reads it back reads it back as a fact for that reason.
    */
    void setKey (std::string_view key);

private:
    friend class Session;

    explicit EditOps (Session& owner) noexcept;
    ~EditOps() = default;

    Session& session;
};

/** A reference to an item an earlier operation in the same Suggestion creates.

    Placeholder refs exist only inside the ordered operation list. They are
    resolved afresh each time the Suggestion is auditioned or accepted, so a
    later operation never depends on an engine ID that did not exist when the
    Suggestion was made.
*/
struct SuggestionRef
{
    std::uint64_t value = 0;
};

/** Either an item already in the project or one an earlier operation creates. */
using SuggestionTarget = std::variant<std::uint64_t, SuggestionRef>;

/** An automation target carried by Suggestion data. */
struct SuggestionAutomationTarget
{
    AutomationTarget::Kind kind = AutomationTarget::Kind::trackVolume;
    SuggestionTarget item = std::uint64_t { 0 };
    std::string parameterId;

    [[nodiscard]] static SuggestionAutomationTarget trackVolumeOf (SuggestionTarget track);
    [[nodiscard]] static SuggestionAutomationTarget trackPanOf (SuggestionTarget track);
    [[nodiscard]] static SuggestionAutomationTarget parameterOf (SuggestionTarget plugin,
                                                                 std::string_view parameterId);
};

/** A pending Suggestion: a name and an ordered list of edit operations.

    It is data only. Building one does not touch a Session, its project file, or
    its undo history. The Collaborator layer owns the pending data; the model
    only applies it transiently for Audition or as one accepted Action.
*/
class Suggestion
{
public:
    explicit Suggestion (std::string name);
    ~Suggestion();

    Suggestion (const Suggestion& other);
    Suggestion& operator= (const Suggestion& other);
    Suggestion (Suggestion&&) noexcept;
    Suggestion& operator= (Suggestion&&) noexcept;

    [[nodiscard]] const std::string& name() const;

    /** Appends a track creation and returns the placeholder later operations
        use to name that track.
    */
    SuggestionRef createTrack (TrackKind kind,
                               std::string_view name,
                               std::optional<BuiltinPlugin> instrument = {});
    SuggestionRef duplicateTrack (SuggestionTarget track);
    void removeTrack (SuggestionTarget track);
    void renameTrack (SuggestionTarget track, std::string_view newName);
    void moveTrack (SuggestionTarget track, int newIndex);
    void setTrackOutput (SuggestionTarget track, SuggestionTarget bus);

    SuggestionRef insertAudioClip (SuggestionTarget track,
                                   std::string_view name,
                                   std::filesystem::path sourceFile,
                                   double startSeconds,
                                   double lengthSeconds);
    SuggestionRef insertMidiClip (SuggestionTarget track,
                                  std::string_view name,
                                  double startSeconds,
                                  double lengthSeconds);
    void moveClip (SuggestionTarget clip, double newStartSeconds);

    /** Moves a clip in time and to another track. */
    void moveClip (SuggestionTarget clip, SuggestionTarget toTrack, double newStartSeconds);

    void trimClip (SuggestionTarget clip, double newLengthSeconds);

    /** Trims either edge while keeping the clip's content fixed to timeline
        time. */
    void trimClip (SuggestionTarget clip, double newStartSeconds, double newLengthSeconds);
    void deleteClip (SuggestionTarget clip);
    void setClipLoop (SuggestionTarget clip, bool looped, double loopLengthBeats);
    SuggestionRef
        duplicateClip (SuggestionTarget clip, SuggestionTarget toTrack, double startSeconds);

    SuggestionRef addNote (SuggestionTarget clip,
                           int pitch,
                           double startBeats,
                           double lengthBeats,
                           int velocity);
    void removeNote (SuggestionTarget note);
    void moveNote (SuggestionTarget note, int newPitch, double newStartBeats);
    void resizeNote (SuggestionTarget note, double newLengthBeats);
    void setNoteVelocity (SuggestionTarget note, int velocity);

    void setTrackVolumeDb (SuggestionTarget track, double db);
    void setTrackPan (SuggestionTarget track, double pan);
    void setTrackMute (SuggestionTarget track, bool muted);
    void setTrackSolo (SuggestionTarget track, bool soloed);
    void setTrackColour (SuggestionTarget track, TrackColour colour);
    void setSend (SuggestionTarget track, SuggestionTarget bus, double levelDb);

    SuggestionRef addPlugin (SuggestionTarget track, BuiltinPlugin plugin, int position);
    SuggestionRef
        addPlugin (SuggestionTarget track, std::string_view knownPluginIdentifier, int position);
    void removePlugin (SuggestionTarget plugin);
    void reorderPlugin (SuggestionTarget plugin, int newPosition);
    void setPluginParameter (SuggestionTarget plugin, std::string_view parameterId, double value);
    void setPluginSidechainSource (SuggestionTarget plugin, SuggestionTarget source);

    void setAutomationPoints (SuggestionAutomationTarget target,
                              std::vector<AutomationPoint> points);
    void removeAutomationPoints (SuggestionAutomationTarget target,
                                 double fromSeconds,
                                 double toSeconds);

    void setTempo (double bpm);
    void setTimeSignature (int numerator, int denominator);

    /** Appends a copy of another Suggestion's operations to this one, renaming
        that one's placeholders so that neither list can resolve the other's.

        This is what cherry-picking is made of. An Element of a Suggestion is
        applicable on its own, so any set of Elements is one operation list, and
        the Action that applies it is one Action however many Elements went into
        it.
    */
    void append (const Suggestion& other);

private:
    friend class Session;

    struct Impl;
    std::unique_ptr<Impl> impl;
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

    /** Starts the engine's plugin-scan worker when a process was launched with
        its private scan command line. False for an ordinary app or test run.

        The application calls this before making a window. A test executable
        that can be launched as the scanner calls it before handing its command
        line to the test runner.
    */
    static bool startPluginScanChild (std::string_view commandLine);

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
    // Suggestions and Audition.

    /** Applies a pending Suggestion transiently to the real project, with no
        producer undo history. False when another Suggestion is already live.
    */
    bool auditionSuggestion (const Suggestion& suggestion);

    /** Reverts a live Audition exactly. A call while idle does nothing. */
    void stopAudition();

    /** True while the transient suggested state is the state being heard. */
    [[nodiscard]] bool isAuditioning() const;

    /** Reverts any live Audition, then applies the Suggestion as one named
        Action. One undo therefore removes every operation.
    */
    bool acceptSuggestion (const Suggestion& suggestion);

    /** Discards a pending Suggestion. If it is live, reverts it first. */
    void rejectSuggestion (const Suggestion& suggestion);

    //==============================================================================
    /** The audio tracks of the project, in their running order. */
    [[nodiscard]] std::vector<TrackInfo> tracks() const;

    /** One track. A ref that names no track reads back as a track of nothing,
        whose own ref is noTrack.
    */
    [[nodiscard]] TrackInfo track (TrackRef ref) const;

    /** The specialised Master mixer channel. It is not an arrangement track. */
    [[nodiscard]] MasterInfo master() const;

    /** How many audio tracks the edit holds. */
    [[nodiscard]] int audioTrackCount() const;

    /** The notes in a MIDI clip, in the order they start. */
    [[nodiscard]] std::vector<NoteInfo> notes (ClipRef clip) const;

    /** A plugin's parameters, in the order the plugin declares them.

        Empty for a plugin with no parameters, and empty for one that would not
        say what it has; `readPluginParameters` tells the two apart, and every
        caller that only wants the parameters can ignore the difference.
    */
    [[nodiscard]] std::vector<PluginParameterInfo> pluginParameters (PluginRef plugin) const;

    /** The same read, with the plugin's refusal to answer told from its having
        nothing to answer. The one place a hosted plugin is asked what its
        values mean, and so the one place such a plugin's raise can be stopped.
    */
    [[nodiscard]] PluginParameterRead readPluginParameters (PluginRef plugin) const;

    /** What one of the built-ins has, before the project holds one of it.

        Duet ships them, so what a built-in's parameters are is a fact that does
        not wait on an instance in a chain: the ids, the two ends, the skew and
        the units are the ones an added instance reports, read off a plugin made
        and thrown away here rather than off a second table that could drift
        from the first. The value and the display string are the plugin's own
        defaults, which is all an untouched plugin has to say about itself.

        Empty for a built-in with no automatable parameters, which the engine's
        sampler is.
    */
    [[nodiscard]] std::vector<PluginParameterInfo>
        builtinPluginParameters (BuiltinPlugin plugin) const;

    /** A hosted plugin's current opaque state. Empty for an unavailable or
        engine-built plugin.
    */
    [[nodiscard]] std::string pluginOpaqueState (PluginRef plugin) const;

    /** The points on an automation curve, in time order. Empty for a curve that
        does not exist, which is the same thing as one with no points on it.
    */
    [[nodiscard]] std::vector<AutomationPoint>
        automationPoints (const AutomationTarget& target) const;

    /** What a curve is driving its target to at a time, in the units its points
        are written in. A curve with no points reads back the value the producer
        set by hand, which is what the target plays where no curve takes it over.
    */
    [[nodiscard]] double automationValueAt (const AutomationTarget& target,
                                            double timeSeconds) const;

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

    /** Which bar a moment falls in, counting from one, and fractional through
        the bar: the start of bar 5 is 5.0 and its middle is 5.5.

        The inverse of barStartSeconds, and what turns a length in seconds into
        one in bars — the difference between the two ends.
    */
    [[nodiscard]] double barAtSeconds (double seconds) const;

    /** How many beats into the project a moment is, counting from zero. */
    [[nodiscard]] double beatsAtSeconds (double seconds) const;

    /** When a beat falls, in seconds: the exact inverse of beatsAtSeconds.

        The reads publish a time in beats and an edit takes one in seconds, so
        this is what turns the one back into the other.
    */
    [[nodiscard]] double secondsAtBeats (double beats) const;

    /** When a bar falls, in seconds: the inverse of barAtSeconds, and
        barStartSeconds for a bar that is not a whole one.

        Part of the way through a bar is that proportion of the bar's own
        length, which is exact for every tempo the project can hold, there
        being one of them.
    */
    [[nodiscard]] double secondsAtBar (double bar) const;

    /** The arrangement's named sections, in the order the producer put them. */
    [[nodiscard]] std::vector<SectionInfo> sections() const;

    /** The key the project declares it is in, and empty when it declares none.
    */
    [[nodiscard]] std::string key() const;

    /** The fader level automation is driving a track to right now, in decibels.
        Equal to the track's volumeDb until playback hands the fader to a curve.
    */
    [[nodiscard]] double liveTrackVolumeDb (TrackRef track) const;

    /** Applies an audible mixer preview with no Action, dirty state or undo.
        A gesture restores the original through this seam before committing its
        final value as one Action.
    */
    void previewVolumeDb (TrackRef channel, double db);
    void previewPan (TrackRef channel, double pan);

    /** Calls back after every change this session makes to the project — an
        Action, an undo, or a redo — so that the persistence facade can know the
        project has changes that are not on disk. One callback at a time.
    */
    void onProjectChanged (std::function<void()> callback);

    /** Calls back with each notice the engine addresses to the producer — a
        refused paste, a transport that cannot do what was asked — so the
        interface can show it as one line in its own chrome. Without a handler
        the notice is dropped; the engine's default was a bubble pinned to
        whatever component sat under the mouse, one more per retry. Message
        thread, one callback at a time.
    */
    void onEngineMessage (std::function<void (const std::string&)> callback);

    /** A digest of the whole project state, independent of the order in which
        its properties happen to be stored.

        Undo, redo and revert all preserve the state's meaning while permuting
        property order, so this — not the raw state — is how a test says two
        states are the same.
    */
    [[nodiscard]] std::string stateDigest() const;

    /** A message-thread revision advanced by each Action, undo, and redo. */
    [[nodiscard]] std::uint64_t revision() const noexcept;

    /** A digest of everything about one track that decides what it renders: its
        own state, and the tempo map its clips are placed against.

        What a measurement of a track can be cached against, in other words. An
        edit to another track leaves it where it was, which is the point: what a
        track puts out on its own does not change because a neighbour did. The
        master's digest is the whole project's, because the master is the whole
        project. An empty string for a track that is not there.
    */
    [[nodiscard]] std::string trackStateDigest (TrackRef track) const;

    /** Renders the whole project to an audio file, offline and synchronously.
        False when nothing could be written.

        The render has one shape whatever machine it runs on — 44100 Hz, 512
        samples to a block, 32 bits deep, no dithering — because ADR 0006
        asserts a render's measured features and a feature needs a known shape
        to be measured against. The block size is also how early the engine may
        place a note: it starts one at the beginning of the block that contains
        it.

        The file is what the producer hears: a muted track is silent in it, and
        a soloed track is the only thing in it. Mute and solo are the project,
        and a render of the whole project is the project.

        Offline renders belong on a worker thread, and this may be called from
        one as long as the message loop is running, because the engine builds
        the render graph on the message thread and waits for it. The destination
        is written afresh: the engine answers a repeated render from its
        audio-file cache, keyed on the destination, so a render that must be a
        render asks for a file of its own.
    */
    bool renderToFile (const std::filesystem::path& destination,
                       const std::function<bool()>& keepGoing = {});

    /** Renders one track of the project to an audio file, the same way, and
        without the master chain: what a track puts out on its own, whatever
        else the project holds. False for a track that is not there.

        This is what measured analysis of a track is made of, so it isolates:
        no other track reaches the file, and neither does another track's solo.

        `keepGoing` is asked between blocks, and a render it says no to stops
        where it is and writes nothing: an analysis nobody is waiting for any
        more is work worth abandoning. The same holds of the whole-project
        render above.
    */
    bool renderTrackToFile (TrackRef track,
                            const std::filesystem::path& destination,
                            const std::function<bool()>& keepGoing = {});

    /** The same two renders, made off a detached copy of the project, so that
        the project itself never stops.

        An offline render frees the playback context of the Edit it renders and
        keeps it freed until the render ends, so an Edit that is rendering is an
        Edit that is not playing — and a render costs seconds. These two put a
        copy of the project under the render instead: the producer goes on
        playing and recording the project throughout, and the file is the same
        file, because the copy is the project's own state.

        The copy costs what it costs to build — every plugin the render reaches
        is instantiated a second time — so this is the render for a measurement
        made while the producer works, and `renderToFile` above is still the
        render for one they asked for and are waiting on.

        The same thread rules as above: called from a worker thread, with the
        message loop running, because the copy is made and taken down on the
        message thread. False when the copy could not be made.
    */
    bool renderDetachedToFile (const std::filesystem::path& destination,
                               const std::function<bool()>& keepGoing = {});

    bool renderDetachedTrackToFile (TrackRef track,
                                    const std::filesystem::path& destination,
                                    const std::function<bool()>& keepGoing = {});

    /** Writes a stretch of the project to a file the producer asked for, in the
        format, depth and rate they asked for. False when nothing was written,
        which is also what a cancelled export answers.

        The render is made off a detached copy, like the two above it, and for a
        reason the producer can hear: an export is not a gap in the session.
        The transport goes on rolling, the undo history is where it was, and the
        project has no more unsaved changes afterwards than it had before —
        exporting is a read of the project, not an edit of it.

        Belongs on a worker thread, with the message loop running, on the same
        terms as the renders above: the copy is made and taken down on the
        message thread, and the blocks in between are the worker's.
    */
    bool exportToFile (const ExportOptions& options, const ExportProgress& progress = {});

    //==============================================================================
    // External plugins.

    /** Whether this build can host VST3 plugins. */
    [[nodiscard]] bool canHostVst3() const;

    /** Whether plugin metadata is scanned in a child process. Hosting itself is
        in-process.
    */
    [[nodiscard]] bool scansPluginsOutOfProcess() const;

    /** Scans one directory recursively for VST3 plugins. Known good results and
        bad files are retained in the app-global plugin list.

        The whole scan, in one call, for a caller with nothing to draw while it
        runs. What the interface starts is the stepped one below.
    */
    [[nodiscard]] PluginScanResult scanVst3Plugins (const std::filesystem::path& directory);

    /** The same scan, to be stepped a plugin at a time by whoever is showing it.
        Nothing when this build cannot host VST3 at all, or when the directory is
        not one.
    */
    [[nodiscard]] std::unique_ptr<Vst3Scan> beginVst3Scan (const std::filesystem::path& directory);

    /** Where VST3 plugins live on this machine, as the format itself says: what
        a scan the producer did not point anywhere looks in.
    */
    [[nodiscard]] std::vector<std::filesystem::path> vst3Directories() const;

    /** Every VST3 retained by scanning, including one whose file has since gone
        missing.
    */
    [[nodiscard]] std::vector<KnownPluginInfo> knownVst3Plugins() const;

    //==============================================================================
    // The meters: what playback is putting out.
    //
    // The engine builds one graph to play through and another to render offline,
    // and they route differently — a track with nowhere to go is summed into the
    // master by the rendering one and blocked by the playing one. So a measured
    // render says nothing about what a producer hears, and these are the reads
    // that do (ADR 0006).

    /** The loudest the output has been since this was last asked, in decibels
        of full scale.

        Full scale is 0 dB, so a peak at or above it is a signal with no
        headroom left — the clipping the producer hears — and silence reads as
        silentDb. Asking clears the reading, the way a meter falls back once it
        has been looked at.
    */
    [[nodiscard]] double outputPeakDb();

    /** The same reading for one track, taken where the engine puts a track's
        meter — after its fader, and before whatever the track is routed into
        has done anything with what it hands on. A track that is not there reads
        as silentDb.

        A track that reads loud while the output reads silent is a track routed
        into something that nobody can hear.
    */
    [[nodiscard]] double trackPeakDb (TrackRef track);

    /** Gives up the audio device, and takes inputs and outputs that go nowhere
        in its place — from here on, for this session.

        What playWithoutAudioDevice does for itself on first use, done in
        advance and deliberately, because a track cannot be armed to record from
        an input that does not exist and a machine with no audio hardware has
        none. It is what puts recording in CI (ADR 0006).
    */
    void useNoAudioDevice();

    /** Stops the engine rebuilding its device list on its own four-second
        timer.

        Hazard 6: DeviceManager::initialise starts that timer, and the rebuild
        frees every playback graph. A test that is not about the rebuild, or
        that wants to drive it itself, says so here. The producer path never
        calls this.

        Does not ask for the device list. Asking from the constructor was tried
        and destabilised the headless record path.
    */
    void suppressDeviceRebuild();

    /** Rebuilds the device list now, the way the engine would four seconds
        into a session, and frees the playback graph, which is what that
        rebuild does to a rolling transport (hazard 6).

        A test that needs the rebuild to have happened — or to happen to a
        rolling transport — says so here, instead of pumping the message loop
        until the engine's timer lands.

        Returns once the engine has built its list and gone quiet about it, and
        so promises the same thing on a loaded machine as on an idle one. It
        gives up after two seconds, which is still well under the engine's own
        four-second timer.
    */
    void rebuildDevices();

    /** How long the engine's devices have to have been unchanged before a
        take starts on them, how often a waiting take looks, and how many of
        those looks it takes before the take starts regardless.

        The production defaults are 100 ms, 20 ms, and 100 attempts. A test
        that is about the wait itself drives these so it spends no real
        seconds on them.
    */
    void setDeviceWait (int quietMilliseconds, int pollMilliseconds, int attempts);

    /** How often the model asks a transport that is not rolling to play, and
        how many of those asks it makes before it accepts the answer.

        The production defaults are 100 ms and 100 attempts, which is the ten
        seconds hazard 6 needs. A test that is about what happens when that
        window runs out drives these so it spends no real seconds on them.
    */
    void setPlayRetry (int intervalMilliseconds, int attempts);

    /** Runs a stretch of the project's audio with no audio device, as fast as
        the machine will go, playing a signal into its inputs.

        Whatever the transport is doing it keeps doing: this is the blocks going
        through and not the transport, so a take is started, played into, and
        stopped around it.
    */
    void runWithoutAudioDevice (double seconds, const InputSignal& playedIn = {});

    /** Plays a stretch of the project with no audio device, as fast as the
        machine will go, and stops. True when it played at all.

        The graph is the playback graph — the one a producer hears, and the one
        an offline render does not build — but the blocks it fills go nowhere,
        so the meters are the only trace playing this way leaves. It is how a
        machine with no audio hardware can still be asked what reaches the
        output, which is what puts that question in CI.

        Playing starts at the playhead and leaves it where it stopped. A session
        that has played this way has given up its audio device to do it, and
        keeps playing without one.
    */
    bool playWithoutAudioDevice (double seconds);

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
    // Recording.
    //
    // Which input a track records from, and whether it is armed, are the
    // producer's standing answer to where the next take comes from — not a
    // change to what the project holds. So they are written with no undo
    // history, like the transport, and an undo can never disarm a track in the
    // middle of a take. What a take puts into the project is an Action, and
    // that is stopRecording's business.

    /** Says where takes are written.

        The model knows which folder a project is and not what shape it has —
        that is the persistence facade's (ADR 0005) — so whoever opened the
        project says which directory inside it recordings go into. Until
        something says, a take lands beside the edit file.
    */
    void setRecordingDirectory (std::filesystem::path directory);

    /** The inputs this machine offers, in the order the engine lists them. */
    [[nodiscard]] std::vector<InputInfo> availableInputs() const;

    /** Records a track from an input; noInput records it from nothing.

        A track records from one input and an input feeds one track, so this
        takes the input away from whatever it fed before.
    */
    void setTrackInput (TrackRef track, InputRef input);

    /** Arms a track: the next record takes what its input carries. */
    void setTrackRecordArmed (TrackRef track, bool armed);

    /** How much of what an input carries the producer hears while it is live.

        A property of the input and not of a track, because it is the machine's
        signal that is being listened to: an input feeds one track, so the two
        readings are the same one.
    */
    void setInputMonitoring (InputRef input, InputMonitoring monitoring);
    [[nodiscard]] InputMonitoring inputMonitoring (InputRef input) const;

    /** Starts recording: every armed track takes what its input carries, from
        the playhead on.

        Unlike startPlayback this cannot ask twice. The engine rebuilds its
        devices in the opening seconds of a session and the rebuild ends a take
        it lands in, but asking again would start a second take at the playhead
        rather than continue the first — so the rebuild is got out of the way
        first instead. Where it has not happened yet the session asks the engine
        for it and the take starts on the far side of it, a pre-roll of
        milliseconds that an ordinary Record never waits out at all.

        So a take may begin very slightly after this returns, and isRecording
        says which: it is the transport recording, not the asking for it.
    */
    void startRecording();

    /** Stops recording and lands the take: one Action named "Record Take".

        One undo takes the take's clips away again. The files they were recorded
        into stay on disk, which is what makes the redo bring back the same
        audio and not silence.
    */
    void stopRecording();

    [[nodiscard]] bool isRecording() const;

    //==============================================================================
    // The transport. Every one of these is written with no undo history at all,
    // so that an undo can never stop playback or move the playhead (ADR 0004).

    /** Starts playback, and keeps it started.

        One call is a producer pressing Play once: the engine rebuilds its
        device list a few seconds into the first playback of a session and
        stops the transport doing it, so the session remembers that playback
        was asked for and asks again, until the transport rolls or the asking
        runs out. Every caller gets this; none has to know about it.
    */
    void startPlayback();

    /** Stops playback, and stops asking for it. */
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

    /** Whether the engine's click track sounds with playback. Like the other
        transport controls, this never enters the producer's undo history. */
    void setMetronomeEnabled (bool enabled);
    [[nodiscard]] bool metronomeEnabled() const;

    /** The audio device's published process load, as a proportion. Reading it
        takes no lock and does no work on the audio callback. */
    [[nodiscard]] double cpuLoad() const noexcept;

    /** The current audio device, named with its sample rate, block size and
        output latency — empty when no device could be opened.
    */
    [[nodiscard]] std::string audioDeviceDescription() const;

    //==============================================================================
    // The machine's audio hardware, as the Audio tab of the Settings window sets
    // it. All of it is app-global: it is the producer's machine and not their
    // project, so none of it is written into the project or its undo history.

    /** The devices this machine offers, in the order its driver lists them. */
    [[nodiscard]] std::vector<std::string> availableOutputDevices() const;
    [[nodiscard]] std::vector<std::string> availableInputDevices() const;

    /** The rates and buffer sizes the device that is open can be run at. Both
        are empty when no device is open, there being nothing to ask.
    */
    [[nodiscard]] std::vector<double> availableSampleRates() const;
    [[nodiscard]] std::vector<int> availableBufferSizes() const;

    /** What the device is doing right now, latency included. */
    [[nodiscard]] AudioDeviceState audioDevice() const;

    /** Opens the device the producer chose, and returns empty.

        What comes back otherwise is what went wrong, in the driver's own words,
        and then nothing changed: the device that was running is still running.
        A producer who chose a device their machine cannot open keeps the one
        they could hear.
    */
    std::string setAudioDevice (const AudioDeviceChoice& choice);

    /** Every MIDI input the machine has, switched on or off. */
    [[nodiscard]] std::vector<MidiInputInfo> midiInputs() const;

    /** Switches a MIDI input on or off. A switched-off input reaches no track:
        it leaves `availableInputs`, and nothing it carries is played or
        recorded.
    */
    void setMidiInputEnabled (InputRef input, bool enabled);

private:
    friend class EditOps;

    // The narrow crossings through the engine seam. Their headers are not on
    // this module's public include path.
    friend struct EngineAccess;
    friend struct PluginEditorAccess;

    /** Tells the two constructors apart: one starts a project, one reads one. */
    struct FromFile
    {
    };

    Session (std::filesystem::path editFile, FromFile readIt);

    void startUndoHistory();
    void applySuggestion (const Suggestion& suggestion);

    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::model
