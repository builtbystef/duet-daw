// PROTOTYPE (r4m858) — top transport bar + Duet menu + theme toggle + teal tuner.
#pragma once
#include "Model.h"

class TealTunerPopup : public juce::Component
{
public:
    TealTunerPopup (AppState& a) : app (a)
    {
        auto make = [this] (juce::Slider& s, double min, double max, double val)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
            s.setRange (min, max, 0.001);
            s.setValue (val, juce::dontSendNotification);
            s.setLookAndFeel (&app.lnf);
            s.onValueChange = [this] { push(); };
            addAndMakeVisible (s);
        };
        make (hue, 0.0, 1.0, app.theme.tealHue);
        make (sat, 0.0, 1.0, app.theme.tealSat);
        make (bri, 0.0, 1.0, app.theme.tealBri);
        addAndMakeVisible (hexLabel);
        hexLabel.setJustificationType (juce::Justification::centredLeft);
        hexLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
        setSize (280, 150);
        push();
    }
    ~TealTunerPopup() override { hue.setLookAndFeel (nullptr); sat.setLookAndFeel (nullptr); bri.setLookAndFeel (nullptr); }

    void push()
    {
        app.theme.tealHue = (float) hue.getValue();
        app.theme.tealSat = (float) sat.getValue();
        app.theme.tealBri = (float) bri.getValue();
        hexLabel.setText ("teal = #" + app.theme.teal().toDisplayString (false).toLowerCase()
                          + (app.theme.dark ? "  (dark)" : "  (light)"), juce::dontSendNotification);
        hexLabel.setColour (juce::Label::textColourId, app.theme.textPrimary);
        app.refresh();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (app.theme.raised);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.setColour (app.theme.textMuted);
        g.drawText ("HUE", 10, 12, 40, 14, juce::Justification::centredLeft);
        g.drawText ("SAT", 10, 44, 40, 14, juce::Justification::centredLeft);
        g.drawText ("BRI", 10, 76, 40, 14, juce::Justification::centredLeft);
        g.setColour (app.theme.teal());
        g.fillRoundedRectangle (10.0f, 108.0f, 34.0f, 30.0f, 4.0f);
    }

    void resized() override
    {
        hue.setBounds (46, 8, getWidth() - 56, 22);
        sat.setBounds (46, 40, getWidth() - 56, 22);
        bri.setBounds (46, 72, getWidth() - 56, 22);
        hexLabel.setBounds (54, 108, getWidth() - 60, 30);
    }

    AppState& app;
    juce::Slider hue, sat, bri;
    juce::Label hexLabel;
};

//==============================================================================
class TransportBar : public juce::Component
{
public:
    explicit TransportBar (AppState& a) : app (a)
    {
        addAndMakeVisible (gridBox);
        gridBox.setLookAndFeel (&app.lnf);
        auto names = AppState::gridChoiceNames();
        for (int i = 0; i < names.size(); ++i) gridBox.addItem (names[i], i + 1);
        gridBox.setSelectedId (1, juce::dontSendNotification);
        gridBox.onChange = [this] { app.gridChoice = gridBox.getSelectedId() - 1; app.refresh(); };
    }
    ~TransportBar() override { gridBox.setLookAndFeel (nullptr); }

    juce::ComboBox gridBox;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> hits;

    juce::String barBeatText() const
    {
        int bpb = app.model.beatsPerBar;
        int bar = (int) (app.playheadBeats / bpb) + 1;
        int beat = (int) std::fmod (app.playheadBeats, (double) bpb) + 1;
        int tick = (int) (std::fmod (app.playheadBeats, 1.0) * 960);
        return juce::String (bar).paddedLeft ('0', 3) + "." + juce::String (beat) + "." + juce::String (tick).paddedLeft ('0', 3);
    }
    juce::String wallTimeText() const
    {
        double secs = app.playheadBeats * 60.0 / app.model.bpm;
        int mins = (int) (secs / 60);
        return juce::String (mins).paddedLeft ('0', 2) + ":" + juce::String (secs - mins * 60, 2).paddedLeft ('0', 5);
    }

    void resized() override
    {
        gridBox.setBounds (getWidth() / 2 + 178, 9, 76, getHeight() - 18);
    }

    void paint (juce::Graphics& g) override
    {
        auto& th = app.theme;
        hits.clear();
        g.fillAll (th.raised);
        g.setColour (th.borderDefault);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);

        auto text = [&] (juce::Rectangle<int> r, const juce::String& s, juce::Colour c, float size, bool bold = false)
        {
            g.setColour (c);
            g.setFont (juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)));
            g.drawText (s, r, juce::Justification::centred);
        };
        auto chipBtn = [&] (juce::Rectangle<int> r, const juce::String& label, bool on, const juce::String& id, juce::Colour onCol)
        {
            g.setColour (on ? onCol : th.interactive);
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            text (r, label, on ? th.onAccent : th.textSecondary, 10.0f, true);
            hits.push_back ({ r, id });
        };

        int h = getHeight();
        int cy = 8, ch = h - 16;

        // left: Duet menu + project name
        juce::Rectangle<int> menuB (10, cy, 52, ch);
        g.setColour (th.interactive); g.fillRoundedRectangle (menuB.toFloat(), 4.0f);
        text (menuB, "Duet", th.textPrimary, 12.0f, true);
        hits.push_back ({ menuB, "menu" });
        text ({ 72, 0, 160, h }, "Demo Project", th.textMuted, 11.0f);
        g.setColour (th.textMuted);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("Demo Project", 72, 0, 160, h, juce::Justification::centredLeft);

        // centre cluster
        int cx = getWidth() / 2;
        // transport buttons
        juce::Rectangle<int> playB (cx - 210, cy, 34, ch), stopB (cx - 172, cy, 34, ch), recB (cx - 134, cy, 34, ch);
        g.setColour (app.playing ? th.success : th.interactive); g.fillRoundedRectangle (playB.toFloat(), 4.0f);
        g.setColour (th.interactive); g.fillRoundedRectangle (stopB.toFloat(), 4.0f);
        g.setColour (app.recording ? th.danger : th.interactive); g.fillRoundedRectangle (recB.toFloat(), 4.0f);
        { // glyphs
            g.setColour (app.playing ? th.onAccent : th.textSecondary);
            juce::Path p; auto b = playB.toFloat().withSizeKeepingCentre (10, 12);
            p.addTriangle (b.getX(), b.getY(), b.getX(), b.getBottom(), b.getRight(), b.getCentreY());
            g.fillPath (p);
            g.setColour (th.textSecondary);
            g.fillRect (stopB.toFloat().withSizeKeepingCentre (10, 10));
            g.setColour (app.recording ? th.onAccent : th.danger);
            g.fillEllipse (recB.toFloat().withSizeKeepingCentre (10, 10));
        }
        hits.push_back ({ playB, "play" }); hits.push_back ({ stopB, "stop" }); hits.push_back ({ recB, "rec" });

        // readouts
        juce::Rectangle<int> pos (cx - 92, cy, 96, ch);
        g.setColour (th.canvas); g.fillRoundedRectangle (pos.toFloat(), 4.0f);
        text (pos, barBeatText(), th.textPrimary, 13.0f);
        juce::Rectangle<int> wall (cx + 8, cy, 66, ch);
        g.setColour (th.canvas); g.fillRoundedRectangle (wall.toFloat(), 4.0f);
        text (wall, wallTimeText(), th.textMuted, 11.0f);

        text ({ cx + 80, 0, 46, h }, juce::String (app.model.bpm, 1), th.textPrimary, 13.0f);
        text ({ cx + 80, h - 15, 46, 12 }, "BPM", th.textDisabled, 8.0f);
        text ({ cx + 128, 0, 34, h }, "4/4", th.textSecondary, 12.0f);

        // toggles
        chipBtn ({ cx + 262, cy, 26, ch }, "L", app.loopOn, "loop", th.accentStrong);
        chipBtn ({ cx + 292, cy, 26, ch }, "M", app.metronomeOn, "metro", th.accentStrong);
        chipBtn ({ cx + 322, cy, 26, ch }, "F", app.followOn, "follow", th.accentStrong);

        // right side: CPU + health, undo/redo, theme, teal tuner
        int rx = getWidth() - 10;
        auto right = [&] (int w) { rx -= w + 6; return juce::Rectangle<int> (rx + 6, cy, w, ch); };

        auto tealB = right (46);
        g.setColour (app.theme.teal()); g.drawRoundedRectangle (tealB.toFloat(), 4.0f, 1.2f);
        text (tealB, "Teal", app.theme.teal(), 10.0f, true);
        hits.push_back ({ tealB, "teal" });

        auto themeB = right (34);
        g.setColour (th.interactive); g.fillRoundedRectangle (themeB.toFloat(), 4.0f);
        text (themeB, th.dark ? juce::String::fromUTF8 ("\xe2\x98\xbc") : juce::String::fromUTF8 ("\xe2\x98\xbd"), th.textSecondary, 13.0f);
        hits.push_back ({ themeB, "theme" });

        auto redoB = right (30);
        auto undoB = right (30);
        g.setColour (th.interactive);
        g.fillRoundedRectangle (undoB.toFloat(), 4.0f);
        g.fillRoundedRectangle (redoB.toFloat(), 4.0f);
        text (undoB, juce::String::fromUTF8 ("\xe2\x86\xb6"), th.textSecondary, 12.0f);
        text (redoB, juce::String::fromUTF8 ("\xe2\x86\xb7"), th.textDisabled, 12.0f);

        auto cpu = right (76);
        g.setColour (th.success); g.fillEllipse ((float) cpu.getX(), (float) cpu.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (th.textMuted);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("CPU 3%", cpu.withTrimmedLeft (12), juce::Justification::centredLeft);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& [r, id] : hits)
            if (r.contains (e.getPosition())) { act (id, r); return; }
    }

    void act (const juce::String& id, juce::Rectangle<int> r)
    {
        if (id == "play") app.playing = ! app.playing;
        else if (id == "stop") { app.playing = false; app.playheadBeats = app.loopOn ? app.loopStartBeats : 0.0; }
        else if (id == "rec") app.recording = ! app.recording;
        else if (id == "loop") app.loopOn = ! app.loopOn;
        else if (id == "metro") app.metronomeOn = ! app.metronomeOn;
        else if (id == "follow") app.followOn = ! app.followOn;
        else if (id == "theme")
        {
            float th2 = app.theme.tealHue, ts = app.theme.tealSat, tb = app.theme.tealBri;
            bool wasDark = app.theme.dark;
            app.theme = wasDark ? Theme::lightTheme() : Theme::darkTheme();
            juce::ignoreUnused (th2, ts, tb); // each mode keeps its own tuned teal default
        }
        else if (id == "teal")
        {
            auto popup = std::make_unique<TealTunerPopup> (app);
            juce::CallOutBox::launchAsynchronously (std::move (popup), localAreaToGlobal (r), nullptr);
            return;
        }
        else if (id == "menu")
        {
            juce::PopupMenu menu; menu.setLookAndFeel (&app.lnf);
            for (auto* s : { "New Project", "Open Project...", "Save", "Save As...", "Recent" })
                menu.addItem (s, [] {});
            menu.addSeparator();
            for (auto* s : { "Export / Bounce...", "Audio & MIDI Settings...", "Plugin Scan..." })
                menu.addItem (s, [] {});
            menu.addSeparator();
            menu.addItem ("Toggle Browser (B)", [this] { app.browserVisible = ! app.browserVisible; app.refresh(); });
            menu.addItem ("Toggle Collaborator (C)", [this] { app.collabVisible = ! app.collabVisible; app.refresh(); });
            menu.addItem ("Toggle Bottom Panel (E)", [this] { app.bottomVisible = ! app.bottomVisible; app.refresh(); });
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (juce::Rectangle<int> (10, 8, 52, getHeight() - 16))));
            return;
        }
        app.refresh();
    }

    AppState& app;
};
