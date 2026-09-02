#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/Browser.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <string>
#include <vector>

namespace duet::gui
{
/** What a drag out of the browser carries, and how a surface reads it.

    The description is the dragged item's identity behind a prefix, so a drop
    target can tell a browser drag from anything else a window may one day drag
    without knowing what the browser's component is.
*/
namespace browserDrag
{
    inline constexpr const char* prefix = "browser:";

    [[nodiscard]] juce::var descriptionOf (const std::string& itemIdentity);

    /** The identity a drag carries, and nothing when it is not a browser drag.
    */
    [[nodiscard]] std::optional<std::string> identityOf (const juce::var& description);
} // namespace browserDrag

/** The left dock, on screen: a search box over the sections the browser model
    holds.

    The thin half of the surface. Which sections there are, what a search
    matches, what is favourited and what a drop does are all the view-model's;
    this paints them, turns a click into one of its calls, and starts the drag
    that the arrangement and the mixer finish (spec 535bbo).
*/
class BrowserCanvas final : public juce::Component,
                            private juce::Timer,
                            private Appearance::Listener
{
public:
    BrowserCanvas (Appearance& lookAndScale, Browser& dock);

    ~BrowserCanvas() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

    /** Reads the model again and lays the rows out from what it says: what
        opening a project, adding a folder and finishing a scan all need.
    */
    void refresh();

    /** The Scanning… line the dock composes from the model's snapshot, empty
        when nothing is in flight.
    */
    [[nodiscard]] const std::string& composedStatus() const { return statusLine; }

    /** Play or Stop, as the visible Source audition control currently reads.
        A test of visible state without paint.
    */
    [[nodiscard]] std::string auditionButtonText() const;

    /** Loading, playing, progress, stopped, or a row-local error — the same
        snapshot the button and the selected row read.
    */
    [[nodiscard]] SourceAuditionStatus composedAudition() const;

    [[nodiscard]] Browser& model() noexcept { return browser; }
    [[nodiscard]] const Browser& model() const noexcept { return browser; }

    /** The chrome's measurements, in logical units. */
    static constexpr int searchHeight = 22;
    static constexpr int rowHeight = 18;
    static constexpr int sectionHeight = 20;
    static constexpr int indent = 14;
    static constexpr int favouriteWidth = 18;

private:
    /** One line of the dock: a section's header, or an item under it. */
    struct Row
    {
        bool isSection = false;
        std::string identity;
        std::string name;
        bool expanded = false;
        bool favourite = false;
        std::string status;
    };

    void appearanceChanged() override;
    void timerCallback() override;
    void refreshAuditionButton();
    [[nodiscard]] int rowHeightPx (const Row& row) const;
    [[nodiscard]] std::optional<std::size_t> rowAt (int y) const;
    [[nodiscard]] int rowTop (std::size_t index) const;
    [[nodiscard]] int contentHeightPx() const;
    [[nodiscard]] juce::Rectangle<int> listArea() const;

    Appearance& appearance;
    Browser& browser;
    juce::TextEditor searchBox;
    juce::TextButton auditionButton { "Play" };
    std::vector<Row> rows;
    std::string statusLine;
    int scrollOffsetPx = 0;
    std::optional<std::size_t> pressedRow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserCanvas)
};
} // namespace duet::gui
