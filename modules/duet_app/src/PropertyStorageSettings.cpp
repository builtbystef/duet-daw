#include <duet/app/PropertyStorageSettings.h>

#include <duet/model/AppSettings.h>

namespace duet::app
{
struct PropertyStorageSettings::Impl
{
    duet::model::AppSettings settings;
};

PropertyStorageSettings::PropertyStorageSettings() : impl (std::make_unique<Impl>()) {}

PropertyStorageSettings::~PropertyStorageSettings() = default;

std::optional<std::string> PropertyStorageSettings::value (std::string_view key) const
{
    return impl->settings.value (key);
}

void PropertyStorageSettings::setValue (std::string_view key, std::string_view newValue)
{
    impl->settings.setValue (key, newValue);
}
} // namespace duet::app
