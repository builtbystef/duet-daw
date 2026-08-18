#pragma once

#include <memory>
#include <string>

/** The edit vocabulary layer over the engine's Edit.

    This is the engine seam. Nothing in this module's public interface names an
    engine or JUCE type: refs are opaque integers, time is plain seconds, paths
    are std::filesystem::path. A future engine swap replaces implementations,
    not callers.
*/
namespace duet::model
{
/** One open project's model, and the transport that plays it.

    The message thread is the sole writer of the project model, so every member
    of this class is called from the message thread only.
*/
class Session
{
public:
    Session();
    ~Session();

    Session (const Session&) = delete;
    Session& operator= (const Session&) = delete;

    /** How many audio tracks the edit holds. */
    [[nodiscard]] int audioTrackCount() const;

    /** The edit's tempo, in beats per minute. */
    [[nodiscard]] double tempoBpm() const;

    /** How far the edit's content reaches, in seconds. Zero when it is empty. */
    [[nodiscard]] double editLengthSeconds() const;

    /** Fills the session with a short audible phrase.

        The walking skeleton's only content source: it exists so that the shell
        has something to play before the edit vocabulary (issue quiwf3) lands,
        and it goes away when that vocabulary can build content instead.
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
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::model
