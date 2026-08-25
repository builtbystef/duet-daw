#include <duet/gui/CollaboratorPanelCanvas.h>

#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <cmath>
#include <utility>

namespace duet::gui
{
namespace
{
    // Logical units: the interface scale is what turns one into a pixel.
    constexpr int badgeSize = 12;
    constexpr int bubblePadding = 8;
    constexpr int bubbleGap = 8;
    constexpr int chipHeight = 15;
    constexpr int chipPadding = 6;
    constexpr int accentRuleWidth = 2;
    constexpr int spinnerSize = 16;
    constexpr int cancelButtonWidth = 62;
    constexpr int sendButtonWidth = 58;
    constexpr float spinnerTurnsPerSecond = 0.6F;

    /** The ✦ badge, drawn rather than typed: the four-pointed star is not a
        glyph every copy of the typeface has, and the badge has to read the same
        wherever Duet runs. It is the Collaborator's mark and appears nowhere
        else in the interface.
    */
    void paintCollaboratorBadge (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink)
    {
        const auto centre = area.getCentre();
        const auto arm = area.getWidth() / 2.0F;
        const auto waist = arm * 0.24F;

        juce::Path star;

        star.startNewSubPath (centre.x, centre.y - arm);
        star.lineTo (centre.x + waist, centre.y - waist);
        star.lineTo (centre.x + arm, centre.y);
        star.lineTo (centre.x + waist, centre.y + waist);
        star.lineTo (centre.x, centre.y + arm);
        star.lineTo (centre.x - waist, centre.y + waist);
        star.lineTo (centre.x - arm, centre.y);
        star.lineTo (centre.x - waist, centre.y - waist);
        star.closeSubPath();

        g.setColour (ink);
        g.fillPath (star);
    }

    /** How tall a run of text is when it is wrapped to a width. */
    [[nodiscard]] int wrappedHeight (const juce::String& text, const juce::Font& font, int width)
    {
        juce::AttributedString attributed { text };
        juce::TextLayout layout;

        attributed.setFont (font);
        layout.createLayout (attributed, static_cast<float> (juce::jmax (1, width)));

        return static_cast<int> (std::ceil (layout.getHeight()));
    }
} // namespace

//==============================================================================
/** The scrolling conversation: every entry, laid out top to bottom.

    The producer's messages and the Collaborator's commentary read as two
    different things — commentary sits in an accent bubble behind the reserved
    teal rule, a producer message on the interactive surface — and the two ways
    a Task Run can end are plain lines that belong to neither.
*/
class CollaboratorPanelCanvas::Conversation final : public juce::Component
{
public:
    Conversation (Appearance& lookAndScale, CollaboratorPanel& panelModel)
        : appearance (lookAndScale), panel (panelModel)
    {
        setComponentID (collaboratorId::conversation);
        setInterceptsMouseClicks (false, false);
    }

    ~Conversation() override = default;

    /** Measures every entry at the width the panel has now and takes the height
        they need. The viewport is what scrolls the result.
    */
    void layOutEntries (int width)
    {
        laidOut.clear();

        const auto padding = appearance.scaled (bubblePadding);
        const auto gap = appearance.scaled (bubbleGap);
        const auto font = interFont (appearance.scaled (typography::body));
        const auto chipFont = interFont (appearance.scaled (typography::eyebrow));
        auto y = gap;

        for (const auto& entry : panel.conversation())
        {
            const auto textWidth = width - (2 * padding) - appearance.scaled (accentRuleWidth);
            const auto chip = juce::String { entry.context };
            const auto chipRoom =
                chip.isEmpty() ? 0 : appearance.scaled (chipHeight) + appearance.scaled (2);
            const auto height = wrappedHeight (juce::String { entry.text }, font, textWidth)
                                + (2 * padding) + chipRoom;

            laidOut.push_back ({ entry.kind,
                                 juce::String { entry.text },
                                 chip,
                                 juce::Rectangle<int> { 0, y, width, height } });
            y += height + gap;
        }

        setSize (width, y);
        chipTypeface = chipFont;
        bodyTypeface = font;
    }

    void paint (juce::Graphics& g) override
    {
        const auto padding = appearance.scaled (bubblePadding);
        const auto radius = static_cast<float> (appearance.scaled (metrics::radiusMedium));
        const auto rule = appearance.scaled (accentRuleWidth);

        for (const auto& entry : laidOut)
        {
            auto area = entry.bounds;

            if (entry.kind == EntryKind::producer || entry.kind == EntryKind::commentary)
            {
                const auto commentary = entry.kind == EntryKind::commentary;

                g.setColour (
                    toJuce (appearance.colour (commentary ? ColourToken::collaborator
                                                          : ColourToken::surfaceInteractive))
                        .withAlpha (commentary ? 0.12F : 1.0F));
                g.fillRoundedRectangle (area.toFloat(), radius);

                if (commentary)
                {
                    g.setColour (toJuce (appearance.colour (ColourToken::collaborator)));
                    g.fillRect (area.withWidth (rule));
                }
            }

            area = area.reduced (padding).withTrimmedLeft (rule);

            if (entry.context.isNotEmpty())
            {
                auto chip = area.removeFromTop (appearance.scaled (chipHeight));

                const auto chipWidth =
                    static_cast<int> (std::ceil (
                        juce::GlyphArrangement::getStringWidth (chipTypeface, entry.context)))
                    + (2 * appearance.scaled (chipPadding));

                chip = chip.withWidth (juce::jmin (chip.getWidth(), chipWidth));

                g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
                g.fillRoundedRectangle (
                    chip.toFloat(), static_cast<float> (appearance.scaled (metrics::radiusSmall)));
                g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
                g.setFont (chipTypeface);
                g.drawText (entry.context, chip, juce::Justification::centred);

                area.removeFromTop (appearance.scaled (2));
            }

            g.setColour (toJuce (appearance.colour (inkFor (entry.kind))));
            g.setFont (bodyTypeface);
            g.drawFittedText (
                entry.text,
                area,
                juce::Justification::topLeft,
                juce::jmax (1, area.getHeight() / juce::jmax (1, (int) bodyTypeface.getHeight())));
        }
    }

private:
    struct EntryLayout
    {
        EntryKind kind = EntryKind::producer;
        juce::String text;
        juce::String context;
        juce::Rectangle<int> bounds;
    };

    [[nodiscard]] static ColourToken inkFor (EntryKind kind)
    {
        switch (kind)
        {
            case EntryKind::failure:
                return ColourToken::semanticDanger;

            case EntryKind::notice:
                return ColourToken::textMuted;

            case EntryKind::producer:
            case EntryKind::commentary:
                break;
        }

        return ColourToken::textPrimary;
    }

    Appearance& appearance;
    CollaboratorPanel& panel;
    std::vector<EntryLayout> laidOut;
    juce::Font chipTypeface { juce::FontOptions {} };
    juce::Font bodyTypeface { juce::FontOptions {} };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Conversation)
};

//==============================================================================
/** The Task Run card: what is on screen while the Collaborator is working.

    A spinner, the phrase it is saying now, the hint that the producer need not
    wait for it, and Cancel.
*/
class CollaboratorPanelCanvas::TaskRunCard final : public juce::Component
{
public:
    TaskRunCard (Appearance& lookAndScale, CollaboratorPanel& panelModel)
        : appearance (lookAndScale), panel (panelModel)
    {
        setComponentID (collaboratorId::taskRun);
        cancel.setComponentID (collaboratorId::cancel);
        addAndMakeVisible (cancel);
        cancel.onClick = [this] { panel.requestCancel(); };
    }

    ~TaskRunCard() override = default;

    /** Turns the spinner. It is a spinner and not a progress bar because a Task
        Run takes as long as it takes.
    */
    void advanceSpinner (double seconds)
    {
        turn = std::fmod (turn + (static_cast<float> (seconds) * spinnerTurnsPerSecond), 1.0F);
    }

    void resized() override
    {
        cancel.setBounds (getLocalBounds()
                              .reduced (appearance.scaled (metrics::panelPadding))
                              .removeFromBottom (appearance.scaled (metrics::rowHeight))
                              .removeFromRight (appearance.scaled (cancelButtonWidth)));
    }

    void paint (juce::Graphics& g) override
    {
        const auto radius = static_cast<float> (appearance.scaled (metrics::radiusLarge));
        const auto teal = toJuce (appearance.colour (ColourToken::collaborator));

        g.setColour (teal.withAlpha (0.10F));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), radius);
        g.setColour (teal.withAlpha (0.55F));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5F), radius, 1.0F);

        auto area = getLocalBounds().reduced (appearance.scaled (metrics::panelPadding));
        auto top = area.removeFromTop (appearance.scaled (metrics::rowHeight));
        const auto spinner = top.removeFromLeft (appearance.scaled (spinnerSize)).toFloat();

        juce::Path arc;

        arc.addCentredArc (spinner.getCentreX(),
                           spinner.getCentreY(),
                           spinner.getWidth() / 2.0F,
                           spinner.getHeight() / 2.0F,
                           0.0F,
                           turn * juce::MathConstants<float>::twoPi,
                           (turn + 0.7F) * juce::MathConstants<float>::twoPi,
                           true);
        g.setColour (teal);
        g.strokePath (arc, juce::PathStrokeType { 2.0F });

        top.removeFromLeft (appearance.scaled (metrics::rowGap));
        g.setFont (interFont (appearance.scaled (typography::body), true));
        g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
        g.drawText (panel.statusPhrase(), top, juce::Justification::centredLeft);

        g.setFont (interFont (appearance.scaled (typography::eyebrow)));
        g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
        g.drawText (CollaboratorPanel::keepEditingHint,
                    area.removeFromTop (appearance.scaled (metrics::rowHeight)),
                    juce::Justification::centredLeft);
    }

private:
    Appearance& appearance;
    CollaboratorPanel& panel;
    juce::TextButton cancel { "Cancel" };
    float turn = 0.0F;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaskRunCard)
};

//==============================================================================
CollaboratorPanelCanvas::CollaboratorPanelCanvas (Appearance& lookAndScale,
                                                  CollaboratorPanel& panelModel)
    : appearance (lookAndScale), panel (panelModel)
{
    conversation = std::make_unique<Conversation> (appearance, panel);
    card = std::make_unique<TaskRunCard> (appearance, panel);

    scroller.setViewedComponent (conversation.get(), false);
    scroller.setScrollBarsShown (true, false);
    addAndMakeVisible (scroller);
    addChildComponent (*card);

    composer.setComponentID (collaboratorId::composer);
    sendButton.setComponentID (collaboratorId::send);
    composer.setMultiLine (true, true);
    composer.setReturnKeyStartsNewLine (false);
    composer.setTextToShowWhenEmpty ("Ask the Collaborator for something",
                                     toJuce (appearance.colour (ColourToken::textDisabled)));
    composer.onTextChange = [this] { refresh(); };
    composer.onReturnKey = [this] { sendComposer(); };
    composer.onEscapeKey = [this] { returnFocusFromComposer(); };
    sendButton.onClick = [this] { sendComposer(); };

    addAndMakeVisible (composer);
    addAndMakeVisible (sendButton);

    appearance.addListener (this);
    juce::Desktop::getInstance().addFocusChangeListener (this);
    startTimerHz (idleTickHz);
    refresh();
}

CollaboratorPanelCanvas::~CollaboratorPanelCanvas()
{
    juce::Desktop::getInstance().removeFocusChangeListener (this);
    appearance.removeListener (this);
}

void CollaboratorPanelCanvas::appearanceChanged()
{
    resized();
    refresh();
    repaint();
}

//==============================================================================
void CollaboratorPanelCanvas::setSelectionContextSource (
    std::function<SelectionContext()> currentSelection)
{
    selectionSource = std::move (currentSelection);
    refresh();
}

void CollaboratorPanelCanvas::refresh()
{
    // The composer's text lives in the editor, and the panel is told what it
    // holds here rather than on the editor's change message: that message
    // arrives on a later pass of the message loop, and Send has to be lit the
    // moment there is something to send.
    panel.setComposerText (composer.getText().toStdString());

    if (selectionSource)
        panel.setSelectionContext (selectionSource());

    auto changed = false;

    if (const auto prompts = panel.quickPrompts(); prompts != shownPrompts)
    {
        shownPrompts = prompts;
        layOutQuickPrompts();
        changed = true;
    }

    const auto running = panel.taskRunning();

    card->setVisible (running);
    composer.setEnabled (panel.composerEnabled());
    sendButton.setEnabled (panel.canSend() && panel.composerEnabled());

    if (running != shownRunning)
    {
        shownRunning = running;

        // A spinner is worth thirty frames a second and a selection poll is
        // not, so the panel only ticks that fast while a run is on.
        startTimerHz (running ? runningTickHz : idleTickHz);
        resized();
        changed = true;
    }

    if (panel.conversation().size() != shownEntries)
    {
        shownEntries = panel.conversation().size();
        conversation->layOutEntries (scroller.getMaximumVisibleWidth());

        // The newest message is the one the producer is waiting for, so the
        // conversation is always scrolled to the end of itself.
        scroller.setViewPositionProportionately (0.0, 1.0);
        changed = true;
    }

    // Nothing about an idle panel changes, and it is on screen the whole time
    // the producer is working: it repaints when it has something else to say.
    if (changed)
        repaint();
}

void CollaboratorPanelCanvas::returnFocusFromComposer()
{
    // What is in the composer is the producer's, and Escape is them looking
    // away from it rather than throwing it away.
    if (lastFocused != nullptr && lastFocused != &composer)
    {
        lastFocused->grabKeyboardFocus();
        return;
    }

    if (auto* window = getTopLevelComponent(); window != nullptr && window != this)
        window->grabKeyboardFocus();
}

void CollaboratorPanelCanvas::globalFocusChanged (juce::Component* focused)
{
    if (focused != nullptr && focused != &composer)
        lastFocused = focused;
}

//==============================================================================
void CollaboratorPanelCanvas::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds();

    g.setColour (toJuce (appearance.colour (ColourToken::surfaceDefault)));
    g.fillRect (area);
    g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
    g.drawRect (area, 1);

    auto header = area.reduced (appearance.scaled (metrics::panelPadding))
                      .withHeight (appearance.scaled (headerHeight));
    const auto badge = header.removeFromLeft (appearance.scaled (badgeSize)).toFloat();

    paintCollaboratorBadge (g,
                            badge.withSizeKeepingCentre (badge.getWidth(), badge.getWidth()),
                            toJuce (appearance.colour (ColourToken::collaborator)));

    header.removeFromLeft (appearance.scaled (metrics::rowGap));
    g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
    g.setFont (interFont (appearance.scaled (typography::eyebrow), true));
    g.drawText ("COLLABORATOR", header, juce::Justification::centredLeft);
}

void CollaboratorPanelCanvas::resized()
{
    auto area = getLocalBounds().reduced (appearance.scaled (metrics::panelPadding));

    area.removeFromTop (appearance.scaled (headerHeight));

    // The composer is taken off the bottom before anything else, so that a
    // conversation of any length leaves it exactly where it was.
    auto composerRow = area.removeFromBottom (appearance.scaled (composerHeight));

    sendButton.setBounds (composerRow.removeFromRight (appearance.scaled (sendButtonWidth))
                              .removeFromBottom (appearance.scaled (metrics::rowHeight)));
    composerRow.removeFromRight (appearance.scaled (metrics::rowGap));
    composer.setBounds (composerRow);

    area.removeFromBottom (appearance.scaled (metrics::rowGap));
    layOutQuickPrompts();
    area.removeFromBottom (appearance.scaled (quickPromptRowHeight));

    if (panel.taskRunning())
    {
        area.removeFromBottom (appearance.scaled (metrics::rowGap));
        card->setBounds (area.removeFromBottom (appearance.scaled (taskCardHeight)));
    }

    scroller.setBounds (area);
    conversation->layOutEntries (scroller.getMaximumVisibleWidth());
    scroller.setViewPositionProportionately (0.0, 1.0);
}

//==============================================================================
void CollaboratorPanelCanvas::timerCallback()
{
    const auto tickSeconds = 1.0 / (panel.taskRunning() ? runningTickHz : idleTickHz);

    if (panel.taskRunning())
    {
        panel.advance (tickSeconds);
        card->advanceSpinner (tickSeconds);
        card->repaint();
    }

    refresh();
}

void CollaboratorPanelCanvas::sendComposer()
{
    panel.setComposerText (composer.getText().toStdString());

    if (! panel.canSend() || ! panel.composerEnabled())
        return;

    panel.send();
    composer.setText (juce::String {}, false);
    refresh();
}

void CollaboratorPanelCanvas::layOutQuickPrompts()
{
    while (promptChips.size() > static_cast<int> (shownPrompts.size()))
        promptChips.removeLast();

    while (promptChips.size() < static_cast<int> (shownPrompts.size()))
    {
        auto* chip = promptChips.add (new juce::TextButton {});
        const auto index = static_cast<std::size_t> (promptChips.size() - 1);

        chip->setComponentID (collaboratorId::quickPrompts);
        chip->onClick = [this, index]
        {
            // A quick prompt is an opening, not a message: it fills the composer
            // and stops there.
            panel.useQuickPrompt (index);
            composer.setText (juce::String { panel.composerText() }, false);
            composer.grabKeyboardFocus();
            refresh();
        };
        addAndMakeVisible (*chip);
    }

    for (auto index = 0; index < promptChips.size(); ++index)
        promptChips[index]->setButtonText (shownPrompts[static_cast<std::size_t> (index)]);

    auto row = getLocalBounds().reduced (appearance.scaled (metrics::panelPadding));

    row = row.removeFromBottom (
                 appearance.scaled (composerHeight + metrics::rowGap + quickPromptRowHeight))
              .removeFromTop (appearance.scaled (quickPromptRowHeight));

    if (promptChips.isEmpty())
        return;

    const auto each = row.getWidth() / promptChips.size();

    for (auto* chip : promptChips)
        chip->setBounds (row.removeFromLeft (each).reduced (appearance.scaled (1)));
}
} // namespace duet::gui
