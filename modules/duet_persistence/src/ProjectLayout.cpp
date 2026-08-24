#include <duet/persistence/ProjectLayout.h>

namespace duet::persistence
{
namespace
{
    constexpr const char* audioDirectoryName = "audio";
    constexpr const char* editFileName = "project.tracktionedit";
    constexpr const char* partialSaveFileName = "project.tracktionedit.saving";
    constexpr const char* recoveryFileName = "project.tracktionedit.recovery";
    constexpr const char* partialRecoveryFileName = "project.tracktionedit.recovery.saving";
} // namespace

std::filesystem::path audioDirectory (const std::filesystem::path& projectFolder)
{
    return projectFolder / audioDirectoryName;
}

std::filesystem::path editFile (const std::filesystem::path& projectFolder)
{
    return projectFolder / editFileName;
}

std::filesystem::path partialSaveFile (const std::filesystem::path& projectFolder)
{
    return projectFolder / partialSaveFileName;
}

std::filesystem::path recoveryFile (const std::filesystem::path& projectFolder)
{
    return projectFolder / recoveryFileName;
}

std::filesystem::path partialRecoveryFile (const std::filesystem::path& projectFolder)
{
    return projectFolder / partialRecoveryFileName;
}
} // namespace duet::persistence
