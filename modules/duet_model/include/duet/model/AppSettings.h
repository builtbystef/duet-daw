#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace duet::model
{
/** The one app-global settings store, as everything outside the engine sees it.

    The machine's settings and not a project's: the interface's theme and scale
    beside the engine's own device choices and scanned plugin list, all in one
    `Settings.xml` under the user's configuration folder. One object holds that
    file open, because the file is written whole from the set its holder read —
    two holders write over each other, and the loser's keys are gone.

    So this is what a holder is. Every Engine a session makes is given a
    forwarding reference to the same store, and the shell's `duet::gui::Settings`
    is one of these; the store is made on first use and written out when the last
    holder — the shell's, or the last open session — goes away.

    String in, string out, and no engine type in sight: the store is the engine's
    own, and reaching it is what this module's seam is for.
*/
class AppSettings
{
public:
    AppSettings();
    ~AppSettings();

    AppSettings (const AppSettings&) = delete;
    AppSettings& operator= (const AppSettings&) = delete;

    /** What is stored under a key, or nothing when nobody has ever set it. */
    [[nodiscard]] std::optional<std::string> value (std::string_view key) const;

    /** Stores a value under a key and puts the whole store on disk, so that a
        setting the producer changes is written before the next thing that could
        end the process.
    */
    void setValue (std::string_view key, std::string_view newValue);

    /** The folder the store is kept in, under the user's configuration folder.

        It is the app-global place: anything else that belongs to the machine
        rather than to a project goes beside `Settings.xml` here, which is where
        the Collaborator's credentials are kept — never in a project folder, and
        never in anything the project's persistence owns (spec js437t).
    */
    [[nodiscard]] std::filesystem::path folder() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::model
