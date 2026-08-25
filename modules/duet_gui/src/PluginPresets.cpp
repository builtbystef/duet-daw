#include <duet/gui/PluginPresets.h>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace duet::gui
{
namespace
{
    constexpr std::string_view settingsKey = "pluginPresetLibrary";

    std::string trim (std::string_view text)
    {
        constexpr std::string_view whitespace = " \t\n\r\f\v";
        const auto first = text.find_first_not_of (whitespace);
        if (first == std::string_view::npos)
            return {};
        const auto last = text.find_last_not_of (whitespace);
        return std::string { text.substr (first, last - first + 1) };
    }

    std::string folded (std::string_view text)
    {
        std::string result { text };
        std::transform (result.begin(),
                        result.end(),
                        result.begin(),
                        [] (unsigned char character)
                        { return static_cast<char> (std::tolower (character)); });
        return result;
    }

    std::string hexadecimal (std::string_view text)
    {
        constexpr std::string_view digits = "0123456789abcdef";
        std::string result;
        result.reserve (text.size() * 2);
        for (const auto character : text)
        {
            const auto byte = static_cast<unsigned char> (character);
            result.push_back (digits.at (byte >> 4));
            result.push_back (digits.at (byte & 0x0f));
        }
        return result;
    }

    std::optional<std::string> fromHexadecimal (std::string_view text)
    {
        if (text.size() % 2 != 0)
            return std::nullopt;
        const auto value = [] (char character) -> int
        {
            if (character >= '0' && character <= '9')
                return character - '0';
            if (character >= 'a' && character <= 'f')
                return character - 'a' + 10;
            if (character >= 'A' && character <= 'F')
                return character - 'A' + 10;
            return -1;
        };
        std::string result;
        result.reserve (text.size() / 2);
        for (std::size_t index = 0; index < text.size(); index += 2)
        {
            const auto high = value (text[index]);
            const auto low = value (text[index + 1]);
            if (high < 0 || low < 0)
                return std::nullopt;
            result.push_back (static_cast<char> ((high << 4) | low));
        }
        return result;
    }
} // namespace

std::vector<PluginPreset> PluginPresets::all() const
{
    std::vector<PluginPreset> result;
    const auto stored = settings->value (settingsKey);
    if (! stored.has_value())
        return result;

    std::istringstream lines { *stored };
    std::string line;
    while (std::getline (lines, line))
    {
        std::vector<std::string_view> fields;
        std::string_view remaining { line };
        for (int index = 0; index < 3; ++index)
        {
            const auto separator = remaining.find ('|');
            if (separator == std::string_view::npos)
                break;
            fields.push_back (remaining.substr (0, separator));
            remaining.remove_prefix (separator + 1);
        }
        fields.push_back (remaining);
        if (fields.size() != 4 || fields[0] != "1")
            continue;
        const auto identity = fromHexadecimal (fields[1]);
        const auto name = fromHexadecimal (fields[2]);
        const auto state = fromHexadecimal (fields[3]);
        if (identity.has_value() && name.has_value() && state.has_value())
            result.push_back ({ 1, *identity, *name, *state });
    }
    return result;
}

void PluginPresets::writeAll (const std::vector<PluginPreset>& presets)
{
    std::string stored;
    for (const auto& preset : presets)
        stored += "1|" + hexadecimal (preset.pluginIdentity) + "|" + hexadecimal (preset.name) + "|"
                  + hexadecimal (preset.opaqueState) + "\n";
    settings->setValue (settingsKey, stored);
}

std::vector<PluginPreset> PluginPresets::presetsFor (std::string_view identity) const
{
    auto presets = all();
    std::erase_if (presets,
                   [identity] (const auto& preset) { return preset.pluginIdentity != identity; });
    std::sort (presets.begin(),
               presets.end(),
               [] (const auto& first, const auto& second)
               { return folded (first.name) < folded (second.name); });
    return presets;
}

std::optional<PluginPreset> PluginPresets::preset (std::string_view identity,
                                                   std::string_view name) const
{
    const auto wanted = folded (name);
    for (auto& candidate : presetsFor (identity))
        if (folded (candidate.name) == wanted)
            return candidate;
    return std::nullopt;
}

SavePresetResult PluginPresets::save (std::string_view identity,
                                      std::string_view name,
                                      std::string_view opaqueState,
                                      bool replaceExisting)
{
    const auto cleanName = trim (name);
    if (identity.empty() || cleanName.empty())
        return SavePresetResult::invalidName;

    auto presets = all();
    const auto wantedName = folded (cleanName);
    const auto found = std::find_if (presets.begin(),
                                     presets.end(),
                                     [&] (const auto& candidate) {
                                         return candidate.pluginIdentity == identity
                                                && folded (candidate.name) == wantedName;
                                     });
    if (found != presets.end() && ! replaceExisting)
        return SavePresetResult::alreadyExists;

    const PluginPreset replacement {
        1, std::string { identity }, cleanName, std::string { opaqueState }
    };
    if (found != presets.end())
        *found = replacement;
    else
        presets.push_back (replacement);
    writeAll (presets);
    return SavePresetResult::saved;
}
} // namespace duet::gui
