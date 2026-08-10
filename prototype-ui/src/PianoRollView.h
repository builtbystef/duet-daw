// PROTOTYPE (r4m858) — piano roll: scale highlighting, Fold, smart-tool note editing.
#pragma once
#include "Model.h"

class PianoRollView : public juce::Component
{
public:
    explicit PianoRollView (AppState& a) : app (a) {}

    static constexpr int keysW = 64;
    static constexpr int barH = 26; // top option bar

    double pxPerBeat = 34.0;
    int rowH = 13;
    double scrollBeats = 0.0;
    int scrollRow = 48; // bottom visible pitch when not folded

    // C minor
    static bool inScale (int pitch)
    {
        static const bool deg[12] = { true, false, true, true, false, true, false, true, true, false, true, false };
        return deg[pitch % 12];
    }
    static bool isBlackKey (int p)
    {
        static const bool bk[12] = { false, true, false, true, false, false, true, false, true, false, true, false };
        return bk[p % 12];
    }
    static juce::String pitchName (int p)
    {
        static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (n[p % 12]) + juce::String (p / 12 - 1);
    }

    Clip* clip() const { return app.model.findClip (app.editingClipId); }

    std::vector<int> visiblePitches() const
    {
        std::vector<int> rows;
        if (app.fold)
        {
            auto* c = clip();
            if (c != nullptr)
            {
                for (auto& n : c->notes)
                    if (std::find (rows.begin(), rows.end(), n.pitch) == rows.end())
                        rows.push_back (n.pitch);
                std::sort (rows.begin(), rows.end(), std::greater<>());
            }
            if (rows.empty()) rows.push_back (60);
        }
        else
        {
            int top = scrollRow + (getHeight() - barH) / rowH;
            for (int p = std::min (127, top); p >= std::max (0, scrollRow); --p)
                rows.push_back (p);
        }
        return rows;
    }

    int yForPitch (int pitch, const std::vector<int>& rows) const
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[i] == pitch) return barH + i * rowH;
        return -1000;
    }
    int pitchForY (float y, const std::vector<int>& rows) const
    {
        int idx = (int) ((y - barH) / rowH);
        return (idx >= 0 && idx < (int) rows.size()) ? rows[idx] : -1;
    }
    float xForBeat (double b) const { return (float) (keysW + (b - scrollBeats) * pxPerBeat); }
    double beatForX (float x) const { return scrollBeats + (x - keysW) / pxPerBeat; }

    double gridBeats() const
    {
        if (app.gridChoice > 0) return AppState::gridChoiceBeats (app.gridChoice);
        static const double levels[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        double best = 4.0;
        for (double l : levels) if (l * pxPerBeat >= 16.0) best = l;
        return best;
    }
    double snap (double b, const juce::ModifierKeys& mods) const
    {
        if (mods.isAltDown()) return b;
        auto g = gridBeats();
        return std::round (b / g) * g;
    }

    void paint (juce::Graphics& g) override
    {
        auto& th = app.theme;
        g.fillAll (th.surface);
        auto* c = clip();

        // option bar
        {
            juce::Rectangle<int> bar (0, 0, getWidth(), barH);
            g.setColour (th.raised); g.fillRect (bar);
            g.setColour (th.borderSubtle); g.fillRect (bar.withHeight (1).withY (barH - 1));
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.setColour (th.textSecondary);
            juce::String title = c != nullptr ? c->name : "No clip — double-click a MIDI clip in the arrangement";
            g.drawText (title, 10, 0, 300, barH, juce::Justification::centredLeft);

            auto chip = [&] (juce::Rectangle<int> r, const juce::String& text, bool on)
            {
                g.setColour (on ? th.accentStrong : th.interactive);
                g.fillRoundedRectangle (r.toFloat(), 4.0f);
                g.setColour (on ? th.onAccent : th.textSecondary);
                g.drawText (text, r, juce::Justification::centred);
            };
            chip (scaleChipBounds(), "Scale: C min", app.scaleHighlight);
            chip (foldChipBounds(), "Fold", app.fold);
            chip (noteLenChipBounds(), "Len: " + noteLenName(), false);
        }

        if (c == nullptr) return;

        auto rows = visiblePitches();
        auto area = getLocalBounds().withTrimmedTop (barH);

        // rows
        g.saveState();
        g.reduceClipRegion (area.withTrimmedLeft (keysW));
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            int p = rows[i];
            int y = barH + i * rowH;
            bool white = ! isBlackKey (p);
            juce::Colour bg = white ? th.keyWhite : th.keyBlack;
            if (app.scaleHighlight && ! inScale (p))
                bg = bg.overlaidWith (th.overlaySoft);
            g.setColour (bg);
            g.fillRect (keysW, y, getWidth() - keysW, rowH);
            g.setColour (th.borderSubtle.withAlpha (0.5f));
            g.fillRect (keysW, y + rowH - 1, getWidth() - keysW, 1);
        }

        // grid
        const double g1 = gridBeats();
        const double endB = beatForX ((float) getWidth());
        for (double b = std::max (0.0, std::floor (beatForX ((float) keysW) / g1) * g1); b <= endB; b += g1)
        {
            float x = xForBeat (b);
            bool isBar = std::abs (std::fmod (b, (double) app.model.beatsPerBar)) < 1e-6;
            bool isBeat = std::abs (b - std::round (b)) < 1e-6;
            g.setColour (isBar ? th.gridBar : (isBeat ? th.gridBeat : th.gridFine));
            g.fillRect (juce::Rectangle<float> (x, (float) barH, 1.0f, (float) getHeight()));
        }

        // clip extent shading (past clip end)
        {
            float endX = xForBeat (c->len);
            g.setColour (th.overlaySoft);
            g.fillRect (juce::Rectangle<float> (endX, (float) barH, (float) getWidth() - endX, (float) getHeight()));
        }

        // notes
        auto col = th.trackColors[trackColorFor (c)];
        for (auto& n : c->notes)
        {
            int y = yForPitch (n.pitch, rows);
            juce::Rectangle<float> r (xForBeat (n.start), (float) y + 1.0f,
                                      (float) (n.len * pxPerBeat) - 1.0f, (float) rowH - 2.0f);
            g.setColour (col.withAlpha (0.95f));
            g.fillRoundedRectangle (r, 2.0f);
            if (n.selected)
            {
                g.setColour (th.accentBright);
                g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.2f);
            }
        }

        // rubber band
        if (dragMode == Rubber)
        {
            g.setColour (th.accentDefault.withAlpha (0.12f)); g.fillRect (rubberRect);
            g.setColour (th.accentDefault.withAlpha (0.5f)); g.drawRect (rubberRect, 1);
        }

        // playhead (clip-relative)
        double ph = app.playheadBeats - clipStart (c);
        if (ph >= 0)
        {
            g.setColour (th.accentBright);
            g.fillRect (juce::Rectangle<float> (xForBeat (ph), (float) barH, 1.5f, (float) getHeight()));
        }
        g.restoreState();

        // keys column
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            int p = rows[i];
            int y = barH + i * rowH;
            bool white = ! isBlackKey (p);
            g.setColour (white ? th.keyWhite.brighter (app.theme.dark ? 0.5f : 0.0f) : th.keyBlack);
            if (! app.theme.dark)
                g.setColour (white ? juce::Colours::white : th.keyBlack);
            g.fillRect (0, y, keysW, rowH);
            g.setColour (th.borderSubtle);
            g.fillRect (0, y + rowH - 1, keysW, 1);
            if (p % 12 == 0 || app.fold)
            {
                g.setFont (juce::Font (juce::FontOptions (9.0f)));
                g.setColour (th.textMuted);
                g.drawText (pitchName (p), 4, y, keysW - 8, rowH, juce::Justification::centredLeft);
            }
        }
        g.setColour (th.borderDefault);
        g.fillRect (keysW - 1, barH, 1, getHeight() - barH);
    }

    int trackColorFor (Clip* c) const
    {
        for (auto& t : app.model.tracks)
            for (auto& cc : t.clips)
                if (cc.id == c->id) return t.colorIndex;
        return 0;
    }
    double clipStart (Clip* c) const
    {
        for (auto& t : app.model.tracks)
            for (auto& cc : t.clips)
                if (cc.id == c->id) return cc.start;
        return 0;
    }

    juce::Rectangle<int> scaleChipBounds() const { return { getWidth() - 262, 4, 92, barH - 9 }; }
    juce::Rectangle<int> foldChipBounds() const { return { getWidth() - 162, 4, 52, barH - 9 }; }
    juce::Rectangle<int> noteLenChipBounds() const { return { getWidth() - 102, 4, 92, barH - 9 }; }

    juce::String noteLenName() const
    {
        switch (app.noteLenChoiceBeats4) { case 16: return "1 bar"; case 8: return "1/2"; case 4: return "1/4";
                                           case 2: return "1/8"; case 1: return "1/16"; default: return "1/4"; }
    }

    //==============================================================
    enum DragMode { NoDrag, MoveNotes, TrimNote, Rubber };
    DragMode dragMode = NoDrag;
    Note* dragNote = nullptr;
    double dragOffset = 0; double origStart = 0, origLen = 0; int dragPitchAnchor = 0;
    juce::Rectangle<int> rubberRect; juce::Point<int> rubberAnchor;

    Note* hitNote (juce::Point<float> pos, const std::vector<int>& rows)
    {
        auto* c = clip(); if (c == nullptr) return nullptr;
        int p = pitchForY (pos.y, rows);
        for (auto it = c->notes.rbegin(); it != c->notes.rend(); ++it)
            if (it->pitch == p && pos.x >= xForBeat (it->start) && pos.x <= xForBeat (it->start + it->len))
                return &(*it);
        return nullptr;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y < barH)
        {
            if (scaleChipBounds().contains (e.getPosition())) { app.scaleHighlight = ! app.scaleHighlight; app.refresh(); }
            else if (foldChipBounds().contains (e.getPosition())) { app.fold = ! app.fold; app.refresh(); }
            else if (noteLenChipBounds().contains (e.getPosition()))
            {
                juce::PopupMenu menu; menu.setLookAndFeel (&app.lnf);
                struct Opt { const char* n; int v; };
                for (auto o : { Opt { "1 bar", 16 }, Opt { "1/2", 8 }, Opt { "1/4", 4 }, Opt { "1/8", 2 }, Opt { "1/16", 1 } })
                    menu.addItem (o.n, true, app.noteLenChoiceBeats4 == o.v, [this, v = o.v] { app.noteLenChoiceBeats4 = v; app.refresh(); });
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                    localAreaToGlobal (noteLenChipBounds())));
            }
            return;
        }
        auto* c = clip(); if (c == nullptr) return;
        auto rows = visiblePitches();
        auto* n = hitNote (e.position, rows);
        if (n == nullptr)
        {
            if (! e.mods.isCtrlDown()) app.model.clearSelection();
            dragMode = Rubber; rubberAnchor = e.getPosition(); rubberRect = {};
            app.refresh(); return;
        }
        if (! n->selected)
        {
            if (! e.mods.isCtrlDown()) app.model.clearSelection();
            n->selected = true;
        }
        else if (e.mods.isCtrlDown()) { n->selected = false; app.refresh(); dragMode = NoDrag; return; }

        origStart = n->start; origLen = n->len; dragPitchAnchor = n->pitch;
        float x2 = xForBeat (n->start + n->len);
        if (x2 - e.position.x < 5.0f) dragMode = TrimNote;
        else if (e.mods.isCtrlDown())
        {
            Note copy = *n; copy.selected = true; n->selected = false;
            c->notes.push_back (copy);
            dragNote = &c->notes.back();
            dragMode = MoveNotes;
            dragOffset = beatForX (e.position.x) - copy.start;
            app.refresh(); return;
        }
        else dragMode = MoveNotes;
        dragNote = n;
        dragOffset = beatForX (e.position.x) - n->start;
        app.refresh();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* c = clip(); if (c == nullptr) return;
        auto rows = visiblePitches();
        if (dragMode == Rubber)
        {
            rubberRect = juce::Rectangle<int> (rubberAnchor, e.getPosition());
            for (auto& n : c->notes)
            {
                juce::Rectangle<int> nr ((int) xForBeat (n.start), yForPitch (n.pitch, rows),
                                         (int) (n.len * pxPerBeat), rowH);
                n.selected = rubberRect.intersects (nr);
            }
            app.refresh(); return;
        }
        if (dragNote == nullptr) return;
        if (dragMode == MoveNotes)
        {
            dragNote->start = std::max (0.0, snap (beatForX (e.position.x) - dragOffset, e.mods));
            if (! app.fold)
            {
                int p = pitchForY (e.position.y, rows);
                if (p >= 0) dragNote->pitch = p;
            }
        }
        else if (dragMode == TrimNote)
        {
            double b = std::max (origStart + 0.0625, snap (beatForX (e.position.x), e.mods));
            dragNote->len = b - origStart;
        }
        app.refresh();
    }

    void mouseUp (const juce::MouseEvent&) override { dragMode = NoDrag; dragNote = nullptr; rubberRect = {}; app.refresh(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto* c = clip(); if (c == nullptr || e.y < barH) return;
        auto rows = visiblePitches();
        auto* n = hitNote (e.position, rows);
        if (n != nullptr)
        {
            c->notes.erase (c->notes.begin() + (n - c->notes.data()));
            app.refresh(); return;
        }
        int p = pitchForY (e.position.y, rows);
        if (p < 0) return;
        Note nn; nn.pitch = p;
        nn.len = app.noteLenChoiceBeats4 / 4.0;
        nn.start = std::max (0.0, snap (beatForX (e.position.x), e.mods));
        app.model.clearSelection(); nn.selected = true;
        c->notes.push_back (nn);
        app.refresh();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCtrlDown() && e.mods.isShiftDown())
            rowH = juce::jlimit (8, 24, rowH + (int) (wheel.deltaY * 12.0f));
        else if (e.mods.isCtrlDown())
        {
            double anchor = beatForX (e.position.x);
            pxPerBeat = juce::jlimit (6.0, 300.0, pxPerBeat * (1.0 + wheel.deltaY * 1.8));
            scrollBeats = std::max (0.0, anchor - (e.position.x - keysW) / pxPerBeat);
        }
        else if (e.mods.isShiftDown())
            scrollBeats = std::max (0.0, scrollBeats - wheel.deltaY * 240.0 / pxPerBeat);
        else if (! app.fold)
            scrollRow = juce::jlimit (0, 110, scrollRow + (int) (wheel.deltaY * 8.0f));
        repaint();
    }

    AppState& app;
};
