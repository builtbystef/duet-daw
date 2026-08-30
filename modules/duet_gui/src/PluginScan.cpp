#include <duet/gui/PluginScan.h>

#include <algorithm>
#include <utility>

namespace duet::gui
{
void PluginScan::setSession (duet::model::Session* openProject)
{
    // The engine that was scanning is the project's, so a scan does not outlive
    // the project it was started in.
    cancel();

    session = openProject;
}

void PluginScan::setDirectories (std::vector<std::filesystem::path> folders)
{
    chosenDirectories = std::move (folders);
}

std::vector<std::filesystem::path> PluginScan::directories() const
{
    if (! chosenDirectories.empty())
        return chosenDirectories;

    return session != nullptr ? session->vst3Directories() : std::vector<std::filesystem::path> {};
}

void PluginScan::onFinished (std::function<void()> callback)
{
    finishedCallback = std::move (callback);
}

//==============================================================================
bool PluginScan::start()
{
    if (session == nullptr)
        return false;

    cancel();

    walking = directories();
    nextDirectory = 0;
    finishedDirectories = 0;
    rejectedFiles.clear();
    finished = false;

    return openNextDirectory();
}

bool PluginScan::step()
{
    if (scan == nullptr)
        return false;

    if (scan->step())
        return true;

    // This directory is done. What it found is taken before the next one is
    // opened, the blacklist being app-global and read per directory.
    takeResultOfCurrent();
    ++finishedDirectories;
    scan.reset();

    return openNextDirectory();
}

void PluginScan::cancel()
{
    if (scan == nullptr)
        return;

    takeResultOfCurrent();
    scan.reset();
    finish();
}

//==============================================================================
double PluginScan::progress() const
{
    if (walking.empty())
        return finished ? 1.0 : 0.0;

    const auto whole = static_cast<double> (walking.size());
    const auto done = static_cast<double> (finishedDirectories);
    const auto inHand = scan != nullptr ? scan->progress() : 0.0;

    return std::min (1.0, (done + inHand) / whole);
}

std::filesystem::path PluginScan::scanningNow() const
{
    return scan != nullptr ? scan->nextPlugin() : std::filesystem::path {};
}

bool PluginScan::scansOutOfProcess() const
{
    return session != nullptr && session->scansPluginsOutOfProcess();
}

std::vector<duet::model::KnownPluginInfo> PluginScan::found() const
{
    return session != nullptr ? session->knownVst3Plugins()
                              : std::vector<duet::model::KnownPluginInfo> {};
}

//==============================================================================
bool PluginScan::openNextDirectory()
{
    while (nextDirectory < walking.size())
    {
        const auto folder = walking[nextDirectory++];

        scan = session->beginVst3Scan (folder);

        if (scan != nullptr)
            return true;

        // A directory the machine no longer has is not a scan that failed: it is
        // one directory fewer to walk.
        ++finishedDirectories;
    }

    finish();

    return false;
}

void PluginScan::takeResultOfCurrent()
{
    if (scan == nullptr)
        return;

    const auto result = scan->result();

    const auto remember = [this] (const std::vector<std::filesystem::path>& files)
    {
        for (const auto& file : files)
            if (std::ranges::find (rejectedFiles, file) == rejectedFiles.end())
                rejectedFiles.push_back (file);
    };

    remember (result.badFiles);
    remember (result.failedFiles);
}

void PluginScan::finish()
{
    finished = true;

    if (finishedCallback)
        finishedCallback();
}
} // namespace duet::gui
