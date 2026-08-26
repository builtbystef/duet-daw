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
    one, so it cannot hang off a session. What it holds is the one store every
    open session's engine holds too, reached through the model's facade — the
    interface's settings and the engine's are one file, and one holder is what
    keeps either side from writing the whole file over the other.

    Every write puts the store on disk, so a setting the producer changes is
    written before the next thing that could end the process.
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
