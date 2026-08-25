#pragma once

#include <duet/gui/Settings.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace duet::gui
{
struct PluginPreset
{
    int formatVersion = 1;
    std::string pluginIdentity;
    std::string name;
    std::string opaqueState;
};

enum class SavePresetResult : std::uint8_t
{
    saved,
    invalidName,
    alreadyExists
};

/** The app-global Duet preset library. Its one settings value contains a
    versioned, escaped record stream, so presets belong to the installation and
    never dirty a project or enter its undo history.
*/
class PluginPresets
{
public:
    explicit PluginPresets (Settings& store) : settings (&store) {}

    [[nodiscard]] std::vector<PluginPreset> presetsFor (std::string_view identity) const;
    [[nodiscard]] std::optional<PluginPreset> preset (std::string_view identity,
                                                      std::string_view name) const;
    SavePresetResult save (std::string_view identity,
                           std::string_view name,
                           std::string_view opaqueState,
                           bool replaceExisting);

private:
    [[nodiscard]] std::vector<PluginPreset> all() const;
    void writeAll (const std::vector<PluginPreset>& presets);

    Settings* settings = nullptr;
};
} // namespace duet::gui
