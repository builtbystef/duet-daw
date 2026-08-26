#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <memory>

/** The one app-global settings store, and the adapter that lends it to an
    Engine.

    Off the module's public include path, like everything else engine-shaped:
    `AppSettings.h` is what the rest of Duet holds one of.
*/
namespace duet::model
{
namespace te = tracktion;

/** What the store is called: the folder under the user's configuration folder,
    and the name every holder of the store answers with.
*/
inline constexpr const char* appSettingsName = "Duet";

/** The app-global engine settings store — the one `juce::PropertiesFile` Duet
    opens on `Settings.xml`.

    Reading XDG_CONFIG_HOME here keeps the test process's isolated settings home
    effective even when another JUCE static has already cached its special
    locations before main. In the app this resolves to the same Duet/Settings.xml
    as Tracktion's default storage.

    Nothing constructs this directly. It is reached through a
    `juce::SharedResourcePointer`, which makes it on first use and destroys it —
    writing the file out — when the last holder goes.
*/
class DuetPropertyStorage final : public te::PropertyStorage
{
public:
    DuetPropertyStorage() : te::PropertyStorage (appSettingsName) {}

    juce::File getAppPrefsFolder() override
    {
        const auto xdgConfigHome =
            juce::SystemStats::getEnvironmentVariable ("XDG_CONFIG_HOME", {});

        if (xdgConfigHome.isNotEmpty())
        {
            auto folder = juce::File { xdgConfigHome }.getChildFile (getApplicationName());
            folder.createDirectory();
            return folder;
        }

        return te::PropertyStorage::getAppPrefsFolder();
    }
};

/** One Engine's PropertyStorage: a forwarding adapter onto the one store.

    `te::Engine`'s constructor takes ownership of a `unique_ptr<PropertyStorage>`,
    so the shared store cannot be handed over. This is handed over instead, and
    holding it open is what keeps the store alive for as long as the Engine can
    ask it anything.

    Every virtual forwards, the ones the base class would answer through
    `getPropertiesFile()` included. What the engine reads or writes has to reach
    the one file, and that must not rest on which of the base's methods happen to
    route through a virtual.
*/
class SharedPropertyStorage final : public te::PropertyStorage
{
public:
    SharedPropertyStorage() : te::PropertyStorage (appSettingsName) {}

    juce::File getAppCacheFolder() override { return storage->getAppCacheFolder(); }
    juce::File getAppPrefsFolder() override { return storage->getAppPrefsFolder(); }

    void flushSettingsToDisk() override { storage->flushSettingsToDisk(); }

    void removeProperty (te::SettingID setting) override { storage->removeProperty (setting); }

    juce::var getProperty (te::SettingID setting, const juce::var& defaultValue) override
    {
        return storage->getProperty (setting, defaultValue);
    }

    void setProperty (te::SettingID setting, const juce::var& value) override
    {
        storage->setProperty (setting, value);
    }

    std::unique_ptr<juce::XmlElement> getXmlProperty (te::SettingID setting) override
    {
        return storage->getXmlProperty (setting);
    }

    void setXmlProperty (te::SettingID setting, const juce::XmlElement& xml) override
    {
        storage->setXmlProperty (setting, xml);
    }

    void removePropertyItem (te::SettingID setting, juce::StringRef item) override
    {
        storage->removePropertyItem (setting, item);
    }

    juce::var getPropertyItem (te::SettingID setting,
                               juce::StringRef item,
                               const juce::var& defaultValue) override
    {
        return storage->getPropertyItem (setting, item, defaultValue);
    }

    void setPropertyItem (te::SettingID setting,
                          juce::StringRef item,
                          const juce::var& value) override
    {
        storage->setPropertyItem (setting, item, value);
    }

    std::unique_ptr<juce::XmlElement> getXmlPropertyItem (te::SettingID setting,
                                                          juce::StringRef item) override
    {
        return storage->getXmlPropertyItem (setting, item);
    }

    void setXmlPropertyItem (te::SettingID setting,
                             juce::StringRef item,
                             const juce::XmlElement& xml) override
    {
        storage->setXmlPropertyItem (setting, item, xml);
    }

    juce::File getDefaultLoadSaveDirectory (juce::StringRef label) override
    {
        return storage->getDefaultLoadSaveDirectory (label);
    }

    void setDefaultLoadSaveDirectory (juce::StringRef label, const juce::File& newPath) override
    {
        storage->setDefaultLoadSaveDirectory (label, newPath);
    }

    juce::File getDefaultLoadSaveDirectory (te::ProjectItem::Category category) override
    {
        return storage->getDefaultLoadSaveDirectory (category);
    }

    juce::String getUserName() override { return storage->getUserName(); }
    juce::String getApplicationName() override { return storage->getApplicationName(); }
    juce::String getApplicationVersion() override { return storage->getApplicationVersion(); }

    juce::PropertiesFile& getPropertiesFile() override { return storage->getPropertiesFile(); }

private:
    juce::SharedResourcePointer<DuetPropertyStorage> storage;
};
} // namespace duet::model
