// PROTOTYPE (r4m858) — left browser dock and right Collaborator dock.
#pragma once
#include "Model.h"

//==============================================================================
class BrowserPanel : public juce::Component
{
public:
    explicit BrowserPanel (AppState& a) : app (a)
    {
        addAndMakeVisible (search);
        search.setTextToShowWhenEmpty ("Search...", juce::Colours::grey);
        search.onTextChange = [this] { repaint(); };

        items = {
            { "SAMPLES", "", false }, { "", "Kick_Punchy_01.wav", true }, { "", "Snare_Tight.wav", false },
            { "", "HatLoop_124.wav", false }, { "", "Vox_Chop_Am.wav", false },
            { "INSTRUMENTS", "", false }, { "", "Duet Poly", true }, { "", "Duet Sampler", false },
            { "EFFECTS", "", false }, { "", "Duet EQ", false }, { "", "Duet Compressor", true }, { "", "Duet Reverb", false },
            { "PLUGINS (VST3)", "", false }, { "", "Vital", false }, { "", "ValhallaSupermassive", false }, { "", "TDR Nova", false },
        };
    }

    struct Item { juce::String section, name; bool fav; };
    std::vector<Item> items;
    juce::TextEditor search;

    void resized() override { search.setBounds (getLocalBounds().removeFromTop (46).reduced (10, 10)); }

    void paint (juce::Graphics& g) override
    {
        auto& th = app.theme;
        g.fillAll (th.surface);
        g.setColour (th.borderDefault);
        g.fillRect (getWidth() - 1, 0, 1, getHeight());

        auto filter = search.getText().trim();
        int y = 52;
        for (auto& it : items)
        {
            if (it.section.isNotEmpty())
            {
                g.setFont (juce::Font (juce::FontOptions (9.0f)));
                g.setColour (th.textDisabled);
                g.drawText (it.section, 12, y + 6, getWidth() - 24, 12, juce::Justification::centredLeft);
                y += 24;
                continue;
            }
            if (filter.isNotEmpty() && ! it.name.containsIgnoreCase (filter)) continue;
            g.setFont (juce::Font (juce::FontOptions (11.5f)));
            g.setColour (th.textSecondary);
            g.drawText (it.name, 22, y, getWidth() - 50, 20, juce::Justification::centredLeft);
            g.setColour (it.fav ? th.warning : th.textDisabled.withAlpha (0.45f));
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (it.fav ? juce::String::fromUTF8 ("\xe2\x98\x85") : juce::String::fromUTF8 ("\xe2\x98\x86"),
                        getWidth() - 26, y, 16, 20, juce::Justification::centred);
            y += 21;
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // favorites toggle (hit the star column)
        auto filter = search.getText().trim();
        int y = 52;
        for (auto& it : items)
        {
            if (it.section.isNotEmpty()) { y += 24; continue; }
            if (filter.isNotEmpty() && ! it.name.containsIgnoreCase (filter)) continue;
            if (e.x > getWidth() - 30 && e.y >= y && e.y < y + 21) { it.fav = ! it.fav; repaint(); return; }
            y += 21;
        }
    }

    AppState& app;
};

//==============================================================================
class CollaboratorPanel : public juce::Component
{
public:
    explicit CollaboratorPanel (AppState& a) : app (a) {}

    juce::Rectangle<int> auditionBtn, acceptBtn, rejectBtn;
    std::vector<std::pair<juce::Rectangle<int>, int>> checkboxes; // bounds → element index
    juce::Rectangle<int> historyHeader;
    bool historyOpen = true;

    void paint (juce::Graphics& g) override
    {
        auto& th = app.theme;
        auto& m = app.model;
        auto teal = th.teal();
        g.fillAll (th.surface);
        g.setColour (th.borderDefault);
        g.fillRect (0, 0, 1, getHeight());

        auto area = getLocalBounds().withTrimmedLeft (1).reduced (12, 0);
        int y = 10;

        // header
        g.setColour (teal);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (juce::String::fromUTF8 ("\xe2\x9c\xa6 Collaborator"), area.getX(), y, area.getWidth(), 18, juce::Justification::centredLeft);
        y += 26;

        // context chip
        {
            juce::Rectangle<int> chip (area.getX(), y, area.getWidth(), 22);
            g.setColour (th.raised); g.fillRoundedRectangle (chip.toFloat(), 4.0f);
            g.setColour (th.borderSubtle); g.drawRoundedRectangle (chip.toFloat(), 4.0f, 1.0f);
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.setColour (th.textMuted);
            g.drawText ("Context: " + m.selectionSummary(), chip.reduced (8, 0), juce::Justification::centredLeft);
            y += 30;
        }

        // conversation snippets
        auto message = [&] (const juce::String& who, const juce::String& text, bool ai)
        {
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.setColour (ai ? teal : th.textDisabled);
            g.drawText (who, area.getX(), y, area.getWidth(), 11, juce::Justification::centredLeft);
            y += 13;
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.setColour (th.textSecondary);
            juce::Rectangle<int> tr (area.getX() + (ai ? 8 : 0), y, area.getWidth() - 8, 200);
            if (ai) { g.setColour (teal.withAlpha (0.5f)); g.fillRect (area.getX(), y, 2, textHeight (text, tr.getWidth())); g.setColour (th.textSecondary); }
            g.drawMultiLineText (text, tr.getX(), y + 10, tr.getWidth());
            y += textHeight (text, tr.getWidth()) + 10;
        };
        message ("YOU", "The mix feels empty in the second half.", false);
        message ("COLLABORATOR", "Measured the arrangement: Pad stops at bar 3 while Keys sustain to bar 8. That absence is most of the emptiness. I have a proposal.", true);

        // proposal card
        if (m.proposal.state == Proposal::Pending || m.proposal.state == Proposal::Auditioning)
        {
            checkboxes.clear();
            juce::Rectangle<int> card (area.getX(), y, area.getWidth(), 0);
            int cy = y + 10;
            bool aud = m.proposal.state == Proposal::Auditioning;

            // measure card height first (elements + text)
            int textH = textHeight (m.proposal.summary, area.getWidth() - 20);
            int cardH = 10 + 16 + 6 + textH + 10 + (int) m.proposal.elements.size() * 20 + 12 + 26 + 10;
            card.setHeight (cardH);

            // glow + fill
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (teal.withAlpha (0.05f * (float) (4 - k)));
                g.drawRoundedRectangle (card.toFloat().expanded ((float) k), 6.0f, 2.0f);
            }
            g.setColour (th.raised); g.fillRoundedRectangle (card.toFloat(), 5.0f);
            g.setColour (teal.withAlpha (aud ? 0.9f : 0.55f));
            g.drawRoundedRectangle (card.toFloat(), 5.0f, aud ? 1.5f : 1.0f);

            auto inner = card.reduced (10, 0);
            g.setColour (teal);
            g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
            g.drawText (juce::String::fromUTF8 ("\xe2\x9c\xa6 ") + m.proposal.title, inner.getX(), cy, inner.getWidth(), 16, juce::Justification::centredLeft);
            cy += 22;

            g.setColour (th.textSecondary);
            g.setFont (juce::Font (juce::FontOptions (10.5f)));
            g.drawMultiLineText (m.proposal.summary, inner.getX(), cy + 9, inner.getWidth());
            cy += textH + 10;

            // elements with cherry-pick checkboxes
            g.setFont (juce::Font (juce::FontOptions (10.5f)));
            for (int i = 0; i < (int) m.proposal.elements.size(); ++i)
            {
                auto& el = m.proposal.elements[i];
                juce::Rectangle<int> cb (inner.getX(), cy + 3, 13, 13);
                checkboxes.push_back ({ cb, i });
                g.setColour (el.included ? teal : th.interactive);
                if (el.included) g.fillRoundedRectangle (cb.toFloat(), 3.0f);
                else g.drawRoundedRectangle (cb.toFloat(), 3.0f, 1.0f);
                if (el.included)
                {
                    g.setColour (th.dark ? th.onAccent : juce::Colours::white);
                    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
                    g.drawText (juce::String::fromUTF8 ("\xe2\x9c\x93"), cb, juce::Justification::centred);
                    g.setFont (juce::Font (juce::FontOptions (10.5f)));
                }
                g.setColour (el.included ? th.textPrimary : th.textDisabled);
                g.drawText (el.description, cb.getRight() + 7, cy, inner.getWidth() - 22, 18, juce::Justification::centredLeft);
                cy += 20;
            }
            cy += 8;

            // buttons
            auto btnRow = juce::Rectangle<int> (inner.getX(), cy, inner.getWidth(), 24);
            int bw = (inner.getWidth() - 12) / 3;
            auditionBtn = btnRow.removeFromLeft (bw); btnRow.removeFromLeft (6);
            acceptBtn = btnRow.removeFromLeft (bw); btnRow.removeFromLeft (6);
            rejectBtn = btnRow;

            auto button = [&] (juce::Rectangle<int> b, const juce::String& text, juce::Colour bg, juce::Colour fg, bool outline)
            {
                g.setColour (bg);
                if (outline) g.drawRoundedRectangle (b.toFloat(), 4.0f, 1.2f);
                else g.fillRoundedRectangle (b.toFloat(), 4.0f);
                g.setColour (fg);
                g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
                g.drawText (text, b, juce::Justification::centred);
            };
            button (auditionBtn, aud ? "Stop Audition" : "Audition", teal, aud ? teal : teal, true);
            if (aud) { g.setColour (teal.withAlpha (0.15f)); g.fillRoundedRectangle (auditionBtn.toFloat(), 4.0f); }
            button (acceptBtn, "Accept", teal, th.dark ? th.canvas : juce::Colours::white, false);
            button (rejectBtn, "Reject", th.borderDefault, th.textSecondary, true);

            y = card.getBottom() + 14;
        }
        else
        {
            g.setColour (th.textMuted);
            g.setFont (juce::Font (juce::FontOptions (10.5f)));
            g.drawText (m.proposal.state == Proposal::Accepted ? "Proposal accepted." : "Proposal rejected.",
                        area.getX(), y, area.getWidth(), 16, juce::Justification::centredLeft);
            y += 24;
        }

        // history
        historyHeader = { area.getX(), y, area.getWidth(), 18 };
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.setColour (th.textDisabled);
        g.drawText ((historyOpen ? juce::String::fromUTF8 ("\xe2\x96\xbe HISTORY") : juce::String::fromUTF8 ("\xe2\x96\xb8 HISTORY")),
                    historyHeader, juce::Justification::centredLeft);
        y += 22;
        if (historyOpen)
        {
            for (auto& h : app.model.history)
            {
                g.setFont (juce::Font (juce::FontOptions (10.5f)));
                g.setColour (th.textMuted);
                g.drawText (h.title, area.getX() + 4, y, area.getWidth() - 60, 16, juce::Justification::centredLeft);
                if (h.stale)
                {
                    juce::Rectangle<int> chip (area.getRight() - 46, y + 1, 42, 14);
                    g.setColour (th.warning.withAlpha (0.18f)); g.fillRoundedRectangle (chip.toFloat(), 3.0f);
                    g.setColour (th.warning);
                    g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
                    g.drawText ("STALE", chip, juce::Justification::centred);
                }
                y += 19;
            }
        }
    }

    int textHeight (const juce::String& text, int width) const
    {
        juce::Font f (juce::FontOptions (10.5f));
        juce::GlyphArrangement ga;
        int lines = 1; float x = 0;
        juce::StringArray words; words.addTokens (text, " ", "");
        for (auto& w : words)
        {
            auto ww = juce::GlyphArrangement::getStringWidth (f, w + " ");
            if (x + ww > (float) width) { ++lines; x = 0; }
            x += ww;
        }
        return lines * 14 + 4;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& m = app.model;
        for (auto& [b, i] : checkboxes)
            if (b.expanded (2).contains (e.getPosition()))
            {
                auto& el = m.proposal.elements[i];
                el.included = ! el.included;
                app.refresh(); return;
            }
        if (m.proposal.state == Proposal::Pending || m.proposal.state == Proposal::Auditioning)
        {
            if (auditionBtn.contains (e.getPosition()))
            {
                m.proposal.state = m.proposal.state == Proposal::Auditioning ? Proposal::Pending : Proposal::Auditioning;
                app.refresh(); return;
            }
            if (acceptBtn.contains (e.getPosition())) { applyProposal (true); return; }
            if (rejectBtn.contains (e.getPosition())) { applyProposal (false); return; }
        }
        if (historyHeader.contains (e.getPosition())) { historyOpen = ! historyOpen; repaint(); }
    }

    void applyProposal (bool accept)
    {
        auto& m = app.model;
        for (auto& el : m.proposal.elements)
        {
            bool keep = accept && el.included;
            if (el.clipId != 0)
            {
                int ti; auto* c = m.findClip (el.clipId, &ti);
                if (c == nullptr) continue;
                if (keep) c->proposal = false; // ghost becomes a real clip
                else
                {
                    auto& clips = m.tracks[ti].clips;
                    clips.erase (std::remove_if (clips.begin(), clips.end(),
                                                 [&] (const Clip& x) { return x.id == el.clipId; }),
                                 clips.end());
                }
            }
            else if (el.trackIndex >= 0)
            {
                auto& t = m.tracks[el.trackIndex];
                if (keep) t.fader = t.proposalFader;
                t.hasProposalFader = false;
            }
        }
        m.proposal.state = accept ? Proposal::Accepted : Proposal::Rejected;
        if (accept) m.history.insert (m.history.begin(), { m.proposal.title, false });
        app.refresh();
    }

    AppState& app;
};
