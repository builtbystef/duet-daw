#pragma once

#include <filesystem>

namespace duet::gui
{
class Settings;

/** The folder new projects are created under. A project that is already open
    keeps its own folder; changing this setting affects only later projects.
*/
[[nodiscard]] std::filesystem::path
    projectsDirectory (const Settings& settings, const std::filesystem::path& defaultDirectory);

/** Changes where later projects are created. */
void setProjectsDirectory (Settings& settings, const std::filesystem::path& directory);
} // namespace duet::gui
