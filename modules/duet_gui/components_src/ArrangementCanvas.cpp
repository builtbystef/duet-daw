#include <duet/gui/ArrangementCanvas.h>

#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/MainShell.h>
#include <duet/gui/TimelineGeometry.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <algorithm>

namespace duet::gui
{
namespace
{
    /** One notch of a wheel, in the units JUCE reports a wheel in.

        JUCE hands on a fraction rather than a count of notches, and X11's notch
        is 50/256 of it. The view-model's conventions are written in notches,
        which is what a producer turns, so the conversion happens here — at the
        edge, with the platform's number, and once.
    */
    constexpr double wheelNotch = 50.0 / 256.0;

    /** The token a line of that weight is drawn in. */
    [[nodiscard]] ColourToken tokenFor (GridWeight weight)
    {
        switch (weight)
        {
            case GridWeight::bar:
                return ColourToken::gridBar;
            case GridWeight::beat:
                return ColourToken::gridBeat;
            case GridWeight::fine:
            default:
                return ColourToken::gridFine;
        }
    }

    /** How tall a tick of that weight is drawn in the ruler. */
    [[nodiscard]] float tickHeightFor (GridWeight weight)
    {
        switch (weight)
        {
            case GridWeight::bar:
                return ArrangementCanvas::barTickHeight;
            case GridWeight::beat:
                return ArrangementCanvas::beatTickHeight;
            case GridWeight::fine:
            default:
                return ArrangementCanvas::fineTickHeight;
        }
    }
} // namespace

//==============================================================================
/** The ruler above the arrangement: the grid as ticks, and the bars and beats
    the producer counts in, in tabular numerals so that a number that changes
    does not shift the ones beside it.

    Clicking it moves the playhead, and dragging scrubs. Neither is an Action —
    where the producer is listening from is not a change to the project.
*/
class ArrangementCanvas::Ruler final : public juce::Component
{
public:
    Ruler (Appearance& lookAndScale, ArrangementView& arrangement)
        : appearance (lookAndScale), view (arrangement)
    {
        setComponentID (surfaceId::arrangementRuler);
    }

    ~Ruler() override = default;

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds();

        g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
        g.fillRect (area);

        for (const auto& line : view.geometry().gridLines())
        {
            const auto height = juce::roundToInt (static_cast<float> (area.getHeight())
                                                  * tickHeightFor (line.weight));

            g.setColour (toJuce (appearance.colour (tokenFor (line.weight))));
            g.fillRect (line.x, area.getBottom() - height, 1, height);
        }

        g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
        g.setFont (readoutFont (appearance.scaled (typography::eyebrow)));

        for (const auto& label : view.geometry().rulerLabels())
            g.drawText (juce::String { label.text },
                        area.withX (label.x + appearance.scaled (labelGap))
                            .withWidth (appearance.scaled (labelWidth)),
                        juce::Justification::centredLeft);

        // The playhead's own head. The canvas draws the line down the timeline
        // and the ruler draws the part over itself, because a child component
        // is painted over whatever its parent drew there.
        g.setColour (toJuce (appearance.colour (ColourToken::accentStrong)));
        g.fillRect (view.playheadX(),
                    0,
                    std::max (1, appearance.scaled (ArrangementCanvas::playheadThickness)),
                    area.getHeight());

        g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
        g.fillRect (area.withHeight (1));
        g.fillRect (area.withY (area.getBottom() - 1).withHeight (1));
    }

    void mouseDown (const juce::MouseEvent& event) override { scrubTo (event); }
    void mouseDrag (const juce::MouseEvent& event) override { scrubTo (event); }

    /** A wheel over the ruler is a wheel over the arrangement: the ruler is
        part of the same surface, and JUCE offers a wheel to the component under
        the pointer rather than to the one that scrolls.
    */
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override
    {
        if (auto* canvas = getParentComponent(); canvas != nullptr)
            canvas->mouseWheelMove (event.getEventRelativeTo (canvas), wheel);
    }

private:
    void scrubTo (const juce::MouseEvent& event)
    {
        view.clickRuler (event.getPosition().x);

        if (auto* canvas = getParentComponent(); canvas != nullptr)
            canvas->repaint();
    }

    /** Where a label sits beside the line it belongs to, and how much room it is
        given to be read in — logical units.
    */
    static constexpr int labelGap = 3;
    static constexpr int labelWidth = 44;

    Appearance& appearance;
    ArrangementView& view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ruler)
};

//==============================================================================
ArrangementCanvas::ArrangementCanvas (Appearance& lookAndScale, ArrangementView& arrangement)
    : appearance (lookAndScale), view (arrangement)
{
    setComponentID (surfaceId::arrangement);

    ruler = std::make_unique<Ruler> (appearance, view);
    addAndMakeVisible (*ruler);

    startTimerHz (playheadRefreshHz);
}

ArrangementCanvas::~ArrangementCanvas() = default;

//==============================================================================
void ArrangementCanvas::paint (juce::Graphics& g)
{
    const auto timeline = timelineArea();

    g.setColour (toJuce (appearance.colour (ColourToken::surfaceCanvas)));
    g.fillRect (timeline);

    for (const auto& line : view.geometry().gridLines())
    {
        g.setColour (toJuce (appearance.colour (tokenFor (line.weight))));
        g.fillRect (line.x, timeline.getY(), 1, timeline.getHeight());
    }

    // The playhead down the timeline; the ruler draws the part over itself.
    g.setColour (toJuce (appearance.colour (ColourToken::accentStrong)));
    g.fillRect (playheadColumn (view.playheadX()));

    g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
    g.drawRect (getLocalBounds(), 1);
}

void ArrangementCanvas::resized()
{
    auto area = getLocalBounds();

    ruler->setBounds (area.removeFromTop (appearance.scaled (rulerHeight)));
    view.setWidthPx (getWidth());
}

//==============================================================================
void ArrangementCanvas::mouseWheelMove (const juce::MouseEvent& event,
                                        const juce::MouseWheelDetails& wheel)
{
    ScrollGesture gesture;

    gesture.deltaX = wheel.deltaX / wheelNotch;
    gesture.deltaY = wheel.deltaY / wheelNotch;
    gesture.pointerX = event.getPosition().x;
    gesture.ctrl = event.mods.isCtrlDown();
    gesture.shift = event.mods.isShiftDown();

    if (wheel.isReversed)
    {
        gesture.deltaX = -gesture.deltaX;
        gesture.deltaY = -gesture.deltaY;
    }

    view.scroll (gesture);
    repaint();
}

//==============================================================================
void ArrangementCanvas::timerCallback()
{
    // What the engine has published, and no lock to read it behind.
    movePlayheadTo (view.playheadX());
}

void ArrangementCanvas::movePlayheadTo (int x)
{
    if (x == playheadX)
        return;

    const auto was = playheadColumn (playheadX);

    playheadX = x;

    const auto inTheRuler = [this] (juce::Rectangle<int> column)
    { return column.withY (0).withHeight (ruler->getHeight()); };

    repaint (was);
    repaint (playheadColumn (playheadX));
    ruler->repaint (inTheRuler (was));
    ruler->repaint (inTheRuler (playheadColumn (playheadX)));
}

juce::Rectangle<int> ArrangementCanvas::timelineArea() const
{
    return getLocalBounds().withTrimmedTop (appearance.scaled (rulerHeight));
}

juce::Rectangle<int> ArrangementCanvas::playheadColumn (int x) const
{
    const auto timeline = timelineArea();
    const auto thickness = std::max (1, appearance.scaled (playheadThickness));

    return { x, timeline.getY(), thickness, timeline.getHeight() };
}
} // namespace duet::gui
