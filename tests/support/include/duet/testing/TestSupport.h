#pragma once

#include <duet/gui/Settings.h>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace duet::model
{
class Session;
}

/** Test-only helpers shared by every suite.

    This header follows the same engine-free rule as the facades it supports, so
    a test that uses it still cannot reach an engine or JUCE type.
*/
namespace duet::testing
{
/** The app-global settings store, held in memory.

    A second reader over the same one is the next launch: nothing about a restart
    matters to a test except that the store outlives what reads it.
*/
class StoredSettings final : public duet::gui::Settings
{
public:
    [[nodiscard]] std::optional<std::string> value (std::string_view key) const override
    {
        const auto found = values.find (std::string { key });

        return found == values.end() ? std::nullopt : std::optional { found->second };
    }

    void setValue (std::string_view key, std::string_view newValue) override
    {
        values[std::string { key }] = std::string { newValue };
    }

private:
    std::map<std::string, std::string> values;
};

/** A project folder under the system temp directory, deleted with this object.

    A bare folder of the right shape, for suites that want one without going
    through the persistence facade, and the room to make real projects in: a
    path below folder() is a fresh project folder that this object cleans up.
*/
class TempProject
{
public:
    TempProject();
    ~TempProject();

    TempProject (const TempProject&) = delete;
    TempProject& operator= (const TempProject&) = delete;

    [[nodiscard]] const std::filesystem::path& folder() const { return projectFolder; }

    /** The edit file a session opened on this folder edits. */
    [[nodiscard]] std::filesystem::path editFile() const;

    /** Writes a sine tone into the project's audio subdirectory, and returns
        the file it wrote.
    */
    [[nodiscard]] std::filesystem::path
        writeTone (std::string_view fileName, double lengthSeconds, double frequencyHz) const;

    /** Writes a sequence of chords into that same place, each one the MIDI
        pitches it is made of sounding together as sines, and each lasting the
        same time: what a progression is, for a suite that asks what key or what
        chords something is in.
    */
    [[nodiscard]] std::filesystem::path
        writeChords (std::string_view fileName,
                     double secondsPerChord,
                     const std::vector<std::vector<int>>& chords) const;

private:
    std::filesystem::path projectFolder;
};

/** Keeps this process's message loop up for as long as this object lives.

    A `Session` carries the JUCE initialiser the loop belongs to, so the last
    Session put down takes the loop with it. The application holds an
    initialiser of its own for its whole life and never meets that; a case that
    puts a project down from inside the loop — which is where a project a tool
    call was still inside is put down — holds one too, or the loop is destroyed
    while it is running.
*/
class MessageLoop
{
public:
    MessageLoop();
    ~MessageLoop();

    MessageLoop (const MessageLoop&) = delete;
    MessageLoop (MessageLoop&&) = delete;
    MessageLoop& operator= (const MessageLoop&) = delete;
    MessageLoop& operator= (MessageLoop&&) = delete;

private:
    std::shared_ptr<void> initialiser;
};

/** Runs the message loop for approximately this many milliseconds, so that the
    engine's deferred work — the async clip re-sort above all — can land.
*/
void pumpMessages (int milliseconds);

/** Runs one piece of work on the message thread and waits for it to have run.

    This is what the Collaborator service's own thread marshals a project read
    through: the message thread is the sole writer of the project model, so a
    tool reads the authoritative state by going there. The suite supplies it
    because the Collaborator's own module links no JUCE and cannot.

    Whoever calls it from another thread owes the message loop a pump, or
    nothing there will run. It gives up after a while rather than waiting for
    ever, so a suite that forgot fails an assertion instead of hanging.
*/
using MessageThreadCall = std::function<void (const std::function<void()>&)>;
[[nodiscard]] MessageThreadCall messageThreadMarshal();

/** Leaves one piece of work on the message thread and returns at once.

    The other half of what anything outside the message thread needs of it, and
    what a Task Run's events cross on: a run must never hold the Collaborator
    service's thread up, so what it has to say is left here rather than waited
    for. Whoever calls it owes the message loop a pump, like the marshal above.
*/
using MessageThreadPost = std::function<void (std::function<void()>)>;
[[nodiscard]] MessageThreadPost messageThreadPost();

/** How long pumpUntil runs the message loop before it gives up on a condition.

    Under the four seconds of the engine's own device-list rebuild timer, so
    that nothing waiting here can be answered by that timer instead of by what
    it was waiting for — and far enough over what deferred work costs on an idle
    machine that only a broken condition reaches it.
*/
constexpr int pumpUntilTimeoutMs = 2000;

/** Runs the message loop until a condition holds, and says whether it did.

    What an assertion about deferred work is about is the work happening, not
    the number of milliseconds a machine needed to get there: a pump long enough
    on an idle machine is a coin toss on a loaded one. This returns as soon as
    the condition holds, so it costs an idle machine what a short pump costs and
    still gives a loaded one room.

    It says nothing about how soon the condition held, so an assertion that
    something must *not* have happened yet stays a separate assertion, made
    before this one.
*/
bool pumpUntil (const std::function<bool()>& condition,
                int timeoutMilliseconds = pumpUntilTimeoutMs);

/** Starts playback, and runs the message loop until the transport is rolling.

    The asking is the model's own: `Session::startPlayback` keeps asking a
    transport that is not rolling, which is how it survives hazard 6 — the
    engine's one-time device-list rebuild, which frees the playback graph and
    stops the transport seconds into the first playback of a session. What a
    headless test still owes it is a running message loop, since that asking
    happens on a timer, and that is what this helper is for.
*/
bool playUntilRolling (duet::model::Session&);

/** The peak sample level of an audio file: zero when the file is silent, and
    zero when it cannot be read.
*/
[[nodiscard]] double peakLevelOf (const std::filesystem::path& audioFile);

/** The peak sample level over one stretch of an audio file, in seconds from its
    start: what a feature assertion about a tail, a fade or a gap is made of.
*/
[[nodiscard]] double
    peakLevelBetween (const std::filesystem::path& audioFile, double fromSeconds, double toSeconds);
} // namespace duet::testing
