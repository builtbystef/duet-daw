#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace duet::collab
{
/** The sidecar as a child process: spawn it, watch it, end it.

    Nothing here waits on a process that is still running. `running()` reaps a
    child that has exited, which is how a sidecar killed from outside stops
    counting as one, and `terminate()` is the only call that blocks — briefly,
    and only while a process it has signalled goes away.
*/
class SidecarProcess
{
public:
    SidecarProcess() = default;
    ~SidecarProcess();

    SidecarProcess (const SidecarProcess&) = delete;
    SidecarProcess& operator= (const SidecarProcess&) = delete;

    /** Starts the executable with those arguments. False when it could not be
        started at all — a path that is not there, or a machine out of processes.
    */
    bool spawn (const std::filesystem::path& executable, const std::vector<std::string>& arguments);

    /** Whether a child is still alive, reaping it if it is not. */
    [[nodiscard]] bool running();

    [[nodiscard]] std::optional<int> processId() const { return childId; }

    /** Asks the child to end, and kills it if it will not, within the grace
        period. Returns once no child is left.
    */
    void terminate (std::chrono::milliseconds grace);

private:
    std::optional<int> childId;
};
} // namespace duet::collab
