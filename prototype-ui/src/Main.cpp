// PROTOTYPE (r4m858) — Duet milestone-one UI prototype. Disposable; never ship.
#include "Model.h"
#include "ArrangementView.h"
#include "PianoRollView.h"
#include "MixerView.h"
#include "SidePanels.h"
#include "TransportBar.h"

//==============================================================================
class DividerBar : public juce::Component
{
public:
    DividerBar (bool vertical, std::function<void (int)> onDragFn)
        : isVertical (vertical), onDrag (std::move (onDragFn))
    {
        setMouseCursor (vertical ? juce::MouseCursor::LeftRightResizeCursor
                                 : juce::MouseCursor::UpDownResizeCursor);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto p = e.getEventRelativeTo (getParentComponent()).getPosition();
        onDrag (isVertical ? p.x : p.y);
    }
    bool isVertical; std::function<void (int)> onDrag;
};

//==============================================================================
class BottomPanel : public juce::Component
{
public:
    BottomPanel (AppState& a, PianoRollView& pr, MixerView& mx) : app (a), pianoRoll (pr), mixer (mx)
    {
        addAndMakeVisible (pianoRoll);
        addAndMakeVisible (mixer);
    }

    static constexpr int tabH = 28;

    void resized() override
    {
        auto content = getLocalBounds().withTrimmedTop (tabH);
        pianoRoll.setBounds (content);
        mixer.setBounds (content);
        pianoRoll.setVisible (app.bottomTab == 0);
        mixer.setVisible (app.bottomTab == 1);
    }

    void paint (juce::Graphics& g) override
    {
        auto& th = app.theme;
        auto bar = getLocalBounds().removeFromTop (tabH);
        g.setColour (th.raised); g.fillRect (bar);
        g.setColour (th.borderDefault); g.fillRect (bar.withHeight (1));
        auto tab = [&] (juce::Rectangle<int> r, const juce::String& s, bool on)
        {
            if (on) { g.setColour (th.surface); g.fillRect (r); g.setColour (th.accentStrong); g.fillRect (r.withHeight (2)); }
            g.setColour (on ? th.textPrimary : th.textMuted);
            g.setFont (juce::Font (juce::FontOptions (11.0f, on ? juce::Font::bold : juce::Font::plain)));
            g.drawText (s, r, juce::Justification::centred);
        };
        tab (tab1(), "Piano Roll  (P)", app.bottomTab == 0);
        tab (tab2(), "Mixer  (X)", app.bottomTab == 1);
    }
    juce::Rectangle<int> tab1() const { return { 0, 0, 110, tabH }; }
    juce::Rectangle<int> tab2() const { return { 110, 0, 90, tabH }; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (tab1().contains (e.getPosition())) { app.bottomTab = 0; app.refresh(); }
        else if (tab2().contains (e.getPosition())) { app.bottomTab = 1; app.refresh(); }
    }

    AppState& app; PianoRollView& pianoRoll; MixerView& mixer;
};

//==============================================================================
class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent()
    {
        seedDemoProject (app.model);
        juce::Desktop::getInstance().setGlobalScaleFactor (1.25f); // "everything a bit bigger" — stands in for Settings > Interface scaling
        app.lnf.apply (app.theme);
        juce::LookAndFeel::setDefaultLookAndFeel (&app.lnf);

        addAndMakeVisible (transport);
        addAndMakeVisible (arrangement);
        addAndMakeVisible (browser);
        addAndMakeVisible (collaborator);
        addAndMakeVisible (bottom);
        addAndMakeVisible (browserDivider);
        addAndMakeVisible (collabDivider);
        addAndMakeVisible (bottomDivider);

        app.refreshAll = [this]
        {
            app.lnf.apply (app.theme);
            resized();
            bottom.resized(); // tab visibility lives in its resized(); same-bounds setBounds skips it
            repaint();
        };

        setWantsKeyboardFocus (true);
        startTimerHz (60);
        setSize (1500, 900);
    }
    ~MainComponent() override { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    int browserW = 232, collabW = 312, bottomH = 280;

    void resized() override
    {
        auto r = getLocalBounds();
        transport.setBounds (r.removeFromTop (44));

        if (app.bottomVisible)
        {
            bottom.setBounds (r.removeFromBottom (bottomH));
            bottomDivider.setBounds (juce::Rectangle<int> (0, bottom.getY() - 3, getWidth(), 6));
        }
        bottom.setVisible (app.bottomVisible);
        bottomDivider.setVisible (app.bottomVisible);

        if (app.browserVisible)
        {
            browser.setBounds (r.removeFromLeft (browserW));
            browserDivider.setBounds (juce::Rectangle<int> (browser.getRight() - 3, browser.getY(), 6, browser.getHeight()));
        }
        browser.setVisible (app.browserVisible);
        browserDivider.setVisible (app.browserVisible);

        if (app.collabVisible)
        {
            collaborator.setBounds (r.removeFromRight (collabW));
            collabDivider.setBounds (juce::Rectangle<int> (collaborator.getX() - 3, collaborator.getY(), 6, collaborator.getHeight()));
        }
        collaborator.setVisible (app.collabVisible);
        collabDivider.setVisible (app.collabVisible);

        arrangement.setBounds (r);
    }

    void paint (juce::Graphics& g) override { g.fillAll (app.theme.canvas); }

    bool keyPressed (const juce::KeyPress& k) override
    {
        auto c = k.getTextCharacter();
        if (k == juce::KeyPress::spaceKey)
        {
            app.playing = ! app.playing;
            app.refresh(); return true;
        }
        if (c == 'r' || c == 'R') { app.recording = ! app.recording; app.refresh(); return true; }
        if (c == 'l' || c == 'L') { app.loopOn = ! app.loopOn; app.refresh(); return true; }
        if (c == 'm' || c == 'M') { app.metronomeOn = ! app.metronomeOn; app.refresh(); return true; }
        if (c == 'f' || c == 'F') { app.followOn = ! app.followOn; app.refresh(); return true; }
        if (c == 'b' || c == 'B') { app.browserVisible = ! app.browserVisible; app.refresh(); return true; }
        if (c == 'c' || c == 'C') { app.collabVisible = ! app.collabVisible; app.refresh(); return true; }
        if (c == 'e' || c == 'E') { app.bottomVisible = ! app.bottomVisible; app.refresh(); return true; }
        if (c == 'p' || c == 'P') { app.bottomVisible = true; app.bottomTab = 0; app.refresh(); return true; }
        if (c == 'x' || c == 'X') { app.bottomVisible = true; app.bottomTab = 1; app.refresh(); return true; }
        if (c == '+' || c == '=') { arrangement.nudgeZoom (1.3); return true; }
        if (c == '-') { arrangement.nudgeZoom (1.0 / 1.3); return true; }
        if (c == '0') { arrangement.zoomToFit(); return true; }
        if (k == juce::KeyPress::homeKey) { app.playheadBeats = 0; arrangement.ensurePlayheadVisible(); app.refresh(); return true; }
        if (k == juce::KeyPress::endKey)
        {
            double last = 0;
            for (auto& t : app.model.tracks) for (auto& cl : t.clips) last = std::max (last, cl.start + cl.len);
            app.playheadBeats = last; arrangement.ensurePlayheadVisible(); app.refresh(); return true;
        }
        if (k == juce::KeyPress::escapeKey) { app.model.clearSelection(); app.refresh(); return true; }
        if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
        {
            for (auto& t : app.model.tracks)
            {
                t.clips.erase (std::remove_if (t.clips.begin(), t.clips.end(),
                                               [] (const Clip& x) { return x.selected && ! x.proposal; }),
                               t.clips.end());
                for (auto& cl : t.clips)
                    cl.notes.erase (std::remove_if (cl.notes.begin(), cl.notes.end(),
                                                    [] (const Note& n) { return n.selected; }),
                                    cl.notes.end());
            }
            app.refresh(); return true;
        }
        if (k.getModifiers().isCtrlDown() && (c == 'a' || c == 'A'))
        {
            for (auto& t : app.model.tracks) for (auto& cl : t.clips) cl.selected = true;
            app.refresh(); return true;
        }
        return false;
    }

    void timerCallback() override
    {
        if (! app.playing) return;
        app.playheadBeats += app.model.bpm / 60.0 / 60.0; // beats per frame at 60 fps
        if (app.loopOn && app.playheadBeats >= app.loopStartBeats + app.loopLenBeats)
            app.playheadBeats = app.loopStartBeats;
        if (app.followOn) arrangement.ensurePlayheadVisible();
        arrangement.repaint();
        transport.repaint();
        if (app.bottomVisible && app.bottomTab == 0) pianoRoll.repaint();
    }

    AppState app;
    TransportBar transport { app };
    ArrangementView arrangement { app };
    PianoRollView pianoRoll { app };
    MixerView mixer { app };
    BrowserPanel browser { app };
    CollaboratorPanel collaborator { app };
    BottomPanel bottom { app, pianoRoll, mixer };

    DividerBar browserDivider { true, [this] (int x) { browserW = juce::jlimit (160, 420, x); resized(); } };
    DividerBar collabDivider { true, [this] (int x) { collabW = juce::jlimit (220, 480, getWidth() - x); resized(); } };
    DividerBar bottomDivider { false, [this] (int y) { bottomH = juce::jlimit (140, 500, getHeight() - y); resized(); } };
};

//==============================================================================
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow()
        : DocumentWindow ("Duet UI PROTOTYPE",
                          juce::Colours::black,
                          DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent(), true);
        setResizable (true, true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class DuetUiPrototypeApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Duet UI PROTOTYPE"; }
    const juce::String getApplicationVersion() override { return "0.0.1"; }
    void initialise (const juce::String&) override { window = std::make_unique<MainWindow>(); }
    void shutdown() override { window = nullptr; }
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (DuetUiPrototypeApp)
