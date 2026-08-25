#pragma once

#include <duet/gui/PluginPresets.h>
#include <duet/model/Session.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace duet::gui
{
/** Owns the only floating windows Duet creates: one hosted plugin editor window
    per plugin. This class is the deliberately narrow component-only crossing of
    the engine seam recorded by ADR 0008; paintless GUI code still sees only the
    public model facade.
*/
class PluginEditorManager
{
public:
    explicit PluginEditorManager (Settings& settings);
    ~PluginEditorManager();

    PluginEditorManager (const PluginEditorManager&) = delete;
    PluginEditorManager& operator= (const PluginEditorManager&) = delete;

    void setSession (duet::model::Session* openProject);
    void open (duet::model::PluginRef plugin);
    void closeAll();

    [[nodiscard]] std::size_t openWindowCount() const;
    [[nodiscard]] bool isOpen (duet::model::PluginRef plugin) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::gui
