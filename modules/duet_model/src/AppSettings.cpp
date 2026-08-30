#include <duet/model/AppSettings.h>

#include "AppSettingsStore.h"

namespace duet::model
{
struct AppSettings::Impl
{
    juce::SharedResourcePointer<DuetPropertyStorage> storage;
};

AppSettings::AppSettings() : impl (std::make_unique<Impl>()) {}
AppSettings::~AppSettings() = default;

std::optional<std::string> AppSettings::value (std::string_view key) const
{
    const juce::String name { std::string { key } };
    auto& file = impl->storage->getPropertiesFile();

    if (! file.containsKey (name))
        return std::nullopt;

    return file.getValue (name).toStdString();
}

std::filesystem::path AppSettings::folder() const
{
    return std::filesystem::path {
        impl->storage->getAppPrefsFolder().getFullPathName().toStdString()
    };
}

void AppSettings::setValue (std::string_view key, std::string_view newValue)
{
    auto& file = impl->storage->getPropertiesFile();

    file.setValue (juce::String { std::string { key } }, juce::String { std::string { newValue } });
    file.save();
}
} // namespace duet::model
