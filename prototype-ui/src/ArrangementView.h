// PROTOTYPE (r4m858) — arrangement timeline: adaptive grid, Ctrl-scroll zoom,
// smart-tool clip editing, ghost+glow proposal clips.
#pragma once
#include "Model.h"

class ArrangementView : public juce::Component
{
public:
    explicit ArrangementView (AppState& a) : app (a) { setWantsKeyboardFocus (false); }

    static constexpr int headerW = 170;
    static constexpr int rulerH = 26;

    double pxPerBeat = 26.0;
    int trackH = 56;
    double scrollBeats = 0.0; // leftmost visible beat
    int scrollY = 0;

    //==============================================================
    double gridBeats() const
    {
        if (app.gridChoice > 0)
            return AppState::gridChoiceBeats (app.gridChoice);
        static const double levels[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        double best = 4.0;
        for (double l : levels)
            if (l * pxPerBeat >= 18.0) best = l;
        return best;
    }

    double snap (double beats, const juce::ModifierKeys& mods) const
    {
        if (mods.isAltDown()) return beats;
        auto g = gridBeats();
        return std::round (beats / g) * g;
    }

    float xForBeat (double b) const { return (float) (headerW + (b - scrollBeats) * pxPerBeat); }
    double beatForX (float x) const { return scrollBeats + (x - headerW) / pxPerBeat; }
    int trackIndexForY (float y) const
    {
        int idx = (int) ((y - rulerH + scrollY) / trackH);
        return (idx >= 0 && idx < (int) app.model.tracks.size()) ? idx : -1;
    }
    int yForTrack (int i) const { return rulerH + i * trackH - scrollY; }

    //==============================================================
    void paint (juce::Graphics& g) override
    {
        auto& th = app.theme;
        auto& m = app.model;
        g.fillAll (th.canvas);

        auto lanesArea = getLocalBounds().withTrimmedLeft (headerW).withTrimmedTop (rulerH);

        // ---- lanes background + grid
        g.saveState();
        g.reduceClipRegion (lanesArea);
        for (int i = 0; i < (int) m.tracks.size(); ++i)
        {
            juce::Rectangle<int> lane (headerW, yForTrack (i), getWidth() - headerW, trackH);
            g.setColour (i % 2 == 0 ? th.surface : th.canvas);
            g.fillRect (lane);
            g.setColour (th.borderSubtle);
            g.fillRect (lane.removeFromBottom (1));
        }

        const double g1 = gridBeats();
        const double startB = std::floor (beatForX ((float) headerW) / g1) * g1;
        const double endB = beatForX ((float) getWidth());
        const int bpb = m.beatsPerBar;
        for (double b = std::max (0.0, startB); b <= endB; b += g1)
        {
            float x = xForBeat (b);
            bool isBar = std::abs (std::fmod (b, (double) bpb)) < 1e-6;
            bool isBeat = std::abs (b - std::round (b)) < 1e-6;
            g.setColour (isBar ? th.gridBar : (isBeat ? th.gridBeat : th.gridFine));
            g.fillRect (juce::Rectangle<float> (x, (float) rulerH, 1.0f, (float) getHeight()));
        }

        // ---- clips
        for (int i = 0; i < (int) m.tracks.size(); ++i)
            for (auto& c : m.tracks[i].clips)
                drawClip (g, c, i);

        // rubber band
        if (dragMode == Rubber)
        {
            g.setColour (th.accentDefault.withAlpha (0.12f));
            g.fillRect (rubberRect);
            g.setColour (th.accentDefault.withAlpha (0.5f));
            g.drawRect (rubberRect, 1);
        }
        g.restoreState();

        // ---- ruler
        auto ruler = getLocalBounds().removeFromTop (rulerH).withTrimmedLeft (headerW);
        g.setColour (th.surface); g.fillRect (ruler);
        g.setColour (th.borderDefault); g.fillRect (ruler.withHeight (1).withY (rulerH - 1));
        // loop region
        if (app.loopOn)
        {
            float lx1 = xForBeat (app.loopStartBeats), lx2 = xForBeat (app.loopStartBeats + app.loopLenBeats);
            g.setColour (th.accentStrong.withAlpha (0.22f));
            g.fillRect (juce::Rectangle<float> (lx1, 2.0f, lx2 - lx1, (float) rulerH - 5.0f));
        }
        g.saveState();
        g.reduceClipRegion (ruler);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        for (double b = std::max (0.0, std::floor (beatForX ((float) headerW) / bpb) * bpb); b <= endB; b += bpb)
        {
            float x = xForBeat (b);
            g.setColour (th.gridBar); g.fillRect (juce::Rectangle<float> (x, 0.0f, 1.0f, (float) rulerH));
            g.setColour (th.textMuted);
            g.drawText (juce::String ((int) (b / bpb) + 1), (int) x + 4, 2, 40, rulerH - 4, juce::Justification::centredLeft);
        }
        g.restoreState();

        // ---- playhead
        {
            float px = xForBeat (app.playheadBeats);
            if (px >= headerW)
            {
                g.setColour (th.accentBright);
                g.fillRect (juce::Rectangle<float> (px, 0.0f, 1.5f, (float) getHeight()));
                juce::Path tri; tri.addTriangle (px - 5, 0, px + 5, 0, px, 7);
                g.fillPath (tri);
            }
        }

        // ---- track headers (drawn last, over everything scrolled)
        for (int i = 0; i < (int) m.tracks.size(); ++i)
        {
            auto& t = m.tracks[i];
            juce::Rectangle<int> h (0, yForTrack (i), headerW, trackH);
            if (h.getBottom() < rulerH) continue;
            g.setColour (th.raised); g.fillRect (h);
            g.setColour (th.borderSubtle); g.fillRect (h.withHeight (1).withY (h.getBottom() - 1));
            g.setColour (th.trackColors[t.colorIndex]);
            g.fillRect (h.withWidth (3));
            g.setColour (th.textPrimary);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText (t.name, h.reduced (10, 4).withHeight (18), juce::Justification::centredLeft);
            g.setColour (th.textDisabled);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (t.isMidi ? "MIDI" : "AUDIO", h.reduced (10, 4).withHeight (12).withY (h.getY() + 22), juce::Justification::centredLeft);
            // M/S
            auto ms = h.reduced (8, 0).removeFromRight (44).withSizeKeepingCentre (44, 18);
            auto mB = ms.removeFromLeft (20); ms.removeFromLeft (4); auto sB = ms;
            g.setColour (t.mute ? th.warning : th.interactive); g.fillRoundedRectangle (mB.toFloat(), 3.0f);
            g.setColour (t.solo ? th.info : th.interactive); g.fillRoundedRectangle (sB.toFloat(), 3.0f);
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.setColour (t.mute ? th.onAccent : th.textSecondary); g.drawText ("M", mB, juce::Justification::centred);
            g.setColour (t.solo ? th.onAccent : th.textSecondary); g.drawText ("S", sB, juce::Justification::centred);
        }
        // corner
        g.setColour (th.raised); g.fillRect (0, 0, headerW, rulerH);
        g.setColour (th.borderDefault);
        g.fillRect (headerW - 1, 0, 1, getHeight());
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.setColour (th.textDisabled);
        g.drawText ("TRACKS", 10, 0, headerW - 20, rulerH, juce::Justification::centredLeft);
    }

    void drawClip (juce::Graphics& g, Clip& c, int trackIndex)
    {
        auto& th = app.theme;
        float x = xForBeat (c.start), w = (float) (c.len * pxPerBeat);
        juce::Rectangle<float> r (x, (float) yForTrack (trackIndex) + 3.0f, w, (float) trackH - 7.0f);
        if (r.getRight() < headerW || r.getX() > (float) getWidth()) return;

        auto col = th.trackColors[app.model.tracks[trackIndex].colorIndex];

        if (c.proposal)
        {
            drawGhostClip (g, c, r);
            return;
        }

        g.setColour (col.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 3.0f);
        if (c.selected)
        {
            g.setColour (th.accentBright);
            g.drawRoundedRectangle (r.reduced (0.75f), 3.0f, 1.5f);
        }
        g.setColour (th.onTrack.withAlpha (0.9f));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (c.name, r.reduced (5, 2).withHeight (12), juce::Justification::topLeft);
        drawClipContent (g, c, r, th.onTrack.withAlpha (0.35f));
    }

    void drawGhostClip (juce::Graphics& g, Clip& c, juce::Rectangle<float> r)
    {
        auto& th = app.theme;
        auto teal = th.teal();
        bool auditioning = app.model.proposal.state == Proposal::Auditioning;
        bool included = true;
        for (auto& e : app.model.proposal.elements)
            if (e.clipId == c.id) included = e.included;
        float dim = included ? 1.0f : 0.35f;

        // glow: expanding soft strokes
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (teal.withAlpha ((0.10f - 0.025f * i + (auditioning ? 0.05f : 0.0f)) * dim));
            g.drawRoundedRectangle (r.expanded ((float) i * 1.6f), 4.0f, 2.5f);
        }
        // ghost fill
        g.setColour (teal.withAlpha ((auditioning ? 0.26f : 0.12f) * dim));
        g.fillRoundedRectangle (r, 3.0f);
        // dashed border
        {
            juce::Path p; p.addRoundedRectangle (r.reduced (0.75f), 3.0f);
            juce::PathStrokeType st (auditioning ? 1.6f : 1.2f);
            if (! auditioning)
            {
                const float dashes[] = { 4.0f, 3.0f };
                juce::Path dashed; st.createDashedStroke (dashed, p, dashes, 2);
                g.setColour (teal.withAlpha (0.85f * dim)); g.fillPath (dashed);
            }
            else
            {
                g.setColour (teal.withAlpha (0.95f * dim)); g.strokePath (p, st);
            }
        }
        if (c.selected)
        {
            g.setColour (th.accentBright.withAlpha (0.9f));
            g.drawRoundedRectangle (r.expanded (2.5f), 4.0f, 1.0f);
        }
        g.setColour (teal.withAlpha (0.95f * dim));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (juce::String::fromUTF8 ("\xe2\x9c\xa6 ") + c.name, r.reduced (5, 2).withHeight (12), juce::Justification::topLeft);
        drawClipContent (g, c, r, teal.withAlpha (0.55f * dim));
    }

    void drawClipContent (juce::Graphics& g, Clip& c, juce::Rectangle<float> r, juce::Colour ink)
    {
        g.saveState();
        g.reduceClipRegion (r.getSmallestIntegerContainer());
        auto body = r.withTrimmedTop (13.0f).reduced (2.0f);
        g.setColour (ink);
        if (c.isMidi && ! c.notes.empty())
        {
            int lo = 127, hi = 0;
            for (auto& n : c.notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
            lo -= 2; hi += 2;
            float ph = body.getHeight() / (float) (hi - lo + 1);
            for (auto& n : c.notes)
            {
                float nx = r.getX() + (float) (n.start / c.len) * r.getWidth();
                float nw = juce::jmax (2.0f, (float) (n.len / c.len) * r.getWidth() - 1.0f);
                float ny = body.getBottom() - (float) (n.pitch - lo + 1) * ph;
                g.fillRect (juce::Rectangle<float> (nx, ny, nw, juce::jmax (2.0f, ph - 1.0f)));
            }
        }
        else if (! c.isMidi)
        {
            // fake but deterministic waveform
            juce::Random rnd ((juce::int64) c.id * 7919);
            float mid = body.getCentreY();
            for (float px = body.getX(); px < body.getRight(); px += 2.0f)
            {
                float a = (0.25f + 0.75f * rnd.nextFloat()) * body.getHeight() * 0.48f;
                g.fillRect (juce::Rectangle<float> (px, mid - a, 1.4f, a * 2.0f));
            }
        }
        g.restoreState();
    }

    //==============================================================
    enum DragMode { NoDrag, Move, Copy, TrimL, TrimR, Rubber, Scrub };
    DragMode dragMode = NoDrag;
    juce::uint32 dragClipId = 0;
    double dragOffsetBeats = 0; int dragFromTrack = -1;
    double origStart = 0, origLen = 0;
    juce::Rectangle<int> rubberRect;
    juce::Point<int> rubberAnchor;

    Clip* hitClip (juce::Point<float> pos, int& trackOut)
    {
        trackOut = trackIndexForY (pos.y);
        if (trackOut < 0) return nullptr;
        auto& clips = app.model.tracks[trackOut].clips;
        for (auto it = clips.rbegin(); it != clips.rend(); ++it)
        {
            float x1 = xForBeat (it->start), x2 = xForBeat (it->start + it->len);
            if (pos.x >= x1 && pos.x <= x2) return &(*it);
        }
        return nullptr;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (e.y < rulerH || e.x < headerW) { setMouseCursor (juce::MouseCursor::NormalCursor); return; }
        int ti; auto* c = hitClip (e.position, ti);
        if (c != nullptr)
        {
            float x1 = xForBeat (c->start), x2 = xForBeat (c->start + c->len);
            if (e.position.x - x1 < 6.0f || x2 - e.position.x < 6.0f)
            { setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); return; }
        }
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& m = app.model;
        if (e.x < headerW && e.y >= rulerH)
        {
            // header: M/S hit-test, else select-track no-op
            int ti = trackIndexForY ((float) e.y);
            if (ti >= 0)
            {
                juce::Rectangle<int> h (0, yForTrack (ti), headerW, trackH);
                auto ms = h.reduced (8, 0).removeFromRight (44).withSizeKeepingCentre (44, 18);
                auto mB = ms.removeFromLeft (20); ms.removeFromLeft (4); auto sB = ms;
                if (mB.contains (e.getPosition())) m.tracks[ti].mute = ! m.tracks[ti].mute;
                else if (sB.contains (e.getPosition())) m.tracks[ti].solo = ! m.tracks[ti].solo;
                app.refresh();
            }
            return;
        }
        if (e.y < rulerH)
        {
            dragMode = Scrub;
            app.playheadBeats = std::max (0.0, snap (beatForX (e.position.x), e.mods));
            repaint();
            return;
        }

        int ti; auto* c = hitClip (e.position, ti);
        if (c == nullptr)
        {
            if (! e.mods.isCtrlDown() && ! e.mods.isShiftDown()) m.clearSelection();
            dragMode = Rubber;
            rubberAnchor = e.getPosition();
            rubberRect = {};
            app.refresh();
            return;
        }

        if (! c->selected)
        {
            if (! e.mods.isCtrlDown() && ! e.mods.isShiftDown()) m.clearSelection();
            c->selected = true;
        }
        else if (e.mods.isCtrlDown())
        {
            c->selected = false; app.refresh(); dragMode = NoDrag; return;
        }

        float x1 = xForBeat (c->start), x2 = xForBeat (c->start + c->len);
        origStart = c->start; origLen = c->len;
        dragFromTrack = ti;
        if (e.position.x - x1 < 6.0f) dragMode = TrimL;
        else if (x2 - e.position.x < 6.0f) dragMode = TrimR;
        else if (e.mods.isCtrlDown())
        {
            // Ctrl-drag copy: duplicate now, drag the copy
            Clip copy = *c; copy.id = m.newId(); copy.selected = true;
            c->selected = false;
            m.tracks[ti].clips.push_back (copy);
            dragMode = Move;
            dragClipId = copy.id;
            dragOffsetBeats = beatForX (e.position.x) - copy.start;
            app.refresh();
            return;
        }
        else dragMode = Move;
        dragClipId = c->id;
        dragOffsetBeats = beatForX (e.position.x) - c->start;
        app.refresh();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto& m = app.model;
        if (dragMode == Scrub)
        {
            app.playheadBeats = std::max (0.0, snap (beatForX (e.position.x), e.mods));
            repaint(); return;
        }
        if (dragMode == Rubber)
        {
            rubberRect = juce::Rectangle<int> (rubberAnchor, e.getPosition());
            for (int i = 0; i < (int) m.tracks.size(); ++i)
                for (auto& c : m.tracks[i].clips)
                {
                    juce::Rectangle<int> cr ((int) xForBeat (c.start), yForTrack (i) + 3,
                                             (int) (c.len * pxPerBeat), trackH - 7);
                    c.selected = rubberRect.intersects (cr);
                }
            app.refresh(); return;
        }
        if (dragMode == NoDrag) return;
        int ti; auto* c = m.findClip (dragClipId, &ti);
        if (c == nullptr) return;

        if (dragMode == Move)
        {
            double b = snap (beatForX (e.position.x) - dragOffsetBeats, e.mods);
            c->start = std::max (0.0, b);
            int targetTrack = trackIndexForY (e.position.y);
            if (targetTrack >= 0 && targetTrack != ti
                && m.tracks[targetTrack].isMidi == c->isMidi)
            {
                Clip moved = *c;
                auto& clips = m.tracks[ti].clips;
                clips.erase (std::remove_if (clips.begin(), clips.end(),
                                             [&] (const Clip& x) { return x.id == dragClipId; }),
                             clips.end());
                m.tracks[targetTrack].clips.push_back (moved);
            }
        }
        else if (dragMode == TrimL)
        {
            double b = juce::jlimit (0.0, origStart + origLen - 0.125, snap (beatForX (e.position.x), e.mods));
            c->len = origStart + origLen - b; c->start = b;
        }
        else if (dragMode == TrimR)
        {
            double b = std::max (origStart + 0.125, snap (beatForX (e.position.x), e.mods));
            c->len = b - origStart;
        }
        app.refresh();
    }

    void mouseUp (const juce::MouseEvent&) override { dragMode = NoDrag; rubberRect = {}; app.refresh(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.y < rulerH || e.x < headerW) return;
        auto& m = app.model;
        int ti; auto* c = hitClip (e.position, ti);
        if (c != nullptr && c->isMidi)
        {
            app.editingClipId = c->id;
            app.bottomVisible = true; app.bottomTab = 0;
            app.refresh();
            return;
        }
        if (c == nullptr && ti >= 0 && m.tracks[ti].isMidi)
        {
            Clip nc; nc.id = m.newId(); nc.name = "MIDI Clip"; nc.isMidi = true;
            nc.len = std::max (1.0, gridBeats()); // one grid unit, min one beat
            nc.start = std::max (0.0, snap (beatForX (e.position.x), e.mods));
            m.clearSelection(); nc.selected = true;
            m.tracks[ti].clips.push_back (nc);
            app.refresh();
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCtrlDown() && e.mods.isShiftDown())
        {
            trackH = juce::jlimit (28, 120, trackH + (int) (wheel.deltaY * 40.0f));
        }
        else if (e.mods.isCtrlDown())
        {
            double anchor = beatForX (e.position.x);
            pxPerBeat = juce::jlimit (2.0, 200.0, pxPerBeat * (1.0 + wheel.deltaY * 1.8));
            scrollBeats = std::max (0.0, anchor - (e.position.x - headerW) / pxPerBeat);
        }
        else if (e.mods.isShiftDown())
        {
            scrollBeats = std::max (0.0, scrollBeats - wheel.deltaY * 24.0 / pxPerBeat * 10.0);
        }
        else
        {
            int contentH = (int) app.model.tracks.size() * trackH;
            int maxScroll = std::max (0, contentH - (getHeight() - rulerH));
            scrollY = juce::jlimit (0, maxScroll, scrollY - (int) (wheel.deltaY * 60.0f));
        }
        repaint();
    }

    void zoomToFit()
    {
        double lastBeat = 4;
        for (auto& t : app.model.tracks)
            for (auto& c : t.clips)
                lastBeat = std::max (lastBeat, c.start + c.len);
        pxPerBeat = juce::jlimit (2.0, 200.0, (getWidth() - headerW - 20) / lastBeat);
        scrollBeats = 0;
        repaint();
    }

    void nudgeZoom (double factor)
    {
        double anchor = beatForX ((float) (headerW + (getWidth() - headerW) / 2));
        pxPerBeat = juce::jlimit (2.0, 200.0, pxPerBeat * factor);
        scrollBeats = std::max (0.0, anchor - ((getWidth() - headerW) / 2.0) / pxPerBeat);
        repaint();
    }

    void ensurePlayheadVisible()
    {
        float px = xForBeat (app.playheadBeats);
        if (px > getWidth() - 60 || px < headerW)
        {
            scrollBeats = std::max (0.0, app.playheadBeats - 4.0);
            repaint();
        }
    }

    AppState& app;
};
