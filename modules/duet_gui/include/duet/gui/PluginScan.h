#pragma once

#include <duet/model/Session.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace duet::gui
{
/** The plugin-scan dialog, without the painting: what is being scanned, how far
    it has got, and what it found.

    A scan is stepped rather than run. Asking a plugin what it is means launching
    it in a child process and waiting for the answer, and what that costs is the
    plugin's business — so whoever draws this steps it between repaints, and the
    producer watches the scan instead of a frozen window. The child process is
    also what makes a crashing plugin a line in the results rather than the end
    of the session (spec b1j3me).

    What a finished scan means to the rest of the interface is the browser's VST3
    section: `onFinished` is what the host puts a `Browser::refresh` on, so a
    plugin the producer has just scanned is in the dock without a restart.
*/
class PluginScan
{
public:
    PluginScan() = default;
    ~PluginScan() = default;

    PluginScan (const PluginScan& other) = delete;
    PluginScan& operator= (const PluginScan& other) = delete;

    /** The project whose engine does the scanning, and nothing when none is
        open. A scan in progress ends here: the engine that was scanning is the
        project's.
    */
    void setSession (duet::model::Session* openProject);

    /** Where to look. Nothing set is the machine's own VST3 locations, which is
        what the producer means by Scan.
    */
    void setDirectories (std::vector<std::filesystem::path> folders);
    [[nodiscard]] std::vector<std::filesystem::path> directories() const;

    /** Called once when a scan ends, however it ended. */
    void onFinished (std::function<void()> callback);

    //==============================================================================
    /** Starts a scan of every directory. False when there is nothing to scan —
        no project, or no directory that exists.
    */
    bool start();

    /** Scans one plugin, and moves on to the next directory when one runs out.
        False when the scan is over, which is when `onFinished` has been called.
    */
    bool step();

    /** Stops where the scan stands. What it found up to there is kept: a
        producer who has seen the plugin they were waiting for need not sit
        through the rest.
    */
    void cancel();

    [[nodiscard]] bool isRunning() const { return scan != nullptr; }

    /** Whether a scan has been run to its end since this object was made. */
    [[nodiscard]] bool hasFinished() const { return finished; }

    /** How much of the whole scan has been done, from zero to one. */
    [[nodiscard]] double progress() const;

    /** The plugin about to be looked at, and empty when none is. */
    [[nodiscard]] std::filesystem::path scanningNow() const;

    /** Whether the plugins are asked what they are in a process of their own,
        which is what makes a crashing one survivable.
    */
    [[nodiscard]] bool scansOutOfProcess() const;

    //==============================================================================
    /** The VST3s the machine is known to have, after everything the scan has
        done so far.
    */
    [[nodiscard]] std::vector<duet::model::KnownPluginInfo> found() const;

    /** The modules the scan would not take: the ones that crashed the scanner,
        and the ones that opened and described no plugin. Reported rather than
        silently missing, so that a crashing plugin is something the producer can
        see.
    */
    [[nodiscard]] const std::vector<std::filesystem::path>& rejected() const
    {
        return rejectedFiles;
    }

private:
    /** Starts the next directory that can be scanned, and ends the scan when
        there is none.
    */
    bool openNextDirectory();

    void takeResultOfCurrent();
    void finish();

    duet::model::Session* session = nullptr;
    std::vector<std::filesystem::path> chosenDirectories;
    std::function<void()> finishedCallback;

    std::unique_ptr<duet::model::Vst3Scan> scan;

    /** The directories this run was started over, and how many of them are
        done: what a proportion of the whole scan is worked out from.
    */
    std::vector<std::filesystem::path> walking;
    std::size_t nextDirectory = 0;
    std::size_t finishedDirectories = 0;

    std::vector<std::filesystem::path> rejectedFiles;
    bool finished = false;
};
} // namespace duet::gui
