#include <duet/gui/MainShell.h>

#include <duet/gui/AcceleratedSurface.h>
#include <duet/gui/ArrangementCanvas.h>
#include <duet/gui/CollaboratorPanelCanvas.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/MixerCanvas.h>
#include <duet/gui/PianoRollCanvas.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <array>
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
    constexpr int menuButtonWidth = 64;
    constexpr int tabBarHeight = 28;
    constexpr int pianoRollTabWidth = 104;
    constexpr int mixerTabWidth = 78;
    constexpr int accentBarThickness = 2;

    /** The menu id an entry of the shell's own has. Zero is JUCE's "nothing
        chosen", and the host's ids start above these.
    */
    [[nodiscard]] int idFor (Command command) { return static_cast<int> (command) + 1; }

    [[nodiscard]] std::optional<Command> commandForMenuId (int itemId)
    {
        for (const auto command : { Command::toggleBrowser,
                                    Command::toggleCollaborator,
                                    Command::toggleBottomPanel,
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
/** One of the interface's areas, empty until its own slice fills it in.

    It is a real surface from this slice on — it has its place in the window, its
    name, and the Graphite ground and hairline every panel is drawn on — and it
    carries its title so that a window of empty docks can still be read.
*/
class MainShell::Dock final : public juce::Component
{
public:
    Dock (Appearance& lookAndScale, const char* areaId, juce::String areaTitle)
        : appearance (lookAndScale), title (std::move (areaTitle))
    {
        setComponentID (areaId);
    }

    ~Dock() override = default;

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds();

        g.setColour (toJuce (appearance.colour (ColourToken::surfaceDefault)));
        g.fillRect (area);

        g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
        g.drawRect (area, 1);

        g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
        g.setFont (interFont (appearance.scaled (typography::eyebrow)));
        g.drawText (title,
                    area.reduced (appearance.scaled (metrics::panelPadding)),
                    juce::Justification::topLeft);
    }

private:
    Appearance& appearance;
    juce::String title;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dock)
};

//==============================================================================
/** The live transport strip. Fixed child bounds keep changing digits from
    changing the layout; Inter supplies tabular numerals for both time labels. */
class MainShell::TransportStrip final : public juce::Component, private juce::Timer
{
public:
    TransportStrip (Appearance& lookAndScale,
                    TransportBar& transportModel,
                    std::function<void (Command)> run,
                    std::function<void()> changed)
        : appearance (lookAndScale), model (transportModel), performCommand (std::move (run)),
          modelChanged (std::move (changed))
    {
        setComponentID (surfaceId::transport);

        for (auto* child : std::initializer_list<juce::Component*> { &musical,
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
        loop.onClick = [this] { performCommand (Command::toggleLoop); };
        metronome.onClick = [this] { performCommand (Command::toggleMetronome); };
        follow.onClick = [this] { performCommand (Command::toggleFollowPlayhead); };
        undo.onClick = [this] { performCommand (Command::undo); };
        redo.onClick = [this] { performCommand (Command::redo); };

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

        musical.setBounds (take (92));
        wall.setBounds (take (112));
        tempo.setBounds (take (54).reduced (2));
        metre.setBounds (take (48).reduced (2));
        grid.setBounds (take (88).reduced (2));
        loop.setBounds (take (42));
        metronome.setBounds (take (42));
        follow.setBounds (take (42));
        cpu.setBounds (take (58));
        undo.setBounds (take (34));
        redo.setBounds (take (34));
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
    TransportBar& model;
    std::function<void (Command)> performCommand;
    std::function<void()> modelChanged;
    juce::Label musical;
    juce::Label wall;
    juce::TextEditor tempo;
    juce::TextEditor metre;
    juce::ComboBox grid;
    juce::ToggleButton loop { "L" };
    juce::ToggleButton metronome { "M" };
    juce::ToggleButton follow { "F" };
    juce::Label cpu;
    juce::TextButton undo { "Undo" };
    juce::TextButton redo { "Redo" };
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
                 std::function<void (BottomTab)> onTabClicked)
        : appearance (lookAndScale), view (projectView), tabClicked (std::move (onTabClicked)),
          pianoRoll (std::make_unique<PianoRollCanvas> (appearance, pianoRollModel)),
          mixer (std::make_unique<MixerCanvas> (appearance,
                                                mixerModel,
                                                std::move (openPluginEditor),
                                                std::move (mixerChanged)))
    {
        setComponentID (surfaceId::bottomPanel);

        addAndMakeVisible (*pianoRoll);
        addChildComponent (*mixer);
    }

    ~BottomPanel() override = default;

    /** Puts the surface the view says is in front in front, and lays it out.

        Called from `resized()`, and called again by hand every time the tab
        changes: a `setBounds` with the bounds the panel already has skips
        `resized()` altogether (prototype finding, r4m858), and switching a tab
        moves nothing.
    */
    void layOutSurfaces()
    {
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

    std::unique_ptr<PianoRollCanvas> pianoRoll;
    std::unique_ptr<MixerCanvas> mixer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BottomPanel)
};

//==============================================================================
MainShell::MainShell (Appearance& lookAndScale, ViewState& projectView)
    : appearance (lookAndScale), view (projectView)
{
    shortcuts.add (panelShortcuts());
    shortcuts.add (timelineShortcuts());
    shortcuts.add (arrangementShortcuts());
    shortcuts.add (transportShortcuts());

    transportStrip = std::make_unique<TransportStrip> (
        appearance,
        transport,
        [this] (Command command) { perform (command); },
        [this] { viewStateChanged(); });
    arrangement = std::make_unique<ArrangementCanvas> (appearance,
                                                       arrangementView,
                                                       [this] (duet::model::ClipRef clip)
                                                       {
                                                           pianoRoll.openClip (clip);
                                                           perform (Command::showPianoRoll);
                                                       });
    browser = std::make_unique<Dock> (appearance, surfaceId::browser, "Browser");

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
        { perform (tab == BottomTab::mixer ? Command::showMixer : Command::showPianoRoll); });

    browserDivider = std::make_unique<Divider> (true, [this] (int x) { dragBrowserDivider (x); });
    collaboratorDivider =
        std::make_unique<Divider> (true, [this] (int x) { dragCollaboratorDivider (x); });
    bottomDivider = std::make_unique<Divider> (false, [this] (int y) { dragBottomDivider (y); });

    for (auto* child : std::initializer_list<juce::Component*> { transportStrip.get(),
                                                                 arrangement.get(),
                                                                 browser.get(),
                                                                 collaboratorDock.get(),
                                                                 bottom.get(),
                                                                 browserDivider.get(),
                                                                 collaboratorDivider.get(),
                                                                 bottomDivider.get() })
        addAndMakeVisible (*child);

    duetButton.onClick = [this] { showDuetMenu(); };
    transportStrip->addAndMakeVisible (duetButton);

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

    // The bottom panel is taken off the whole width, and the docks off what is
    // left, so that a dock stops where the panel starts and the arrangement is
    // whatever remains — which is what makes collapsing one give its room to the
    // arrangement rather than to another dock.
    if (view.bottomVisible())
    {
        bottom->setBounds (area.removeFromBottom (view.bottomHeightPx()));
        bottomDivider->setBounds (
            juce::Rectangle<int> { 0, bottom->getY() - divider / 2, getWidth(), divider });
    }

    bottom->setVisible (view.bottomVisible());
    bottomDivider->setVisible (view.bottomVisible());

    if (view.browserVisible())
    {
        browser->setBounds (area.removeFromLeft (view.browserWidthPx()));
        browserDivider->setBounds (juce::Rectangle<int> {
            browser->getRight() - divider / 2, browser->getY(), divider, browser->getHeight() });
    }

    browser->setVisible (view.browserVisible());
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

    arrangement->setBounds (area);
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
    session = openProject;
    arrangementView.setSession (openProject);
    pianoRoll.setSession (openProject);
    mixer.setSession (openProject);
    transport.setSession (openProject);

    // Whatever was pending goes with the project it was made against, so the
    // surfaces are told what there is to draw before they are laid out.
    suggestions.refresh();

    viewStateChanged();
}

SelectionContext MainShell::currentSelectionContext() const
{
    const auto& selected = arrangementView.selection();

    if (! selected.empty() && selected.focusedKind() == SelectionKind::clip)
        return clipsSelected (static_cast<int> (selected.items().size()));

    if (session != nullptr)
        if (const auto track = session->track (arrangementView.focusedTrack());
            track.track != duet::model::noTrack)
            return trackSelected (track.name);

    return noSelection();
}

std::vector<duet::model::ClipRef> MainShell::selectedClips() const
{
    const auto& selected = arrangementView.selection();

    if (selected.empty() || selected.focusedKind() != SelectionKind::clip)
        return {};

    std::vector<duet::model::ClipRef> clips;

    for (const auto& item : selected.items())
        clips.push_back (item.ref);

    return clips;
}

duet::model::TrackRef MainShell::focusedTrack() const { return arrangementView.focusedTrack(); }

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
    sendLookAndFeelChange();
    viewStateChanged();
}
} // namespace duet::gui
