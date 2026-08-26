#pragma once

#include <duet/gui/Settings.h>

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

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

private:
    std::filesystem::path projectFolder;
};

/** Runs the message loop for approximately this many milliseconds, so that the
    engine's deferred work — the async clip re-sort above all — can land.
*/
void pumpMessages (int milliseconds);

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
