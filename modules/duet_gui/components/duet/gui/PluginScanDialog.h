#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/PluginScan.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace duet::gui
{
/** The plugin-scan dialog's surface: start a scan, watch it, and see what it
    found.

    The scan itself is the `PluginScan` beneath this; the surface steps it on a
    timer, so the message loop turns between plugins and the producer watches
    the scan rather than a frozen window. What the scan would not take is listed
    beside what it did, so a plugin that crashed the scanner is something they
    can see (spec 535bbo).
*/
class PluginScanPanel final : public juce::Component
{
public:
    PluginScanPanel (Appearance& lookAndScale, PluginScan& scanning, std::function<void()> onClose);

    ~PluginScanPanel() override;

    PluginScanPanel (const PluginScanPanel& other) = delete;
    PluginScanPanel& operator= (const PluginScanPanel& other) = delete;

    void resized() override;

    /** Escape dismisses it, and stops a scan it was watching. */
    bool keyPressed (const juce::KeyPress& key) override;

    /** Starts the scan, the way the Scan button does. */
    void beginScan();

    /** Steps it once, the way the timer does. */
    bool stepScan();

    /** What the producer is told about the scan. */
    [[nodiscard]] juce::String statusText() const;

    /** The results as the list shows them, in the order it shows them: what was
        found first, and what would not be taken after it.
    */
    [[nodiscard]] std::vector<std::string> resultLines() const;

private:
    class Body;

    std::unique_ptr<Body> body;
    std::function<void()> dismiss;
};

/** The window the panel is shown in. */
class PluginScanDialog final : public juce::DocumentWindow
{
public:
    PluginScanDialog (Appearance& lookAndScale,
                      PluginScan& scanning,
                      std::function<void()> onClose);

    ~PluginScanDialog() override;

    void closeButtonPressed() override;

private:
    std::function<void()> closed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginScanDialog)
};
} // namespace duet::gui
