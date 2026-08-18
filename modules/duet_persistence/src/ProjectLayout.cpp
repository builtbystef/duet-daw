#include <duet/persistence/ProjectLayout.h>

namespace duet::persistence
{
namespace
{
    constexpr const char* audioDirectoryName = "audio";
} // namespace

std::filesystem::path audioDirectory (const std::filesystem::path& projectFolder)
{
    return projectFolder / audioDirectoryName;
}
} // namespace duet::persistence
