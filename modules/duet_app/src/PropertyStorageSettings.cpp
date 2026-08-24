#include "PropertyStorageSettings.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace duet::app
{
struct PropertyStorageSettings::Impl
{
    te::PropertyStorage storage { "Duet" };
};

PropertyStorageSettings::PropertyStorageSettings() : impl (std::make_unique<Impl>())
{
    auto& file = impl->storage.getPropertiesFile();
    file.reload();
    file.setValue ("duetApplicationVersion", JUCE_APPLICATION_VERSION_STRING);
    file.save();
}

PropertyStorageSettings::~PropertyStorageSettings() { impl->storage.getPropertiesFile().save(); }

std::optional<std::string> PropertyStorageSettings::value (std::string_view key) const
{
    const auto name = juce::String::fromUTF8 (key.data(), static_cast<int> (key.size()));
    auto& file = impl->storage.getPropertiesFile();

    if (! file.containsKey (name))
        return std::nullopt;

    return file.getValue (name).toStdString();
}

void PropertyStorageSettings::setValue (std::string_view key, std::string_view newValue)
{
    auto& file = impl->storage.getPropertiesFile();

    // The engine has a properties file of its own open on this same path while a
    // project is open. A reload merges whatever it has written since onto ours,
    // so the save below writes the two sets together rather than one over the
    // other.
    file.reload();
    file.setValue (juce::String::fromUTF8 (key.data(), static_cast<int> (key.size())),
                   juce::String::fromUTF8 (newValue.data(), static_cast<int> (newValue.size())));
    file.save();
}
} // namespace duet::app
