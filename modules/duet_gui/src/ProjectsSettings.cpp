#include <duet/gui/ProjectsSettings.h>

#include <duet/gui/Settings.h>

#include <string_view>

namespace duet::gui
{
namespace
{
    constexpr std::string_view projectsDirectoryKey = "projectsDirectory";
}

std::filesystem::path projectsDirectory (const Settings& settings,
                                         const std::filesystem::path& defaultDirectory)
{
    const auto stored = settings.value (projectsDirectoryKey);

    return stored.has_value() && ! stored->empty() ? std::filesystem::path { *stored }
                                                   : defaultDirectory;
}

void setProjectsDirectory (Settings& settings, const std::filesystem::path& directory)
{
    settings.setValue (projectsDirectoryKey, directory.string());
}
} // namespace duet::gui
