// PROTOTYPE (r4m858) — mixer strips with the proposal's teal ghost fader + A/B toggle.
#pragma once
#include "Model.h"

class MixerView : public juce::Component
{
public:
    explicit MixerView (AppState& a) : app (a) {}

    static constexpr int stripW = 92;

    // A/B: when true, faders show (and "play") the proposed values.
    bool abListenProposed = true;

    juce::Rectangle<int> stripBounds (int i) const { return { i * stripW, 0, stripW, getHeight() }; }
    juce::Rectangle<int> faderBounds (int i) const
    {
        auto s = stripBounds (i);
        return { s.getX() + 14, 64, 20, getHeight() - 64 - 34 };
    }
    juce::Rectangle<int> meterBounds (int i) const
    {
        auto f = faderBounds (i);
        return { f.getRight() + 10, f.getY(), 8, f.getHeight() };
    }
    juce::Rectangle<int> abBounds (int i) const
    {
        auto s = stripBounds (i);
        return { s.getX() + 8, 40, stripW - 16, 16 };
    }

    static float dbFor (float v) { return juce::jmap (v, 0.0f, 1.0f, -60.0f, 6.0f); }

    void paint (juce::Graphics& g) override
    {
        auto& th = app.theme;
        auto& m = app.model;
        g.fillAll (th.surface);
        auto teal = th.teal();
        bool aud = m.proposal.state == Proposal::Auditioning;

        for (int i = 0; i < (int) m.tracks.size(); ++i)
        {
            auto& t = m.tracks[i];
            auto s = stripBounds (i);
            g.setColour (th.borderSubtle);
            g.fillRect (s.getRight() - 1, 0, 1, getHeight());

            // name + color
            g.setColour (th.trackColors[t.colorIndex]);
            g.fillRect (s.getX(), 0, stripW - 1, 3);
            g.setColour (th.textPrimary);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (t.name, s.reduced (6, 0).withY (8).withHeight (14), juce::Justification::centred);

            // M/S
            auto msRow = juce::Rectangle<int> (s.getX() + 14, 22, stripW - 28, 15);
            auto mB = msRow.removeFromLeft ((stripW - 28) / 2 - 2);
            auto sB = msRow.withTrimmedLeft (4);
            g.setColour (t.mute ? th.warning : th.interactive); g.fillRoundedRectangle (mB.toFloat(), 3.0f);
            g.setColour (t.solo ? th.info : th.interactive); g.fillRoundedRectangle (sB.toFloat(), 3.0f);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.setColour (t.mute ? th.onAccent : th.textSecondary); g.drawText ("M", mB, juce::Justification::centred);
            g.setColour (t.solo ? th.onAccent : th.textSecondary); g.drawText ("S", sB, juce::Justification::centred);

            // A/B chip when this strip carries a proposal fader, only while auditioning
            if (t.hasProposalFader && aud)
            {
                auto ab = abBounds (i);
                g.setColour (teal.withAlpha (0.18f));
                g.fillRoundedRectangle (ab.toFloat(), 3.0f);
                g.setColour (teal);
                g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
                g.drawText (abListenProposed ? "B: PROPOSED" : "A: CURRENT", ab, juce::Justification::centred);
            }

            // fader track
            auto f = faderBounds (i);
            g.setColour (th.meterTrack);
            g.fillRoundedRectangle (f.toFloat().withWidth (4.0f).withX ((float) f.getCentreX() - 2.0f), 2.0f);

            auto handleY = [&f] (float v) { return (float) f.getBottom() - v * (float) f.getHeight(); };

            // ghost fader: proposal value, teal, translucent, with glow
            if (t.hasProposalFader && (m.proposal.state == Proposal::Pending || aud))
            {
                bool included = true;
                for (auto& e : m.proposal.elements)
                    if (e.trackIndex == i && e.clipId == 0) included = e.included;
                if (included)
                {
                    float gy = handleY (t.proposalFader);
                    juce::Rectangle<float> gh ((float) f.getX() - 2.0f, gy - 5.0f, (float) f.getWidth() + 4.0f, 10.0f);
                    for (int k = 3; k >= 1; --k)
                    {
                        g.setColour (teal.withAlpha (0.09f * (float) (4 - k)));
                        g.drawRoundedRectangle (gh.expanded ((float) k * 1.5f), 3.0f, 2.0f);
                    }
                    g.setColour (teal.withAlpha (aud && abListenProposed ? 0.95f : 0.45f));
                    g.fillRoundedRectangle (gh, 3.0f);
                }
            }

            // real fader handle
            {
                float fy = handleY (t.fader);
                juce::Rectangle<float> h ((float) f.getX(), fy - 6.0f, (float) f.getWidth(), 12.0f);
                g.setColour (th.interactive.brighter (0.15f));
                g.fillRoundedRectangle (h, 3.0f);
                g.setColour (th.accentStrong);
                g.fillRect (juce::Rectangle<float> (h.getX() + 2.0f, fy - 1.0f, h.getWidth() - 4.0f, 2.0f));
            }

            // static meter
            {
                auto mt = meterBounds (i);
                g.setColour (th.meterTrack); g.fillRect (mt);
                float level = 0.15f + 0.55f * t.fader * (t.mute ? 0.0f : 1.0f);
                auto lit = mt.toFloat().removeFromBottom (mt.toFloat().getHeight() * level);
                g.setColour (th.success.withAlpha (0.8f)); g.fillRect (lit);
            }

            // dB label
            g.setColour (th.textMuted);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            float shown = (aud && abListenProposed && t.hasProposalFader) ? t.proposalFader : t.fader;
            g.drawText (juce::String (dbFor (shown), 1) + " dB",
                        s.withY (getHeight() - 26).withHeight (14).reduced (4, 0), juce::Justification::centred);
        }
    }

    int stripAt (float x) const
    {
        int i = (int) (x / stripW);
        return (i >= 0 && i < (int) app.model.tracks.size()) ? i : -1;
    }

    int dragStrip = -1;

    void mouseDown (const juce::MouseEvent& e) override
    {
        int i = stripAt (e.position.x);
        if (i < 0) return;
        auto& t = app.model.tracks[i];
        bool aud = app.model.proposal.state == Proposal::Auditioning;
        if (t.hasProposalFader && aud && abBounds (i).contains (e.getPosition()))
        {
            abListenProposed = ! abListenProposed; app.refresh(); return;
        }
        // M/S
        auto msRow = juce::Rectangle<int> (stripBounds (i).getX() + 14, 22, stripW - 28, 15);
        auto mB = msRow.removeFromLeft ((stripW - 28) / 2 - 2);
        auto sB = msRow.withTrimmedLeft (4);
        if (mB.contains (e.getPosition())) { t.mute = ! t.mute; app.refresh(); return; }
        if (sB.contains (e.getPosition())) { t.solo = ! t.solo; app.refresh(); return; }

        if (faderBounds (i).expanded (6, 0).contains (e.getPosition()))
        {
            dragStrip = i;
            mouseDrag (e);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStrip < 0) return;
        auto f = faderBounds (dragStrip);
        float v = juce::jlimit (0.0f, 1.0f, ((float) f.getBottom() - e.position.y) / (float) f.getHeight());
        app.model.tracks[dragStrip].fader = v;
        app.refresh();
    }

    void mouseUp (const juce::MouseEvent&) override { dragStrip = -1; }

    AppState& app;
};
