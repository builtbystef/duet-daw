#include <duet/gui/PluginPresets.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using namespace std::string_view_literals;

TEST_CASE ("Duet plugin presets are app-global, sorted, and case-insensitively unique")
{
    duet::testing::StoredSettings settings;
    duet::gui::PluginPresets presets { settings };

    REQUIRE (presets.save ("vst3.example", "  Wide  ", "opaque\0state"sv, false)
             == duet::gui::SavePresetResult::saved);
    REQUIRE (presets.save ("vst3.example", "bright", "second", false)
             == duet::gui::SavePresetResult::saved);
    REQUIRE (presets.save ("vst3.example", "WIDE", "replacement", false)
             == duet::gui::SavePresetResult::alreadyExists);

    const auto saved = presets.presetsFor ("vst3.example");
    REQUIRE (saved.size() == 2);
    REQUIRE (saved[0].name == "bright");
    REQUIRE (saved[1].name == "Wide");
    REQUIRE (saved[1].opaqueState == "opaque\0state"sv);

    REQUIRE (presets.save ("vst3.example", " wide ", "replacement", true)
             == duet::gui::SavePresetResult::saved);
    const auto replaced = presets.preset ("vst3.example", "WIDE");
    REQUIRE (replaced.has_value());
    REQUIRE (replaced.value_or (duet::gui::PluginPreset {}).opaqueState == "replacement");
    REQUIRE (presets.save ("vst3.example", "   ", "state", false)
             == duet::gui::SavePresetResult::invalidName);
}
