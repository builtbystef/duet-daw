#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/** The edit vocabulary layer over the engine's Edit.

    This is the engine seam. Nothing in this module's public interface names an
    engine or JUCE type: refs are opaque integers, time is plain seconds, paths
    are std::filesystem::path. A future engine swap replaces implementations,
    not callers.
*/
namespace duet::model
{
/** An opaque handle to a track, stable for as long as the track exists. */
using TrackRef = std::uint64_t;

/** An opaque handle to a clip, stable for as long as the clip exists. */
using ClipRef = std::uint64_t;

/** No track: what an operation returns when it could not make one. */
inline constexpr TrackRef noTrack = 0;

/** No clip: what an operation returns when it could not make one. */
inline constexpr ClipRef noClip = 0;

class Session;

/** What a clip looks like from outside the model. */
struct ClipInfo
{
    ClipRef clip = noClip;
    std::string name;
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;

    /** The clip's source reference exactly as the project stores it — relative
        to the project folder for a file inside it. Empty for a clip with no
        source file of its own, such as a MIDI clip.
    */
    std::string sourceReference;

    /** The file that source reference resolves to. */
    std::filesystem::path sourceFile;
};

/** What a track looks like from outside the model. */
struct TrackInfo
{
    TrackRef track = noTrack;
    std::string name;
    std::vector<ClipInfo> clips;
};

/** The edit operations, callable only from inside an Action.

    Each operation writes through the project's undo history and never opens a
    transaction of its own, so a bare operation would merge into whatever
    transaction happens to be open, or land in an unnamed step that the engine's
    350 ms timer sealed. That is why the only way to hold an EditOps is to be
    handed one by Session::performAction: it has no public constructor, and it
    can be neither copied nor moved (ADR 0004).
*/
class EditOps
{
public:
    EditOps (const EditOps&) = delete;
    EditOps (EditOps&&) = delete;
    EditOps& operator= (const EditOps&) = delete;
    EditOps& operator= (EditOps&&) = delete;

    /** Adds an audio track after the last one, and returns it. */
    TrackRef addTrack (std::string_view name);

    /** Removes a track and everything on it. */
    void removeTrack (TrackRef track);

    void renameTrack (TrackRef track, std::string_view newName);

    /** Moves a track to an index among the audio tracks, counting from zero.
        An index past the last track moves it to the end.
    */
    void moveTrack (TrackRef track, int newIndex);

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

    void moveClip (ClipRef clip, double newStartSeconds);
    void trimClip (ClipRef clip, double newLengthSeconds);
    void deleteClip (ClipRef clip);

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
    /** Opens an empty session for a project living in the given folder.

        Nothing is written to the folder: the path tells the model where the
        project's files are, so that clip source references inside it are stored
        relative to it. Creating and opening real projects is the persistence
        facade's work.
    */
    explicit Session (std::filesystem::path projectFolder);
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

    /** The audio tracks of the project, in their running order. */
    [[nodiscard]] std::vector<TrackInfo> tracks() const;

    /** How many audio tracks the edit holds. */
    [[nodiscard]] int audioTrackCount() const;

    /** The edit's tempo, in beats per minute. */
    [[nodiscard]] double tempoBpm() const;

    /** How far the edit's content reaches, in seconds. Zero when it is empty. */
    [[nodiscard]] double editLengthSeconds() const;

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

    /** Fills the session with a short audible phrase.

        The walking skeleton's only content source: it exists so that the shell
        has something to play, and it goes away when the app can open a real
        project instead.
    */
    void loadDemoContent();

    void startPlayback();
    void stopPlayback();
    [[nodiscard]] bool isPlaying() const;

    /** The transport position, in seconds from the start of the edit. */
    [[nodiscard]] double playbackPositionSeconds() const;

    /** The current audio device, named with its sample rate, block size and
        output latency — empty when no device could be opened.
    */
    [[nodiscard]] std::string audioDeviceDescription() const;

private:
    friend class EditOps;

    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::model
