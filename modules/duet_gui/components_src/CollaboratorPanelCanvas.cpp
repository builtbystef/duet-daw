#include <duet/gui/CollaboratorPanelCanvas.h>

#include <duet/gui/CollaboratorPainting.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/Text.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

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
        redoButton.setComponentID (collaboratorId::suggestionRedo);
        reason.setComponentID (collaboratorId::suggestionReason);

        auditionButton.setButtonText (Suggestions::auditionLabel);
        compareButton.setButtonText ("A/B");
        acceptButton.setButtonText (Suggestions::acceptLabel);
        rejectButton.setButtonText (Suggestions::rejectLabel);
        redoButton.setButtonText (Suggestions::redoLabel);

        reason.setMultiLine (false);
        reason.setTextToShowWhenEmpty (Suggestions::rejectReasonHint,
                                       toJuce (appearance.colour (ColourToken::textMuted)));

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

        // Saying why is what turns a rejection into the next question, so the
        // reason travels with it rather than being typed anywhere else.
        rejectButton.onClick = [this]
        {
            suggestions.reject (id, reason.getText().toStdString());
            refresh();
        };

        redoButton.onClick = [this]
        {
            suggestions.redo (id);
            refresh();
        };

        addAndMakeVisible (auditionButton);
        addChildComponent (compareButton);
        addAndMakeVisible (acceptButton);
        addAndMakeVisible (rejectButton);
        addChildComponent (redoButton);
        addAndMakeVisible (reason);
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

        return appearance.scaled (suggestionHeaderHeight + suggestionReasonHeight
                                  + suggestionButtonRowHeight + (2 * bubblePadding))
               + (static_cast<int> (view->elements.size())
                  * appearance.scaled (suggestionElementHeight))
               + (view->stale ? appearance.scaled (suggestionButtonRowHeight) : 0);
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
                     &auditionButton, &compareButton, &acceptButton, &rejectButton, &redoButton })
                button->setVisible (false);

            reason.setVisible (false);
            repaint();
            return;
        }

        auditionButton.setVisible (true);
        acceptButton.setVisible (true);
        rejectButton.setVisible (true);
        reason.setVisible (true);

        // The redo control belongs to a Suggestion the project has moved under,
        // and there is nothing for it to do on one that still fits.
        redoButton.setVisible (view->stale);

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

        if (redoButton.isVisible())
            redoButton.setBounds (area.removeFromTop (appearance.scaled (suggestionButtonRowHeight))
                                      .reduced (appearance.scaled (1)));

        reason.setBounds (area.removeFromTop (appearance.scaled (suggestionReasonHeight))
                              .reduced (appearance.scaled (1)));

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
    juce::TextButton redoButton;
    juce::TextEditor reason;

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
        syncMarks();

        const auto padding = appearance.scaled (bubblePadding);
        const auto gap = appearance.scaled (bubbleGap);
        const auto font = interFont (appearance.scaled (typography::body));
        const auto chipFont = interFont (appearance.scaled (typography::eyebrow));
        auto y = gap;
        auto cardIndex = 0;
        auto markIndex = 0;

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

            // The mark is a row of its own under what it is a mark on, and the
            // ledger it opens onto is a line per guess under that.
            const auto marked = ! entry.estimates.empty();
            const auto markRoom = marked ? appearance.scaled (estimateMarkHeight) : 0;
            const auto ledgerRoom = marked && entry.estimatesOpen
                                        ? static_cast<int> (entry.estimates.size())
                                              * appearance.scaled (estimateLineHeight)
                                        : 0;

            const auto height = wrappedHeight (juce::String { entry.text }, font, textWidth)
                                + (2 * padding) + chipRoom + markRoom + ledgerRoom;

            EntryLayout laid;
            laid.kind = entry.kind;
            laid.text = juce::String { entry.text };
            laid.context = chip;
            laid.bounds = juce::Rectangle<int> { 0, y, width, height };
            laid.estimates = entry.estimates;
            laid.estimatesOpen = entry.estimatesOpen;

            if (marked && markIndex < marks.size())
            {
                auto* mark = marks[markIndex++];
                const auto row = laid.bounds.reduced (padding)
                                     .withTrimmedLeft (appearance.scaled (accentRuleWidth))
                                     .removeFromBottom (markRoom + ledgerRoom)
                                     .removeFromTop (markRoom);

                mark->setBounds (row);
                mark->setButtonText (entry.estimatesOpen ? utf8 ("Based on estimates — hide")
                                                         : utf8 ("Based on estimates"));
            }

            laidOut.push_back (std::move (laid));
            y += height + gap;
        }

        y = layOutSections (width, y, gap);

        setSize (width, y);
        chipTypeface = chipFont;
        bodyTypeface = font;
    }

    void paint (juce::Graphics& g) override
    {
        for (const auto& entry : laidOut)
            paintEntry (g, entry);

        paintSections (g);
    }

private:
    struct EntryLayout
    {
        EntryKind kind = EntryKind::producer;
        juce::String text;
        juce::String context;
        juce::Rectangle<int> bounds;
        std::vector<EstimateMarkLine> estimates;
        bool estimatesOpen = false;
    };

    /** One section under the conversation: a heading and the lines beneath it. */
    struct Section
    {
        juce::String heading;
        std::vector<juce::String> lines;
        juce::Rectangle<int> bounds;
    };

    /** One entry: its bubble, the chip frozen onto it, the mark under it, and
        what it says.
    */
    void paintEntry (juce::Graphics& g, const EntryLayout& entry)
    {
        auto area = paintBubble (g, entry);

        if (entry.context.isNotEmpty())
            paintChip (g, entry.context, area);

        if (! entry.estimates.empty())
            paintLedger (g, entry, area);

        g.setColour (toJuce (appearance.colour (inkFor (entry.kind))));
        g.setFont (bodyTypeface);
        g.drawFittedText (
            entry.text,
            area,
            juce::Justification::topLeft,
            juce::jmax (1, area.getHeight() / juce::jmax (1, (int) bodyTypeface.getHeight())));
    }

    /** The surface an entry is written on, and what is left inside it: the
        Collaborator's commentary in an accent bubble behind the reserved teal
        rule, a producer's message on the interactive surface, and an ending in
        neither.
    */
    [[nodiscard]] juce::Rectangle<int> paintBubble (juce::Graphics& g, const EntryLayout& entry)
    {
        const auto rule = appearance.scaled (accentRuleWidth);

        if (entry.kind == EntryKind::producer || entry.kind == EntryKind::commentary)
        {
            const auto radius = static_cast<float> (appearance.scaled (metrics::radiusMedium));
            const auto commentary = entry.kind == EntryKind::commentary;

            g.setColour (toJuce (appearance.colour (commentary ? ColourToken::collaborator
                                                               : ColourToken::surfaceInteractive))
                             .withAlpha (commentary ? 0.12F : 1.0F));
            g.fillRoundedRectangle (entry.bounds.toFloat(), radius);

            if (commentary)
            {
                g.setColour (toJuce (appearance.colour (ColourToken::collaborator)));
                g.fillRect (entry.bounds.withWidth (rule));
            }
        }

        return entry.bounds.reduced (appearance.scaled (bubblePadding)).withTrimmedLeft (rule);
    }

    void paintChip (juce::Graphics& g, const juce::String& context, juce::Rectangle<int>& area)
    {
        auto chip = area.removeFromTop (appearance.scaled (chipHeight));

        const auto chipWidth = static_cast<int> (std::ceil (
                                   juce::GlyphArrangement::getStringWidth (chipTypeface, context)))
                               + (2 * appearance.scaled (chipPadding));

        chip = chip.withWidth (juce::jmin (chip.getWidth(), chipWidth));

        g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
        g.fillRoundedRectangle (chip.toFloat(),
                                static_cast<float> (appearance.scaled (metrics::radiusSmall)));
        g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
        g.setFont (chipTypeface);
        g.drawText (context, chip, juce::Justification::centred);

        area.removeFromTop (appearance.scaled (2));
    }

    /** What the estimate mark opens onto: one line per guess, what it was, what
        made it, and how far that routine trusted itself. The mark's own row is
        a button and is not drawn here — what this takes off the entry is the
        room both of them stand in.
    */
    void paintLedger (juce::Graphics& g, const EntryLayout& entry, juce::Rectangle<int>& area)
    {
        const auto lines = entry.estimatesOpen ? static_cast<int> (entry.estimates.size()) : 0;
        auto below = area.removeFromBottom (appearance.scaled (estimateMarkHeight)
                                            + (lines * appearance.scaled (estimateLineHeight)));

        if (! entry.estimatesOpen)
            return;

        below.removeFromTop (appearance.scaled (estimateMarkHeight));
        g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
        g.setFont (chipTypeface);

        for (const auto& line : entry.estimates)
            g.drawText (ledgerLine (line),
                        below.removeFromTop (appearance.scaled (estimateLineHeight)),
                        juce::Justification::centredLeft,
                        true);
    }

    /** One line of the ledger a mark opens onto. */
    [[nodiscard]] static juce::String ledgerLine (const EstimateMarkLine& line)
    {
        return juce::String { line.field } + ": " + juce::String { line.value } + "  ("
               + juce::String { line.method } + ", "
               + juce::String { juce::roundToInt (line.confidence * 100.0) } + "%)";
    }

    /** One button per marked entry, since opening a mark is a gesture and the
        rest of the conversation is a drawing.
    */
    void syncMarks()
    {
        auto wanted = 0;

        for (const auto& entry : panel.conversation())
            if (! entry.estimates.empty())
                ++wanted;

        while (marks.size() > wanted)
            marks.removeLast();

        while (marks.size() < wanted)
        {
            auto* mark = marks.add (new juce::TextButton {});
            mark->setComponentID (collaboratorId::estimateMark);
            addAndMakeVisible (*mark);
        }

        // Which entry each mark opens is the order they appear in, so the
        // bindings are made again whenever the conversation has grown.
        auto markIndex = 0;

        for (std::size_t entry = 0; entry < panel.conversation().size(); ++entry)
        {
            if (panel.conversation()[entry].estimates.empty())
                continue;

            marks[markIndex++]->onClick = [this, entry] { panel.toggleEstimates (entry); };
        }
    }

    /** Measures the History section and the development trace under the
        conversation, and answers where the whole of it ends.
    */
    [[nodiscard]] int layOutSections (int width, int top, int gap)
    {
        sections.clear();

        if (! panel.history().empty())
        {
            Section history;
            history.heading = "History";

            for (const auto& resolved : panel.history())
                history.lines.push_back (juce::String { resolved.summary } + utf8 (" — ")
                                         + juce::String { resolved.outcome });

            sections.push_back (std::move (history));
        }

        // The trace is the developer's window into a run, and an ordinary build
        // keeps none of it, so there is nothing here to lay out.
        if (! panel.toolTrace().empty())
        {
            Section trace;
            trace.heading = "Tool trace";

            for (const auto& call : panel.toolTrace())
                trace.lines.push_back (juce::String { call.tool } + " "
                                       + juce::String { call.arguments } + utf8 (" → ")
                                       + juce::String { call.result });

            sections.push_back (std::move (trace));
        }

        auto y = top;

        for (auto& section : sections)
        {
            const auto height =
                appearance.scaled (sectionHeaderHeight)
                + (static_cast<int> (section.lines.size()) * appearance.scaled (sectionLineHeight));

            section.bounds = juce::Rectangle<int> { 0, y, width, height };
            y += height + gap;
        }

        return y;
    }

    void paintSections (juce::Graphics& g)
    {
        const auto padding = appearance.scaled (bubblePadding);

        for (const auto& section : sections)
        {
            auto area = section.bounds.reduced (padding, 0);

            g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
            g.setFont (interFont (appearance.scaled (typography::eyebrow), true));
            g.drawText (section.heading,
                        area.removeFromTop (appearance.scaled (sectionHeaderHeight)),
                        juce::Justification::centredLeft);

            g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
            g.setFont (chipTypeface);

            for (const auto& line : section.lines)
                g.drawText (line,
                            area.removeFromTop (appearance.scaled (sectionLineHeight)),
                            juce::Justification::centredLeft,
                            true);
        }
    }

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
    juce::OwnedArray<juce::TextButton> marks;
    std::vector<std::pair<std::string, std::string>> shown;
    std::vector<EntryLayout> laidOut;
    std::vector<Section> sections;
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
    shownShape.clear();
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

    // An ask from a clip's or a track's menu leaves the producer typing, so the
    // keyboard is handed over here rather than by whatever asked.
    if (panel.composerWantsKeyboard() && isShowing() && ! composer.hasKeyboardFocus (false))
        composer.grabKeyboardFocus();

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

    // A card grows and shrinks with the Suggestion under it — an accepted
    // Element leaves a row behind — and commentary grows as it streams, so what
    // the conversation is worth is measured and laid out again when the answer
    // changes.
    if (auto shape = conversationShape(); shape != shownShape)
    {
        const auto grew = panel.conversation().size() != shownEntries;

        shownShape = std::move (shape);
        shownEntries = panel.conversation().size();
        conversation->refreshCards();
        conversation->layOutEntries (scroller.getMaximumVisibleWidth());

        // The newest message is the one the producer is waiting for, so the
        // conversation is scrolled to the end of itself as it arrives.
        if (grew)
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
    panel.composerLostKeyboard();

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
    {
        lastFocused = focused;

        // The keyboard is the producer's to move: once it is elsewhere, the
        // panel stops asking for it.
        panel.composerLostKeyboard();
    }
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

std::string CollaboratorPanelCanvas::conversationShape() const
{
    // Everything the conversation draws, in one string. The entries first —
    // their number, the length of each, whether each is marked and whether the
    // mark is open — then the two sections under them, then the cards.
    std::string shape;

    for (const auto& entry : panel.conversation())
    {
        shape += std::to_string (entry.text.size());
        if (entry.estimates.empty())
            shape += '.';
        else
            shape += entry.estimatesOpen ? '+' : '-';

        shape += ',';
    }

    shape += '|';
    shape += std::to_string (panel.history().size());
    shape += '|';
    shape += std::to_string (panel.toolTrace().size());
    shape += '|';

    if (suggestions == nullptr)
        return shape;

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
