#include <duet/gui/Rendering.h>

#include <duet/gui/Settings.h>

namespace duet::gui
{
namespace
{
    constexpr std::string_view enabledValue = "1";
    constexpr std::string_view disabledValue = "0";
} // namespace

bool hardwareAccelerationEnabled (const Settings& store)
{
    const auto stored = store.value (settingKey::hardwareAcceleration);

    return stored.has_value() && *stored == enabledValue;
}

void setHardwareAccelerationEnabled (Settings& store, bool shouldBeEnabled)
{
    store.setValue (settingKey::hardwareAcceleration,
                    shouldBeEnabled ? enabledValue : disabledValue);
}
} // namespace duet::gui
