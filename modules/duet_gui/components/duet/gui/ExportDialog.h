#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/Export.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace duet::gui
{
/** The Export/Bounce dialog's surface: the producer's choices, and the render
    they start.

    Everything it decides is the `Export` beneath it; this is the surface over
    one. While a render runs it shows how far it has got and offers a Cancel,
    and Escape dismisses it — which is what "keyboard-dismissible" means for
    every dialog here (spec 535bbo).

    A surface and not a window, because what a window is is a peer on a screen,
    and what this does is the same on a machine that has none.
*/
class ExportPanel final : public juce::Component
{
public:
    /** @param lookAndScale  the theme and the interface scale it is drawn and
                             measured in
        @param exporting     the view-model it shows and drives
        @param onClose       called when the producer dismisses it
    */
    ExportPanel (Appearance& lookAndScale, Export& exporting, std::function<void()> onClose);

    ~ExportPanel() override;

    ExportPanel (const ExportPanel& other) = delete;
    ExportPanel& operator= (const ExportPanel& other) = delete;

    void resized() override;

    /** Escape dismisses it. A render in flight is cancelled by the same press:
        this was what was watching it.
    */
    bool keyPressed (const juce::KeyPress& key) override;

    /** Starts the export the rows describe, the way the Export button does. */
    void beginExport();

    /** What the producer is told about the export: empty before one has been
        asked for.
    */
    [[nodiscard]] juce::String statusText() const;

private:
    class Rows;

    std::unique_ptr<Rows> rows;
    std::function<void()> dismiss;
};

/** The window the panel is shown in. */
class ExportDialog final : public juce::DocumentWindow
{
public:
    ExportDialog (Appearance& lookAndScale, Export& exporting, std::function<void()> onClose);

    ~ExportDialog() override;

    void closeButtonPressed() override;

private:
    std::function<void()> closed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportDialog)
};
} // namespace duet::gui
