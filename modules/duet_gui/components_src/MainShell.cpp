#include <duet/gui/MainShell.h>

#include <duet/gui/BrowserCanvas.h>

#include <duet/gui/AcceleratedSurface.h>
#include <duet/gui/ArrangementCanvas.h>
#include <duet/gui/Brand.h>
#include <duet/gui/CollaboratorPanelCanvas.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/MixerCanvas.h>
#include <duet/gui/PianoRollCanvas.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace duet::gui
{
namespace
{
    // Logical units: the interface scale is what turns one into a pixel. The
    // dock sizes are not here — those are the producer's, and they live in the
    // view state in the pixels the producer dragged them to.
    constexpr int menuButtonWidth = 44;
    constexpr int tabBarHeight = 28;
    constexpr int pianoRollTabWidth = 104;
    constexpr int mixerTabWidth = 78;
    constexpr int accentBarThickness = 2;
    constexpr int dockToggleWidth = 30;
    constexpr int transportButtonWidth = 30;

    /** The menu id an entry of the shell's own has. Zero is JUCE's "nothing
        chosen", and the host's ids start above these.
    */
    [[nodiscard]] int idFor (Command command) { return static_cast<int> (command) + 1; }

    [[nodiscard]] std::optional<Command> commandForMenuId (int itemId)
    {
        for (const auto command : { Command::toggleBrowser,
                                    Command::toggleCollaborator,
                                    Command::toggleBottomPanel,
                                    Command::toggleBottomMaximized,
                                    Command::showPianoRoll,
                                    Command::showMixer })
            if (idFor (command) == itemId)
                return command;

        return std::nullopt;
    }

    /** A menu entry that says which state it is in and which key gets it. */
    void addToggle (juce::PopupMenu& menu,
                    Command command,
                    const juce::String& text,
                    const juce::String& key,
                    bool ticked)
    {
        juce::PopupMenu::Item item { text };

        item.itemID = idFor (command);
        item.isTicked = ticked;
        item.shortcutKeyDescription = key;

        menu.addItem (std::move (item));
    }

    /** True while the producer is typing into something.

        The keyboard policy needs the answer and JUCE knows it: a text field is
        whatever has the focus and takes text.
    */
    [[nodiscard]] bool aTextFieldHasFocus()
    {
        const auto* focused = juce::Component::getCurrentlyFocusedComponent();
        const auto* typing = dynamic_cast<const juce::TextInputTarget*> (focused);

        return typing != nullptr && typing->isTextInputActive();
    }

    /** Which dock a transport-strip toggle stands for: the side of the shell
        that dock occupies.
    */
    enum class DockSide : std::uint8_t
    {
        left,
        bottom,
        right
    };

    /** A dock toggle drawn as the shell in miniature: a window outline with a
        hairline where the dock's boundary runs, and the dock's own region
        filled with the accent while the dock is open — the same accent the tab
        bar names its front tab with.
    */
    class DockToggleButton final : public juce::Button
    {
    public:
        DockToggleButton (Appearance& lookAndScale, DockSide dockSide, const juce::String& name)
            : juce::Button (name), appearance (lookAndScale), side (dockSide)
        {
        }

        ~DockToggleButton() override = default;

        void paintButton (juce::Graphics& g,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
        {
            if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
            {
                g.setColour (toJuce (appearance.colour (ColourToken::surfaceInteractive)));
                g.fillRoundedRectangle (
                    getLocalBounds().toFloat(),
                    static_cast<float> (appearance.scaled (metrics::radiusMedium)));
            }

            const auto frame =
                juce::Rectangle<float> { static_cast<float> (appearance.scaled (18)),
                                         static_cast<float> (appearance.scaled (13)) }
                    .withCentre (getLocalBounds().toFloat().getCentre());

            auto dock = frame;
            if (side == DockSide::left)
                dock = dock.removeFromLeft (frame.getWidth() * 0.38F);
            else if (side == DockSide::right)
                dock = dock.removeFromRight (frame.getWidth() * 0.38F);
            else
                dock = dock.removeFromBottom (frame.getHeight() * 0.45F);

            if (getToggleState())
            {
                g.setColour (toJuce (appearance.colour (ColourToken::accentStrong)));
                g.fillRect (dock);
            }

            g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
            g.drawRoundedRectangle (frame, 2.0F, 1.2F);
            if (side == DockSide::left)
                g.drawLine (dock.getRight(), frame.getY(), dock.getRight(), frame.getBottom());
            else if (side == DockSide::right)
                g.drawLine (dock.getX(), frame.getY(), dock.getX(), frame.getBottom());
            else
                g.drawLine (frame.getX(), dock.getY(), frame.getRight(), dock.getY());
        }

    private:
        Appearance& appearance;
        DockSide side;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DockToggleButton)
    };

    /** The tab-bar control that grows the bottom panel to the whole of the
        arrangement's room and back: a chevron pointing where the panel's top
        edge would go next.
    */
    class MaximiseButton final : public juce::Button
    {
    public:
        MaximiseButton (Appearance& lookAndScale, const ViewState& projectView)
            : juce::Button ("Maximize panel"), appearance (lookAndScale), view (projectView)
        {
        }

        ~MaximiseButton() override = default;

        void paintButton (juce::Graphics& g,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
        {
            if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
            {
                g.setColour (toJuce (appearance.colour (ColourToken::surfaceInteractive)));
                g.fillRoundedRectangle (
                    getLocalBounds().toFloat(),
                    static_cast<float> (appearance.scaled (metrics::radiusMedium)));
            }

            const auto centre = getLocalBounds().toFloat().getCentre();
            const auto reach = static_cast<float> (appearance.scaled (5));
            const auto up = ! view.bottomMaximized();
            const auto tipY = centre.y + (up ? -reach * 0.5F : reach * 0.5F);
            const auto baseY = centre.y + (up ? reach * 0.5F : -reach * 0.5F);

            juce::Path chevron;
            chevron.startNewSubPath (centre.x - reach, baseY);
            chevron.lineTo (centre.x, tipY);
            chevron.lineTo (centre.x + reach, baseY);

            g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
            g.strokePath (chevron,
                          juce::PathStrokeType {
                              1.6F, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded });
        }

    private:
        Appearance& appearance;
        const ViewState& view;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MaximiseButton)
    };

    /** Which transport act a strip button stands for. */
    enum class TransportGlyph : std::uint8_t
    {
        goToStart,
        playPause,
        record
    };

    /** A transport button drawn as its glyph: the return-to-start bar, the
        play triangle that becomes a pause while the transport rolls, and the
        record circle that fills red while a take does. The state is read from
        the transport model at paint time, so the glyph is never a step behind
        a spacebar press or a stop the engine made on its own.
    */
    class TransportButton final : public juce::Button
    {
    public:
        TransportButton (Appearance& lookAndScale,
                         const TransportBar& transportModel,
                         TransportGlyph act,
                         const juce::String& name)
            : juce::Button (name), appearance (lookAndScale), model (transportModel), glyph (act)
        {
        }

        ~TransportButton() override = default;

        void paintButton (juce::Graphics& g,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
        {
            if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
            {
                g.setColour (toJuce (appearance.colour (ColourToken::surfaceInteractive)));
                g.fillRoundedRectangle (
                    getLocalBounds().toFloat(),
                    static_cast<float> (appearance.scaled (metrics::radiusMedium)));
            }

            const auto centre = getLocalBounds().toFloat().getCentre();
            const auto reach = static_cast<float> (appearance.scaled (5));

            if (glyph == TransportGlyph::goToStart)
            {
                g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
                g.fillRect (centre.x - reach, centre.y - reach, 1.8F, reach * 2.0F);
                juce::Path triangle;
                triangle.addTriangle (centre.x - reach + 2.0F,
                                      centre.y,
                                      centre.x + reach,
                                      centre.y - reach,
                                      centre.x + reach,
                                      centre.y + reach);
                g.fillPath (triangle);
            }
            else if (glyph == TransportGlyph::playPause)
            {
                if (model.isPlaying())
                {
                    g.setColour (toJuce (appearance.colour (ColourToken::accentStrong)));
                    const auto barWidth = reach * 0.7F;
                    g.fillRect (centre.x - reach, centre.y - reach, barWidth, reach * 2.0F);
                    g.fillRect (
                        centre.x + reach - barWidth, centre.y - reach, barWidth, reach * 2.0F);
                }
                else
                {
                    g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
                    juce::Path triangle;
                    triangle.addTriangle (centre.x - reach * 0.7F,
                                          centre.y - reach,
                                          centre.x - reach * 0.7F,
                                          centre.y + reach,
                                          centre.x + reach,
                                          centre.y);
                    g.fillPath (triangle);
                }
            }
            else
            {
                const juce::Rectangle<float> circle {
                    centre.x - reach * 0.9F, centre.y - reach * 0.9F, reach * 1.8F, reach * 1.8F
                };
                if (model.isRecording())
                {
                    g.setColour (toJuce (appearance.colour (ColourToken::semanticDanger)));
                    g.fillEllipse (circle);
                }
                else
                {
                    g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
                    g.drawEllipse (circle, 1.6F);
                }
            }
        }

    private:
        Appearance& appearance;
        const TransportBar& model;
        TransportGlyph glyph;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportButton)
    };

    [[nodiscard]] int policyKeyCode (int juceKeyCode)
    {
        if (juceKeyCode == juce::KeyPress::deleteKey)
            return deleteKeyCode;
        if (juceKeyCode == juce::KeyPress::escapeKey)
            return escapeKeyCode;
        if (juceKeyCode == juce::KeyPress::F2Key)
            return f2KeyCode;
        if (juceKeyCode == juce::KeyPress::homeKey)
            return homeKeyCode;
        if (juceKeyCode == juce::KeyPress::endKey)
            return endKeyCode;
        return juceKeyCode;
    }
} // namespace

//==============================================================================
/** The live transport strip. Fixed child bounds keep changing digits from
    changing the layout; Inter supplies tabular numerals for both time labels. */
class MainShell::TransportStrip final : public juce::Component, private juce::Timer
{
public:
    TransportStrip (Appearance& lookAndScale,
                    ViewState& projectView,
                    TransportBar& transportModel,
                    std::function<void (Command)> run,
                    std::function<void()> changed)
        : appearance (lookAndScale), view (projectView), model (transportModel),
          performCommand (std::move (run)), modelChanged (std::move (changed))
    {
        setComponentID (surfaceId::transport);

        for (auto* child : std::initializer_list<juce::Component*> { &goStart,
                                                                     &play,
                                                                     &record,
                                                                     &musical,
                                                                     &wall,
                                                                     &tempo,
                                                                     &metre,
                                                                     &grid,
                                                                     &loop,
                                                                     &metronome,
                                                                     &follow,
                                                                     &cpu,
                                                                     &undo,
                                                                     &redo,
                                                                     &browserToggle,
                                                                     &panelToggle,
                                                                     &collaboratorToggle,
                                                                     &project })
            addAndMakeVisible (*child);

        musical.setJustificationType (juce::Justification::centred);
        wall.setJustificationType (juce::Justification::centred);
        cpu.setJustificationType (juce::Justification::centred);
        project.setComponentID (surfaceId::projectName);
        project.setJustification (juce::Justification::centredRight);
        tempo.setComponentID (surfaceId::tempo);
        tempo.setInputRestrictions (6, "0123456789.");
        metre.setInputRestrictions (5, "0123456789/");

        grid.addItem ("Adaptive", 1);
        grid.addItem ("1/4", 2);
        grid.addItem ("1/8", 3);
        grid.onChange = [this]
        {
            auto chosen = GridSize::adaptive;
            if (grid.getSelectedId() == 2)
                chosen = GridSize::quarter;
            else if (grid.getSelectedId() == 3)
                chosen = GridSize::eighth;
            model.setGridSize (chosen);
            modelChanged();
        };

        tempo.onReturnKey = [this] { commitTempo(); };
        tempo.onFocusLost = [this] { commitTempo(); };
        metre.onReturnKey = [this] { commitMetre(); };
        metre.onFocusLost = [this] { commitMetre(); };
        goStart.setTooltip ("Go to start (Home)");
        play.setTooltip ("Play or pause (Space)");
        record.setTooltip ("Record (R)");
        goStart.onClick = [this] { performCommand (Command::goToStart); };
        play.onClick = [this] { performCommand (Command::togglePlayback); };
        record.onClick = [this] { performCommand (Command::toggleRecording); };

        tempo.setTooltip ("Tempo (BPM)");
        metre.setTooltip ("Time signature");
        loop.setTooltip ("Loop playback");
        metronome.setTooltip ("Metronome");
        follow.setTooltip ("Follow the playhead");
        cpu.setTooltip ("Engine CPU load");
        loop.onClick = [this] { performCommand (Command::toggleLoop); };
        metronome.onClick = [this] { performCommand (Command::toggleMetronome); };
        follow.onClick = [this] { performCommand (Command::toggleFollowPlayhead); };
        undo.onClick = [this] { performCommand (Command::undo); };
        redo.onClick = [this] { performCommand (Command::redo); };

        browserToggle.setTooltip ("Show or hide the Browser (B)");
        panelToggle.setTooltip ("Show or hide the bottom panel (E)");
        collaboratorToggle.setTooltip ("Show or hide the Collaborator (C)");
        browserToggle.onClick = [this] { performCommand (Command::toggleBrowser); };
        panelToggle.onClick = [this] { performCommand (Command::toggleBottomPanel); };
        collaboratorToggle.onClick = [this] { performCommand (Command::toggleCollaborator); };

        startTimerHz (30);
        refresh();
    }

    ~TransportStrip() override = default;

    void resized() override
    {
        auto area = getLocalBounds().reduced (appearance.scaled (metrics::rowGap));
        area.removeFromLeft (appearance.scaled (menuButtonWidth + metrics::rowGap));
        const auto take = [&area, this] (int width)
        { return area.removeFromLeft (appearance.scaled (width)); };

        for (auto* button : { &goStart, &play, &record })
            button->setBounds (take (transportButtonWidth).reduced (appearance.scaled (2)));
        area.removeFromLeft (appearance.scaled (metrics::rowGap));

        musical.setBounds (take (92));
        wall.setBounds (take (112));
        tempo.setBounds (take (54).reduced (2));
        metre.setBounds (take (48).reduced (2));
        grid.setBounds (take (88).reduced (2));
        loop.setBounds (take (56));
        metronome.setBounds (take (60));
        follow.setBounds (take (64));
        cpu.setBounds (take (58));
        undo.setBounds (take (34));
        redo.setBounds (take (34));

        // The dock toggles hold the strip's right end, mirroring the docks they
        // stand for; the project name takes what is left between.
        auto toggles = area.removeFromRight (appearance.scaled (3 * dockToggleWidth));
        for (auto* toggle : { &browserToggle, &panelToggle, &collaboratorToggle })
            toggle->setBounds (toggles.removeFromLeft (appearance.scaled (dockToggleWidth))
                                   .reduced (appearance.scaled (2)));
        area.removeFromRight (appearance.scaled (metrics::rowGap));

        project.setBounds (area.reduced (2));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (toJuce (appearance.colour (ColourToken::surfaceRaised)));
        g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
        g.fillRect (getLocalBounds().removeFromBottom (1));
    }

    void refresh()
    {
        // The glyphs read the transport at paint time; what refresh adds is
        // the repaint when the state they read has turned.
        if (const auto playing = model.isPlaying(); playing != playLit)
        {
            playLit = playing;
            play.repaint();
        }
        if (const auto taking = model.isRecording(); taking != recordLit)
        {
            recordLit = taking;
            record.repaint();
        }

        musical.setText (model.musicalPosition(), juce::dontSendNotification);
        wall.setText (model.wallTime(), juce::dontSendNotification);
        if (! tempo.hasKeyboardFocus (true))
            tempo.setText (juce::String { model.tempo(), 1 }, false);
        if (! metre.hasKeyboardFocus (true))
            metre.setText (model.timeSignature(), false);
        auto gridId = 1;
        if (model.gridSize() == GridSize::quarter)
            gridId = 2;
        else if (model.gridSize() == GridSize::eighth)
            gridId = 3;
        grid.setSelectedId (gridId, juce::dontSendNotification);
        loop.setToggleState (model.isLooping(), juce::dontSendNotification);
        metronome.setToggleState (model.metronomeEnabled(), juce::dontSendNotification);
        follow.setToggleState (model.followsPlayhead(), juce::dontSendNotification);
        model.sampleCpuLoad();
        cpu.setText (juce::String { model.cpuPercent() } + "%", juce::dontSendNotification);
        cpu.setColour (juce::Label::textColourId,
                       toJuce (appearance.colour (model.cpuHealth() == CpuHealth::overloaded
                                                      ? ColourToken::semanticDanger
                                                      : ColourToken::textSecondary)));
        undo.setEnabled (model.canUndo());
        redo.setEnabled (model.canRedo());
        undo.setTooltip (model.undoLabel());
        redo.setTooltip (model.redoLabel());
        browserToggle.setToggleState (view.browserVisible(), juce::dontSendNotification);
        panelToggle.setToggleState (view.bottomVisible(), juce::dontSendNotification);
        collaboratorToggle.setToggleState (view.collaboratorVisible(), juce::dontSendNotification);
        if (! project.hasKeyboardFocus (true))
            project.setText (model.projectLabel(), false);
    }

private:
    void timerCallback() override { refresh(); }
    void commitTempo()
    {
        model.setTempo (tempo.getText().getDoubleValue());
        modelChanged();
    }
    void commitMetre()
    {
        const auto parts = juce::StringArray::fromTokens (metre.getText(), "/", {});
        if (parts.size() == 2)
        {
            model.setTimeSignature (parts[0].getIntValue(), parts[1].getIntValue());
            modelChanged();
        }
    }

    Appearance& appearance;
    ViewState& view;
    TransportBar& model;
    std::function<void (Command)> performCommand;
    std::function<void()> modelChanged;
    TransportButton goStart { appearance, model, TransportGlyph::goToStart, "Go to start" };
    TransportButton play { appearance, model, TransportGlyph::playPause, "Play or pause" };
    TransportButton record { appearance, model, TransportGlyph::record, "Record" };
    bool playLit = false;
    bool recordLit = false;
    juce::Label musical;
    juce::Label wall;
    juce::TextEditor tempo;
    juce::TextEditor metre;
    juce::ComboBox grid;
    juce::ToggleButton loop { "Loop" };
    juce::ToggleButton metronome { "Metro" };
    juce::ToggleButton follow { "Follow" };
    juce::Label cpu;
    juce::TextButton undo { "Undo" };
    juce::TextButton redo { "Redo" };
    DockToggleButton browserToggle { appearance, DockSide::left, "Browser" };
    DockToggleButton panelToggle { appearance, DockSide::bottom, "Bottom Panel" };
    DockToggleButton collaboratorToggle { appearance, DockSide::right, "Collaborator" };
    juce::TextEditor project;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportStrip)
};

//==============================================================================
/** The strip a producer drags to resize the dock beside it. */
class MainShell::Divider final : public juce::Component
{
public:
    Divider (bool draggedSideways, std::function<void (int)> onDrag)
        : sideways (draggedSideways), dragged (std::move (onDrag))
    {
        setMouseCursor (sideways ? juce::MouseCursor::LeftRightResizeCursor
                                 : juce::MouseCursor::UpDownResizeCursor);
    }

    ~Divider() override = default;

    void mouseDrag (const juce::MouseEvent& event) override
    {
        // Where the boundary is in the shell's own coordinates, which is what
        // the shell lays a dock out from — and not where it is inside this
        // strip, which moves with the strip.
        const auto position = event.getEventRelativeTo (getParentComponent()).getPosition();

        dragged (sideways ? position.x : position.y);
    }

private:
    bool sideways;
    std::function<void (int)> dragged;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Divider)
};

//==============================================================================
/** The bottom panel: a tab bar, and the one surface it has in front.

    Only the surface in front is laid out. What is behind it costs nothing and
    is asked for nothing, which is what makes `showing` the whole of the panel's
    state — and why a tab switch has to lay the panel out itself.
*/
class MainShell::BottomPanel final : public juce::Component
{
public:
    BottomPanel (Appearance& lookAndScale,
                 ViewState& projectView,
                 PianoRoll& pianoRollModel,
                 Mixer& mixerModel,
                 std::function<void (duet::model::PluginRef)> openPluginEditor,
                 std::function<void()> mixerChanged,
                 std::function<void (BottomTab)> onTabClicked,
                 std::function<void()> onMaximiseToggled)
        : appearance (lookAndScale), view (projectView), tabClicked (std::move (onTabClicked)),
          maximise (appearance, view),
          pianoRoll (std::make_unique<PianoRollCanvas> (appearance, pianoRollModel)),
          mixer (std::make_unique<MixerCanvas> (appearance,
                                                mixerModel,
                                                std::move (openPluginEditor),
                                                std::move (mixerChanged)))
    {
        setComponentID (surfaceId::bottomPanel);

        addAndMakeVisible (*pianoRoll);
        addChildComponent (*mixer);
        addAndMakeVisible (maximise);
        maximise.onClick = std::move (onMaximiseToggled);
    }

    /** The dock a device dropped into a strip's insert chain comes out of. */
    void setBrowser (Browser* dock) { mixer->setBrowser (dock); }

    ~BottomPanel() override = default;

    /** Puts the surface the view says is in front in front, and lays it out.

        Called from `resized()`, and called again by hand every time the tab
        changes: a `setBounds` with the bounds the panel already has skips
        `resized()` altogether (prototype finding, r4m858), and switching a tab
        moves nothing.
    */
    void layOutSurfaces()
    {
        const auto bar = getLocalBounds().removeFromTop (appearance.scaled (tabBarHeight));
        maximise.setBounds (
            bar.withLeft (bar.getRight() - bar.getHeight()).reduced (appearance.scaled (2)));
        maximise.setTooltip (view.bottomMaximized() ? "Restore the panel (Shift+E)"
                                                    : "Maximize the panel (Shift+E)");
        maximise.repaint();

        const auto content = getLocalBounds().withTrimmedTop (appearance.scaled (tabBarHeight));
        const auto mixerInFront = view.bottomTab() == BottomTab::mixer;

        auto& front = mixerInFront ? static_cast<juce::Component&> (*mixer)
                                   : static_cast<juce::Component&> (*pianoRoll);
        auto& behind = mixerInFront ? static_cast<juce::Component&> (*pianoRoll)
                                    : static_cast<juce::Component&> (*mixer);

        behind.setVisible (false);
        front.setBounds (content);
        front.setVisible (true);
    }

    void resized() override { layOutSurfaces(); }

    void paint (juce::Graphics& g) override
    {
        const auto bar = getLocalBounds().removeFromTop (appearance.scaled (tabBarHeight));

        g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
        g.fillRect (bar);
        g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
        g.fillRect (bar.withHeight (1));

        paintTab (g, tabBoundsFor (BottomTab::pianoRoll), "Piano Roll", BottomTab::pianoRoll);
        paintTab (g, tabBoundsFor (BottomTab::mixer), "Mixer", BottomTab::mixer);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        for (const auto tab : { BottomTab::pianoRoll, BottomTab::mixer })
            if (tabBoundsFor (tab).contains (event.getPosition()))
                tabClicked (tab);
    }

private:
    [[nodiscard]] juce::Rectangle<int> tabBoundsFor (BottomTab tab) const
    {
        const auto height = appearance.scaled (tabBarHeight);
        const auto first = appearance.scaled (pianoRollTabWidth);

        return tab == BottomTab::pianoRoll
                   ? juce::Rectangle<int> { 0, 0, first, height }
                   : juce::Rectangle<int> { first, 0, appearance.scaled (mixerTabWidth), height };
    }

    void paintTab (juce::Graphics& g,
                   juce::Rectangle<int> area,
                   const juce::String& text,
                   BottomTab tab) const
    {
        const auto inFront = view.bottomTab() == tab;

        if (inFront)
        {
            g.setColour (toJuce (appearance.colour (ColourToken::surfaceDefault)));
            g.fillRect (area);
            g.setColour (toJuce (appearance.colour (ColourToken::accentStrong)));
            g.fillRect (area.withHeight (appearance.scaled (accentBarThickness)));
        }

        g.setColour (toJuce (
            appearance.colour (inFront ? ColourToken::textPrimary : ColourToken::textMuted)));
        g.setFont (interFont (appearance.scaled (typography::body), inFront));
        g.drawText (text, area, juce::Justification::centred);
    }

    Appearance& appearance;
    ViewState& view;
    std::function<void (BottomTab)> tabClicked;
    MaximiseButton maximise;

    std::unique_ptr<PianoRollCanvas> pianoRoll;
    std::unique_ptr<MixerCanvas> mixer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BottomPanel)
};

//==============================================================================
MainShell::MainShell (Appearance& lookAndScale, ViewState& projectView, Settings& store)
    : appearance (lookAndScale), view (projectView), browserModel (store)
{
    shortcuts.add (panelShortcuts());
    shortcuts.add (timelineShortcuts());
    shortcuts.add (arrangementShortcuts());
    shortcuts.add (transportShortcuts());

    transportStrip = std::make_unique<TransportStrip> (
        appearance,
        view,
        transport,
        [this] (Command command) { perform (command); },
        [this] { viewStateChanged(); });
    arrangement = std::make_unique<ArrangementCanvas> (
        appearance,
        arrangementView,
        [this] (duet::model::ClipRef clip)
        {
            pianoRoll.openClip (clip);
            perform (Command::showPianoRoll);
        },
        [this] { askCollaborator(); });
    browserDock = std::make_unique<BrowserCanvas> (appearance, browserModel);
    browserDock->setComponentID (surfaceId::browser);

    // The shell is the container a drag out of the dock is carried across, and
    // the arrangement is one of the two surfaces it can land on. The other is
    // the mixer, which the bottom panel holds.
    arrangement->setBrowser (&browserModel);

    // A Suggestion is shown as ghosts on the surfaces it would change, and read
    // from one place by all of them.
    arrangementView.setSuggestions (&suggestions);
    mixer.setSuggestions (&suggestions);

    collaboratorDock = std::make_unique<CollaboratorPanelCanvas> (appearance, collaboratorPanel);
    collaboratorDock->setComponentID (surfaceId::collaborator);
    collaboratorDock->setSelectionContextSource ([this] { return currentSelectionContext(); });
    collaboratorDock->setSuggestions (&suggestions);
    bottom = std::make_unique<BottomPanel> (
        appearance,
        view,
        pianoRoll,
        mixer,
        [this] (duet::model::PluginRef plugin)
        {
            if (pluginEditorAction)
                pluginEditorAction (plugin);
        },
        [this] { viewStateChanged(); },
        [this] (BottomTab tab)
        { perform (tab == BottomTab::mixer ? Command::showMixer : Command::showPianoRoll); },
        [this] { perform (Command::toggleBottomMaximized); });
    bottom->setBrowser (&browserModel);

    browserDivider = std::make_unique<Divider> (true, [this] (int x) { dragBrowserDivider (x); });
    collaboratorDivider =
        std::make_unique<Divider> (true, [this] (int x) { dragCollaboratorDivider (x); });
    bottomDivider = std::make_unique<Divider> (false, [this] (int y) { dragBottomDivider (y); });

    for (auto* child : std::initializer_list<juce::Component*> { transportStrip.get(),
                                                                 arrangement.get(),
                                                                 browserDock.get(),
                                                                 collaboratorDock.get(),
                                                                 bottom.get(),
                                                                 browserDivider.get(),
                                                                 collaboratorDivider.get(),
                                                                 bottomDivider.get() })
        addAndMakeVisible (*child);

    duetButton.onClick = [this] { showDuetMenu(); };
    duetButton.setTooltip ("Duet menu");
    refreshDuetButton();
    transportStrip->addAndMakeVisible (duetButton);

    engineNotice.setJustificationType (juce::Justification::centred);
    engineNotice.setInterceptsMouseClicks (false, false);
    addChildComponent (engineNotice);

    setWantsKeyboardFocus (true);
    appearance.addListener (this);
}

MainShell::~MainShell() { appearance.removeListener (this); }

//==============================================================================
void MainShell::paint (juce::Graphics& g)
{
    g.fillAll (toJuce (appearance.colour (ColourToken::surfaceCanvas)));
}

void MainShell::resized()
{
    auto area = getLocalBounds();
    const auto divider = appearance.scaled (dividerThickness);

    auto strip = area.removeFromTop (appearance.scaled (transportStripHeight));
    transportStrip->setBounds (strip);
    duetButton.setBounds (strip.withPosition (0, 0)
                              .reduced (appearance.scaled (metrics::rowGap))
                              .withWidth (appearance.scaled (menuButtonWidth)));

    // The docks are taken off the whole height first, and the bottom panel off
    // what is left, so the panel always sits between them — never across the
    // Browser's or the Collaborator's column — and the arrangement is whatever
    // remains. Maximized, the panel takes the whole of the arrangement's room
    // instead of its docked slice, still stopping at whichever docks are open
    // and reaching the window's edge where one is closed.
    const auto maximized = view.bottomVisible() && view.bottomMaximized();

    if (view.browserVisible())
    {
        browserDock->setBounds (area.removeFromLeft (view.browserWidthPx()));
        browserDivider->setBounds (juce::Rectangle<int> { browserDock->getRight() - divider / 2,
                                                          browserDock->getY(),
                                                          divider,
                                                          browserDock->getHeight() });
    }

    browserDock->setVisible (view.browserVisible());
    browserDivider->setVisible (view.browserVisible());

    if (view.collaboratorVisible())
    {
        collaboratorDock->setBounds (area.removeFromRight (view.collaboratorWidthPx()));
        collaboratorDivider->setBounds (
            juce::Rectangle<int> { collaboratorDock->getX() - divider / 2,
                                   collaboratorDock->getY(),
                                   divider,
                                   collaboratorDock->getHeight() });
    }

    collaboratorDock->setVisible (view.collaboratorVisible());
    collaboratorDivider->setVisible (view.collaboratorVisible());

    if (maximized)
        bottom->setBounds (area);
    else if (view.bottomVisible())
    {
        bottom->setBounds (area.removeFromBottom (view.bottomHeightPx()));
        bottomDivider->setBounds (juce::Rectangle<int> {
            area.getX(), bottom->getY() - divider / 2, area.getWidth(), divider });
    }

    bottom->setVisible (view.bottomVisible());
    bottomDivider->setVisible (view.bottomVisible() && ! maximized);

    arrangement->setVisible (! maximized);
    arrangement->setBounds (maximized ? juce::Rectangle<int> {} : area);
}

//==============================================================================
void MainShell::perform (Command command)
{
    switch (command)
    {
        case Command::toggleBrowser:
            view.setBrowserVisible (! view.browserVisible());
            break;

        case Command::toggleCollaborator:
            view.setCollaboratorVisible (! view.collaboratorVisible());
            break;

        case Command::toggleBottomPanel:
            view.setBottomVisible (! view.bottomVisible());
            break;

        case Command::toggleBottomMaximized:
            // Maximizing a closed panel opens it maximized: the gesture always
            // ends with the panel on screen.
            view.setBottomMaximized (! (view.bottomVisible() && view.bottomMaximized()));
            view.setBottomVisible (true);
            break;

        case Command::zoomIn:
        case Command::zoomOut:
        case Command::zoomToFit:
        case Command::cut:
        case Command::copy:
        case Command::paste:
        case Command::duplicate:
        case Command::rename:
            arrangement->perform (command);
            break;

        case Command::selectAll:
            if (view.bottomVisible() && view.bottomTab() == BottomTab::pianoRoll
                && pianoRoll.isOpen())
                pianoRoll.selectAll();
            else
                arrangement->perform (command);
            break;

        case Command::deleteSelection:
            if (view.bottomVisible() && view.bottomTab() == BottomTab::pianoRoll
                && pianoRoll.isOpen())
                pianoRoll.deleteSelected();
            else
                arrangement->perform (command);
            break;

        case Command::cancel:
            if (view.bottomVisible() && view.bottomTab() == BottomTab::pianoRoll
                && pianoRoll.isOpen())
            {
                pianoRoll.cancelNoteGesture();
                pianoRoll.clearSelection();
            }
            else
                arrangement->perform (command);
            break;

        case Command::togglePlayback:
            transport.togglePlayback();
            break;
        case Command::toggleRecording:
            transport.toggleRecording();
            break;
        case Command::toggleLoop:
            transport.toggleLoop();
            break;
        case Command::toggleMetronome:
            transport.toggleMetronome();
            break;
        case Command::toggleFollowPlayhead:
            transport.toggleFollowPlayhead();
            break;
        case Command::goToStart:
            transport.goToStart();
            break;
        case Command::goToEnd:
            transport.goToEnd();
            break;
        case Command::undo:
            static_cast<void> (transport.undo());
            break;
        case Command::redo:
            static_cast<void> (transport.redo());
            break;
        case Command::save:
            if (saveAction)
                saveAction();
            break;

        case Command::showPianoRoll:
        case Command::showMixer:
            // A tab behind a closed panel is not a tab the producer can see, so
            // choosing one opens the panel it is in.
            if (command == Command::showPianoRoll)
                if (const auto selected = arrangementView.selectedMidiClip();
                    selected != duet::model::noClip)
                    pianoRoll.openClip (selected);
            view.setBottomTab (command == Command::showMixer ? BottomTab::mixer
                                                             : BottomTab::pianoRoll);
            view.setBottomVisible (true);
            break;
    }

    viewStateChanged();
}

void MainShell::setTimelineClock (TimelineClock* projectClock)
{
    arrangementView.setClock (projectClock);
    pianoRoll.setClock (projectClock);
    viewStateChanged();
}

void MainShell::setSession (duet::model::Session* openProject)
{
    arrangementView.setSession (openProject);
    pianoRoll.setSession (openProject);
    mixer.setSession (openProject);
    transport.setSession (openProject);
    browserModel.setSession (openProject);

    // The engine's notices come to the shell's own chrome. The default surface
    // is a bubble pinned to whatever sits under the mouse — and one more per
    // ask while the transport keeps asking.
    if (openProject != nullptr)
        openProject->onEngineMessage (
            [safe = juce::Component::SafePointer { this }] (const std::string& message)
            {
                if (safe != nullptr)
                    safe->showEngineNotice (message);
            });

    // Whatever was pending goes with the project it was made against, so the
    // surfaces are told what there is to draw before they are laid out.
    suggestions.refresh();

    viewStateChanged();
}

void MainShell::setCollaboratorSetupAction (std::function<void()> openSettings)
{
    collaboratorDock->setSetupAction (std::move (openSettings));
}

void MainShell::askCollaborator()
{
    // The answer arrives in the panel, so an ask from a menu opens it rather
    // than leaving the producer to; what to ask is theirs to write, so the
    // keyboard goes to the composer and nothing is sent.
    view.setCollaboratorVisible (true);
    collaboratorPanel.focusComposer();
    viewStateChanged();
    collaboratorDock->refresh();
}

SelectionContext MainShell::currentSelectionContext() const
{
    const auto asked = arrangementView.askContext();

    switch (asked.scope)
    {
        case AskScope::clips:
            return asked.clips.size() == 1 ? clipSelected (asked.name)
                                           : clipsSelected (static_cast<int> (asked.clips.size()));

        case AskScope::track:
            return trackSelected (asked.name);

        case AskScope::nothing:
            break;
    }

    return noSelection();
}

AskContext MainShell::askContext() const { return arrangementView.askContext(); }

void MainShell::setPluginEditorAction (std::function<void (duet::model::PluginRef)> openEditor)
{
    pluginEditorAction = std::move (openEditor);
}

void MainShell::setProjectStatus (std::string name, bool dirty)
{
    transport.setProjectStatus (std::move (name), dirty);
}

void MainShell::setSaveAction (std::function<void()> save) { saveAction = std::move (save); }

void MainShell::viewStateChanged()
{
    // A project the window has just opened brings its own tempo and metre with
    // it, and the grid counts in those.
    arrangementView.refresh();
    resized();

    // The panel's own bounds are unchanged by a tab switch, and a same-bounds
    // setBounds skips resized() (prototype finding, r4m858), so the surface
    // coming to the front is laid out here or nowhere.
    bottom->layOutSurfaces();
    repaint();
}

//==============================================================================
void MainShell::dragBrowserDivider (int x)
{
    view.setBrowserWidthPx (x);
    resized();
}

void MainShell::dragCollaboratorDivider (int x)
{
    view.setCollaboratorWidthPx (getWidth() - x);
    resized();
}

void MainShell::dragBottomDivider (int y)
{
    view.setBottomHeightPx (getHeight() - y);
    resized();
}

//==============================================================================
bool MainShell::keyPressed (const juce::KeyPress& key)
{
    const KeyStroke stroke { policyKeyCode (key.getKeyCode()),
                             key.getModifiers().isCtrlDown(),
                             key.getModifiers().isAltDown(),
                             key.getModifiers().isShiftDown() };

    const auto command = shortcuts.commandFor (stroke, aTextFieldHasFocus());

    if (! command.has_value())
        return false;

    perform (*command);
    return true;
}

//==============================================================================
juce::PopupMenu MainShell::duetMenu() const
{
    juce::PopupMenu menu;

    addToggle (menu, Command::toggleBrowser, "Browser", "B", view.browserVisible());
    addToggle (menu, Command::toggleCollaborator, "Collaborator", "C", view.collaboratorVisible());
    addToggle (menu, Command::toggleBottomPanel, "Bottom Panel", "E", view.bottomVisible());
    addToggle (menu,
               Command::toggleBottomMaximized,
               "Maximize Bottom Panel",
               "Shift+E",
               view.bottomVisible() && view.bottomMaximized());
    menu.addSeparator();
    addToggle (menu,
               Command::showPianoRoll,
               "Piano Roll",
               "P",
               view.bottomVisible() && view.bottomTab() == BottomTab::pianoRoll);
    addToggle (menu,
               Command::showMixer,
               "Mixer",
               "X",
               view.bottomVisible() && view.bottomTab() == BottomTab::mixer);

    if (buildHostMenu)
    {
        menu.addSeparator();
        buildHostMenu (menu);
    }

    return menu;
}

void MainShell::menuItemChosen (int itemId)
{
    if (itemId <= 0)
        return;

    if (const auto command = commandForMenuId (itemId); command.has_value())
    {
        perform (*command);
        return;
    }

    if (hostMenuItemChosen)
        hostMenuItemChosen (itemId);
}

void MainShell::setHostMenu (std::function<void (juce::PopupMenu&)> build,
                             std::function<void (int)> chosen)
{
    buildHostMenu = std::move (build);
    hostMenuItemChosen = std::move (chosen);
}

void MainShell::refreshDuetButton()
{
    // The mark's charcoal D takes the theme's ink so it reads on both palettes;
    // the teal triangle is the brand's own and stays.
    const auto mark = brandMark (toJuce (appearance.colour (ColourToken::textPrimary)));

    duetButton.setImages (mark.get());
}

void MainShell::showEngineNotice (const std::string& message)
{
    const auto font = interFont (appearance.scaled (typography::body));
    const juce::String text { message };

    engineNotice.setFont (font);
    engineNotice.setText (text, juce::dontSendNotification);
    engineNotice.setColour (juce::Label::backgroundColourId,
                            toJuce (appearance.colour (ColourToken::surfaceRaised)));
    engineNotice.setColour (juce::Label::textColourId,
                            toJuce (appearance.colour (ColourToken::textPrimary)));
    engineNotice.setColour (juce::Label::outlineColourId,
                            toJuce (appearance.colour (ColourToken::borderDefault)));

    const auto width =
        std::min (getWidth() - appearance.scaled (2 * metrics::rowGap),
                  static_cast<int> (juce::GlyphArrangement::getStringWidth (font, text))
                      + appearance.scaled (6 * metrics::rowGap));
    const auto height = appearance.scaled (32);

    engineNotice.setBounds ((getWidth() - width) / 2,
                            getHeight() - height - appearance.scaled (2 * metrics::rowGap),
                            width,
                            height);
    engineNotice.setVisible (true);
    engineNotice.toFront (false);

    // Replaced notices hand their clock to the newest one, so a burst of
    // notices reads as one line that stays up, not a flicker of takedowns.
    const auto generation = ++engineNoticeGeneration;

    juce::Timer::callAfterDelay (4000,
                                 [safe = juce::Component::SafePointer { this }, generation]
                                 {
                                     if (safe != nullptr
                                         && safe->engineNoticeGeneration == generation)
                                         safe->engineNotice.setVisible (false);
                                 });
}

void MainShell::showDuetMenu()
{
    duetMenu().showMenuAsync (
        juce::PopupMenu::Options {}.withTargetComponent (duetButton),
        [safeThis = juce::Component::SafePointer<MainShell> { this }] (int chosen)
        {
            if (safeThis != nullptr)
                safeThis->menuItemChosen (chosen);
        });
}

//==============================================================================
void MainShell::setHardwareAccelerated (bool shouldBeAccelerated)
{
    if (accelerated == shouldBeAccelerated)
        return;

    accelerated = shouldBeAccelerated;

    if (hardwareContext == nullptr)
        hardwareContext = std::make_unique<AcceleratedSurface>();

    hardwareContext->attachTo (accelerated ? this : nullptr);
}

//==============================================================================
void MainShell::appearanceChanged()
{
    // A theme is a repaint, but the scale is a layout: every measurement of the
    // shell's own chrome is in logical units.
    refreshDuetButton();
    sendLookAndFeelChange();
    viewStateChanged();
}
} // namespace duet::gui
