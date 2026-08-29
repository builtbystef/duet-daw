#include <duet/gui/BrowserCanvas.h>

#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/Text.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <algorithm>
#include <utility>

namespace duet::gui
{
namespace browserDrag
{
    juce::var descriptionOf (const std::string& itemIdentity)
    {
        return juce::var { juce::String { prefix } + juce::String { itemIdentity } };
    }

    std::optional<std::string> identityOf (const juce::var& description)
    {
        if (! description.isString())
            return std::nullopt;

        const auto text = description.toString();

        if (! text.startsWith (prefix))
            return std::nullopt;

        return text.substring (juce::String { prefix }.length()).toStdString();
    }
} // namespace browserDrag

namespace
{
    /** How far a pointer travels before a click on an item becomes a drag. */
    constexpr int dragThresholdPx = 4;
} // namespace

//==============================================================================
BrowserCanvas::BrowserCanvas (Appearance& lookAndScale, Browser& dock)
    : appearance (lookAndScale), browser (dock)
{
    searchBox.setTextToShowWhenEmpty ("Search",
                                      toJuce (appearance.colour (ColourToken::textMuted)));
    searchBox.setMultiLine (false);
    searchBox.setReturnKeyStartsNewLine (false);
    searchBox.onTextChange = [this] { browser.setSearch (searchBox.getText().toStdString()); };

    addAndMakeVisible (searchBox);

    // The model tells this what to draw, whoever changed it: the Settings
    // window manages the same sample folders, and a folder added there is in
    // the dock without either knowing about the other.
    browser.onChanged ([this] { refresh(); });
    appearance.addListener (this);
    refresh();
}

BrowserCanvas::~BrowserCanvas()
{
    browser.onChanged ({});
    appearance.removeListener (this);
}

//==============================================================================
void BrowserCanvas::refresh()
{
    rows.clear();

    for (const auto& section : browser.sections())
    {
        rows.push_back ({ true, section.identity, section.name, section.expanded, false });

        if (! section.expanded)
            continue;

        for (const auto& item : section.items)
            rows.push_back ({ false, item.identity, item.name, false, item.favourite });
    }

    scrollOffsetPx =
        std::clamp (scrollOffsetPx, 0, std::max (0, contentHeightPx() - listArea().getHeight()));
    repaint();
}

int BrowserCanvas::rowHeightPx (const Row& row) const
{
    return appearance.scaled (row.isSection ? sectionHeight : rowHeight);
}

int BrowserCanvas::contentHeightPx() const
{
    auto total = 0;

    for (const auto& row : rows)
        total += rowHeightPx (row);

    return total;
}

juce::Rectangle<int> BrowserCanvas::listArea() const
{
    return getLocalBounds()
        .reduced (appearance.scaled (metrics::rowGap))
        .withTrimmedTop (appearance.scaled (searchHeight + metrics::rowGap));
}

int BrowserCanvas::rowTop (std::size_t index) const
{
    auto top = listArea().getY() - scrollOffsetPx;

    for (std::size_t before = 0; before < index && before < rows.size(); ++before)
        top += rowHeightPx (rows[before]);

    return top;
}

std::optional<std::size_t> BrowserCanvas::rowAt (int y) const
{
    auto top = listArea().getY() - scrollOffsetPx;

    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        const auto height = rowHeightPx (rows[index]);

        if (y >= top && y < top + height)
            return index;

        top += height;
    }

    return std::nullopt;
}

//==============================================================================
void BrowserCanvas::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds();

    g.setColour (toJuce (appearance.colour (ColourToken::surfaceDefault)));
    g.fillRect (area);

    g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
    g.drawRect (area, 1);

    const auto list = listArea();
    g.reduceClipRegion (list);

    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        const auto& row = rows[index];
        const juce::Rectangle<int> bounds {
            list.getX(), rowTop (index), list.getWidth(), rowHeightPx (row)
        };

        if (! bounds.intersects (list))
            continue;

        if (row.isSection)
        {
            g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
            g.setFont (interFont (appearance.scaled (typography::eyebrow)));
            g.drawText ((row.expanded ? utf8 ("▾ ") : utf8 ("▸ ")) + juce::String { row.name },
                        bounds,
                        juce::Justification::centredLeft);
            continue;
        }

        auto text = bounds.withTrimmedLeft (appearance.scaled (indent));
        const auto star = text.removeFromRight (appearance.scaled (favouriteWidth));

        g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
        g.setFont (interFont (appearance.scaled (typography::body)));
        g.drawText (juce::String { row.name }, text, juce::Justification::centredLeft, true);

        g.setColour (toJuce (appearance.colour (row.favourite ? ColourToken::textPrimary
                                                              : ColourToken::textDisabled)));
        g.drawText (row.favourite ? utf8 ("★") : utf8 ("☆"), star, juce::Justification::centred);
    }
}

void BrowserCanvas::resized()
{
    searchBox.setBounds (getLocalBounds()
                             .reduced (appearance.scaled (metrics::rowGap))
                             .withHeight (appearance.scaled (searchHeight)));
    refresh();
}

//==============================================================================
void BrowserCanvas::mouseDown (const juce::MouseEvent& event)
{
    pressedRow = rowAt (event.y);

    if (! pressedRow.has_value())
        return;

    const auto& row = rows[*pressedRow];

    if (row.isSection)
    {
        browser.setExpanded (row.identity, ! row.expanded);
        pressedRow.reset();
        return;
    }

    const auto list = listArea();

    if (event.x >= list.getRight() - appearance.scaled (favouriteWidth))
    {
        browser.toggleFavourite (row.identity);
        pressedRow.reset();
    }
}

void BrowserCanvas::mouseDrag (const juce::MouseEvent& event)
{
    if (! pressedRow.has_value() || event.getDistanceFromDragStart() < dragThresholdPx)
        return;

    const auto identity = rows[*pressedRow].identity;
    pressedRow.reset();

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        container->startDragging (browserDrag::descriptionOf (identity), this);
}

void BrowserCanvas::mouseWheelMove ([[maybe_unused]] const juce::MouseEvent& event,
                                    const juce::MouseWheelDetails& wheel)
{
    const auto hidden = std::max (0, contentHeightPx() - listArea().getHeight());
    scrollOffsetPx =
        std::clamp (scrollOffsetPx - static_cast<int> (wheel.deltaY * 120.0F), 0, hidden);
    repaint();
}

void BrowserCanvas::appearanceChanged()
{
    searchBox.setTextToShowWhenEmpty ("Search",
                                      toJuce (appearance.colour (ColourToken::textMuted)));

    // The scale is a layout and not a repaint: every row is measured in logical
    // units, so a change is only on screen once they are laid out again.
    resized();
    repaint();
}
} // namespace duet::gui
