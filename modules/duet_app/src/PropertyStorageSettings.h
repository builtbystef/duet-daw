#pragma once

#include <duet/gui/Settings.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace duet::app
{
/** The app-global settings store, over the engine's PropertyStorage.

    The shell is where this lives because the store is the machine's and not a
    project's: it exists before the first project is opened and outlives the last
    one, so it cannot hang off a session. PropertyStorage is the engine's own
    class and needs no Engine to make — it is a properties file under the user's
    application-data folder, which is where the engine's settings already are.

    Every write re-reads the file first and flushes it after, so a setting the
    producer changes is on disk before the next thing that could end the process,
    and so a key another writer has added since is not dropped by ours.
*/
class PropertyStorageSettings final : public duet::gui::Settings
{
public:
    PropertyStorageSettings();
    ~PropertyStorageSettings() override;

    PropertyStorageSettings (const PropertyStorageSettings&) = delete;
    PropertyStorageSettings& operator= (const PropertyStorageSettings&) = delete;

    [[nodiscard]] std::optional<std::string> value (std::string_view key) const override;
    void setValue (std::string_view key, std::string_view newValue) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::app
