#include <duet/gui/CollaboratorPanelCanvas.h>

#include <duet/gui/CollaboratorPainting.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <cmath>
#include <string>
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
/** One Suggestion, as a card in the conversation.

    A teal glow border round the whole of it, the summary and the stale mark
    across the top, one ticked row per Element, and the three things the producer
    can do with it. The card decides nothing: every gesture on it goes to the
    Suggestions view-model, and what it draws is what that says.
*/
class CollaboratorPanelCanvas::SuggestionCard final : public juce::Component
{
public:
    SuggestionCard (Appearance& lookAndScale,
                    Suggestions& pending,
                    std::string suggestionId,
                    std::string suggestionSummary)
        : appearance (lookAndScale), suggestions (pending), id (std::move (suggestionId)),
          summary (std::move (suggestionSummary))
    {
        setComponentID (collaboratorId::suggestionCard);

        auditionButton.setComponentID (collaboratorId::suggestionAudition);
        compareButton.setComponentID (collaboratorId::suggestionCompare);
        acceptButton.setComponentID (collaboratorId::suggestionAccept);
        rejectButton.setComponentID (collaboratorId::suggestionReject);

        auditionButton.setButtonText (Suggestions::auditionLabel);
        compareButton.setButtonText ("A/B");
        acceptButton.setButtonText (Suggestions::acceptLabel);
        rejectButton.setButtonText (Suggestions::rejectLabel);

        // Audition is a place the producer goes and comes back from, so the
        // button that takes them in is the one that brings them out.
        auditionButton.onClick = [this]
        {
            if (suggestions.isAuditioning (id))
                suggestions.stopAudition();
            else
                suggestions.audition (id);

            refresh();
        };

        compareButton.onClick = [this]
        {
            suggestions.toggleAB();
            refresh();
        };

        acceptButton.onClick = [this]
        {
            suggestions.accept (id);
            refresh();
        };

        rejectButton.onClick = [this]
        {
            suggestions.reject (id);
            refresh();
        };

        addAndMakeVisible (auditionButton);
        addChildComponent (compareButton);
        addAndMakeVisible (acceptButton);
        addAndMakeVisible (rejectButton);
        refresh();
    }

    ~SuggestionCard() override = default;

    /** How tall this card is at the width the panel has, which is what the
        conversation lays the entries out with.
    */
    [[nodiscard]] int heightFor() const
    {
        const auto* view = suggestions.card (id);

        // A Suggestion the producer has resolved keeps its place in the
        // conversation and nothing else: the summary line stays as the record of
        // what was offered, and the card around it goes.
        if (view == nullptr)
            return appearance.scaled (suggestionHeaderHeight + (2 * bubblePadding));

        return appearance.scaled (suggestionHeaderHeight + suggestionButtonRowHeight
                                  + (2 * bubblePadding))
               + (static_cast<int> (view->elements.size())
                  * appearance.scaled (suggestionElementHeight));
    }

    /** Takes what the Suggestion says now onto the card: the rows it still has,
        the ticks the producer has made, and whether it is being heard.
    */
    void refresh()
    {
        const auto* view = suggestions.card (id);

        if (view == nullptr)
        {
            ticks.clear();

            for (auto* button : std::initializer_list<juce::Button*> {
                     &auditionButton, &compareButton, &acceptButton, &rejectButton })
                button->setVisible (false);

            repaint();
            return;
        }

        auditionButton.setVisible (true);
        acceptButton.setVisible (true);
        rejectButton.setVisible (true);

        while (ticks.size() > static_cast<int> (view->elements.size()))
            ticks.removeLast();

        while (ticks.size() < static_cast<int> (view->elements.size()))
        {
            auto* tick = ticks.add (new juce::ToggleButton {});
            const auto element = static_cast<std::size_t> (ticks.size() - 1);

            tick->setComponentID (collaboratorId::suggestionElement);
            tick->onClick = [this, element]
            {
                suggestions.setChecked (
                    id, element, ticks[static_cast<int> (element)]->getToggleState());
                refresh();
            };
            addAndMakeVisible (*tick);
        }

        for (auto element = 0; element < ticks.size(); ++element)
        {
            const auto index = static_cast<std::size_t> (element);

            ticks[element]->setToggleState (suggestions.isChecked (id, index),
                                            juce::dontSendNotification);
            ticks[element]->setButtonText (view->elements[index].description);

            // An Element the producer has unticked reads at the excluded
            // intensity, the whole row of it: the box and the words alike, and
            // the same weight its ghosts are drawn at.
            ticks[element]->setAlpha (static_cast<float> (suggestions.intensityOf (id, index)));
        }

        compareButton.setVisible (suggestions.isAuditioning (id));
        resized();
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (appearance.scaled (bubblePadding));

        area.removeFromTop (appearance.scaled (suggestionHeaderHeight));

        for (auto* tick : ticks)
            tick->setBounds (area.removeFromTop (appearance.scaled (suggestionElementHeight)));

        auto buttons = area.removeFromTop (appearance.scaled (suggestionButtonRowHeight));
        const auto each = juce::jmax (1, buttons.getWidth() / (compareButton.isVisible() ? 4 : 3));

        auditionButton.setBounds (buttons.removeFromLeft (each).reduced (appearance.scaled (1)));

        if (compareButton.isVisible())
            compareButton.setBounds (buttons.removeFromLeft (each).reduced (appearance.scaled (1)));

        acceptButton.setBounds (buttons.removeFromLeft (each).reduced (appearance.scaled (1)));
        rejectButton.setBounds (buttons.reduced (appearance.scaled (1)));
    }

    void paint (juce::Graphics& g) override
    {
        const auto* view = suggestions.card (id);
        const auto teal = toJuce (appearance.colour (ColourToken::collaborator));
        const auto radius = static_cast<float> (appearance.scaled (metrics::radiusLarge));
        const auto bounds = getLocalBounds().toFloat().reduced (1.0F);

        if (view == nullptr)
        {
            paintResolved (g, teal);
            return;
        }

        g.setColour (teal.withAlpha (0.08F));
        g.fillRoundedRectangle (bounds, radius);

        for (auto ring = Suggestions::glowRings; ring > 0; --ring)
        {
            g.setColour (teal.withAlpha (0.10F / static_cast<float> (ring)));
            g.drawRoundedRectangle (
                bounds.reduced (static_cast<float> (ring) - 1.0F), radius, 1.0F);
        }

        g.setColour (teal.withAlpha (0.65F));
        g.drawRoundedRectangle (bounds, radius, 1.0F);

        auto header = getLocalBounds()
                          .reduced (appearance.scaled (bubblePadding))
                          .removeFromTop (appearance.scaled (suggestionHeaderHeight));
        const auto badge = header.removeFromLeft (appearance.scaled (badgeSize)).toFloat();

        paintCollaboratorBadge (
            g, badge.withSizeKeepingCentre (badge.getWidth(), badge.getWidth()), teal);
        header.removeFromLeft (appearance.scaled (metrics::rowGap));

        if (view->stale)
        {
            // Stale keeps the warning hue: it is a fact about the Suggestion and
            // not a second meaning for the Collaborator's own.
            auto mark = header.removeFromRight (appearance.scaled (34));

            g.setColour (toJuce (appearance.colour (ColourToken::semanticWarning)));
            g.setFont (interFont (appearance.scaled (typography::eyebrow), true));
            g.drawText (Suggestions::staleLabel, mark, juce::Justification::centredRight);
        }

        g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
        g.setFont (interFont (appearance.scaled (typography::body), true));
        g.drawText (view->summary, header, juce::Justification::centredLeft, true);
    }

private:
    /** What is left of a Suggestion the producer has resolved: the badge and the
        summary it was offered under, in the muted ink the conversation gives
        anything that is no longer live. Where it went — accepted, and into which
        Elements — is the History section's, which is spec js437t's.
    */
    void paintResolved (juce::Graphics& g, juce::Colour teal)
    {
        auto line = getLocalBounds()
                        .reduced (appearance.scaled (bubblePadding))
                        .removeFromTop (appearance.scaled (suggestionHeaderHeight));
        const auto badge = line.removeFromLeft (appearance.scaled (badgeSize)).toFloat();

        paintCollaboratorBadge (g,
                                badge.withSizeKeepingCentre (badge.getWidth(), badge.getWidth()),
                                teal.withAlpha (0.4F));
        line.removeFromLeft (appearance.scaled (metrics::rowGap));

        g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
        g.setFont (interFont (appearance.scaled (typography::body)));
        g.drawText (summary, line, juce::Justification::centredLeft, true);
    }

    Appearance& appearance;
    Suggestions& suggestions;
    std::string id;
    std::string summary;
    juce::OwnedArray<juce::ToggleButton> ticks;
    juce::TextButton auditionButton;
    juce::TextButton compareButton;
    juce::TextButton acceptButton;
    juce::TextButton rejectButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuggestionCard)
};

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

        // The conversation itself is a painting and takes no click; the
        // Suggestion cards in it are the one thing here that does.
        setInterceptsMouseClicks (false, true);
    }

    ~Conversation() override = default;

    /** The pending Suggestions the cards in this conversation are of. */
    void setSuggestions (Suggestions* pending) { suggestions = pending; }

    /** Takes what every card's Suggestion says now onto that card. */
    void refreshCards()
    {
        for (auto* suggestionCard : cards)
            suggestionCard->refresh();
    }

    /** Measures every entry at the width the panel has now and takes the height
        they need. The viewport is what scrolls the result.
    */
    void layOutEntries (int width)
    {
        laidOut.clear();
        syncCards();

        const auto padding = appearance.scaled (bubblePadding);
        const auto gap = appearance.scaled (bubbleGap);
        const auto font = interFont (appearance.scaled (typography::body));
        const auto chipFont = interFont (appearance.scaled (typography::eyebrow));
        auto y = gap;
        auto cardIndex = 0;

        for (const auto& entry : panel.conversation())
        {
            if (entry.kind == EntryKind::suggestion)
            {
                // A card is a component and not a drawing, because the producer
                // ticks and presses things on it.
                if (cardIndex < cards.size())
                {
                    auto* suggestionCard = cards[cardIndex++];
                    const auto height = suggestionCard->heightFor();

                    suggestionCard->setBounds (0, y, width, height);
                    y += height + gap;
                }

                continue;
            }

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

    /** One card per Suggestion entry, made when the entry arrives and kept for
        as long as it is in the conversation.
    */
    void syncCards()
    {
        std::vector<std::pair<std::string, std::string>> wanted;

        for (const auto& entry : panel.conversation())
            if (entry.kind == EntryKind::suggestion)
                wanted.emplace_back (entry.suggestion, entry.text);

        if (wanted == shown)
            return;

        cards.clear();
        shown = wanted;

        if (suggestions == nullptr)
        {
            shown.clear();
            return;
        }

        for (const auto& [id, summary] : shown)
            addAndMakeVisible (
                cards.add (new SuggestionCard { appearance, *suggestions, id, summary }));
    }

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

            // A Suggestion is a card rather than a run of text, and paints
            // itself; there is no ink for the conversation to give it.
            case EntryKind::suggestion:
                break;
        }

        return ColourToken::textPrimary;
    }

    Appearance& appearance;
    CollaboratorPanel& panel;
    Suggestions* suggestions = nullptr;
    juce::OwnedArray<SuggestionCard> cards;
    std::vector<std::pair<std::string, std::string>> shown;
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
    conversation->setSuggestions (suggestions);
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

void CollaboratorPanelCanvas::setSuggestions (Suggestions* pendingSuggestions)
{
    suggestions = pendingSuggestions;
    conversation->setSuggestions (suggestions);
    shownCards.clear();
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

    // The panel is the one surface that polls, so it is where the Suggestions
    // are taken again — which is what makes a Suggestion the producer has edited
    // under go stale on its card and on its ghosts without either being asked.
    if (suggestions != nullptr)
        suggestions->refresh();

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

    // A card grows and shrinks with the Suggestion under it — an accepted
    // Element leaves a row behind — so what the cards are worth is measured and
    // the conversation is laid out again when the answer changes.
    if (auto shape = cardShape(); shape != shownCards)
    {
        shownCards = std::move (shape);
        conversation->refreshCards();
        conversation->layOutEntries (scroller.getMaximumVisibleWidth());
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

std::string CollaboratorPanelCanvas::cardShape() const
{
    if (suggestions == nullptr)
        return {};

    // Everything a card draws differently, in one string: which Suggestions are
    // pending, how many Elements each has left, which are ticked, whether it is
    // stale, and whether it is the one being heard.
    std::string shape;

    for (const auto& pending : suggestions->cards())
    {
        shape += pending.id;
        shape += pending.stale ? '!' : '.';

        if (! suggestions->isAuditioning (pending.id))
            shape += '-';
        else
            shape += suggestions->hearingProposed() ? 'B' : 'A';

        for (std::size_t element = 0; element < pending.elements.size(); ++element)
            shape += suggestions->isChecked (pending.id, element) ? 'x' : 'o';

        shape += ';';
    }

    return shape;
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
