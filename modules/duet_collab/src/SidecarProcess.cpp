#include "SidecarProcess.h"

#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace duet::collab
{
namespace
{
    /** How often terminate() looks again while it waits for a child to go. */
    constexpr auto reapInterval = std::chrono::milliseconds { 10 };
} // namespace

SidecarProcess::~SidecarProcess() { terminate (std::chrono::milliseconds { 1000 }); }

bool SidecarProcess::spawn (const std::filesystem::path& executable,
                            const std::vector<std::string>& arguments)
{
    auto program = executable.string();

    std::vector<std::string> owned;
    owned.reserve (arguments.size() + 1);
    owned.push_back (program);
    owned.insert (owned.end(), arguments.begin(), arguments.end());

    std::vector<char*> argv;
    argv.reserve (owned.size() + 1);

    for (auto& argument : owned)
        argv.push_back (argument.data());

    argv.push_back (nullptr);

    // Every descriptor this process holds is opened close-on-exec, so the child
    // inherits none of them — least of all the listening socket, which it would
    // otherwise keep alive after the DAW had closed it.
    pid_t spawned = 0;

    if (::posix_spawn (&spawned, program.c_str(), nullptr, nullptr, argv.data(), environ) != 0)
        return false;

    childId = static_cast<int> (spawned);

    return true;
}

bool SidecarProcess::running()
{
    if (! childId.has_value())
        return false;

    int status = 0;
    const auto reported = ::waitpid (static_cast<pid_t> (*childId), &status, WNOHANG);

    if (reported == 0)
        return true;

    childId.reset();

    return false;
}

void SidecarProcess::terminate (std::chrono::milliseconds grace)
{
    if (! running() || ! childId.has_value())
        return;

    const auto child = static_cast<pid_t> (*childId);

    ::kill (child, SIGTERM);

    for (auto waited = std::chrono::milliseconds { 0 }; waited < grace; waited += reapInterval)
    {
        if (! running())
            return;

        std::this_thread::sleep_for (reapInterval);
    }

    if (! running())
        return;

    ::kill (child, SIGKILL);

    int status = 0;
    ::waitpid (child, &status, 0);
    childId.reset();
}
} // namespace duet::collab
