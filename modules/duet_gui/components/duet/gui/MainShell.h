#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/ArrangementView.h>
#include <duet/gui/Browser.h>
#include <duet/gui/CollaboratorPanel.h>
#include <duet/gui/Mixer.h>
#include <duet/gui/PianoRoll.h>
#include <duet/gui/Shortcuts.h>
#include <duet/gui/Suggestions.h>
#include <duet/gui/TransportBar.h>
#include <duet/gui/ViewState.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace duet::gui
{
class AcceleratedSurface;
class ArrangementCanvas;
class BrowserCanvas;
class CollaboratorPanelCanvas;
class PianoRollCanvas;
class TimelineClock;

/** The names the shell's areas carry, so that what is on screen can be named
    from outside it — by a test, and by anything that has to find one surface
    among the others.
*/
namespace surfaceId
{
    inline constexpr const char* transport = "transport";
    inline constexpr const char* projectName = "projectName";
    inline constexpr const char* tempo = "tempo";
    inline constexpr const char* arrangement = "arrangement";
    inline constexpr const char* arrangementRuler = "arrangementRuler";
    inline constexpr const char* browser = "browser";
    inline constexpr const char* collaborator = "collaborator";
    inline constexpr const char* bottomPanel = "bottomPanel";
    inline constexpr const char* pianoRoll = "pianoRoll";
    inline constexpr const char* mixer = "mixer";
} // namespace surfaceId

/** The single main window the whole interface lives in.

    A transport strip across the top, the arrangement in the centre, the browser
    docked left and the Collaborator docked right, and a resizable, collapsible
    bottom panel holding the Piano Roll and the Mixer. A draggable divider
    separates each dock from the arrangement, the strip's right end carries a
    toggle for each of the three, and the panel can be maximized into the whole
    of the arrangement's room. The Duet mark in the transport strip opens the
    app menu, which is what this interface has instead of a menu bar; plugin
    editors will be the only floating windows, and nothing tears off (spec
    535bbo).

    What the shell shows is what the view state says, and every gesture that
    changes the layout goes through the view state rather than around it — that
    is what makes the layout the project's, restored when it reopens, and what
    keeps it out of the producer's undo history.
*/
class MainShell final : public juce::Component,
                        public juce::DragAndDropContainer,
                        private Appearance::Listener
{
public:
    /** @param lookAndScale  the palette and the interface scale the shell is
                             drawn and measured in
        @param projectView   the layout of the open project. The shell writes the
                             producer's gestures into it and lays itself out from
                             it; the persistence facade is what saves it.
        @param store         the app-global store the browser's sample folders
                             and favourites live in, they being the producer's
                             rather than the project's
    */
    MainShell (Appearance& lookAndScale, ViewState& projectView, Settings& store);

    ~MainShell() override;

    //==============================================================================
    /** Does what a panel key or a menu entry means. */
    void perform (Command command);

    /** Lays the shell out on a view it has not seen before — what opening a
        project does.
    */
    void viewStateChanged();

    /** The clock of the project open in the window, or nothing when none is:
        what the timeline is drawn against and where the playhead is. The host
        hands it over as it opens a project, and takes it back before the
        project goes.
    */
    void setTimelineClock (TimelineClock* projectClock);
    void setSession (duet::model::Session* openProject);

    /** What the Collaborator panel's context chip records, in the engine-free
        shape the panel reads: what the next message is about — the producer's
        own selection, or the clip or track they asked about from its context
        menu.
    */
    [[nodiscard]] SelectionContext currentSelectionContext() const;

    /** The Collaborator panel's own state, so that the host can give it what
        answers the producer.

        The shell knows about panels and the host knows about the Collaborator
        service, which is why the source arrives from outside rather than being
        made here.
    */
    [[nodiscard]] CollaboratorPanel& collaborator() { return collaboratorPanel; }

    /** What the Collaborator panel's setup state leads to. The shell owns no
        Settings window — the host does — so the way there is handed in, like
        the selection the panel asks for.
    */
    void setCollaboratorSetupAction (std::function<void()> openSettings);

    /** The left dock's own state, so that the host can give it what only the
        host has: the project a drop edits, and the import that puts a dropped
        sample inside the project folder. The Settings window reads the same one
        to manage the sample folders, which is what makes a folder added there
        show in the dock at once.
    */
    [[nodiscard]] Browser& browser() { return browserModel; }

    /** What the next message to the Collaborator is about, as the arrangement
        answers it: the producer's own selection, or the clip or track they asked
        about from its context menu. The panel's chip is made of it and so is a
        Task Run's opening context, so both say the same thing.
    */
    [[nodiscard]] AskContext askContext() const;

    /** The pending Suggestions all three surfaces read, so that the host can
        give them what makes one.

        The same seam as the panel's: the shell knows which surfaces show a
        Suggestion, and the host knows the Suggestion manager behind them.
    */
    [[nodiscard]] Suggestions& pendingSuggestions() { return suggestions; }

    /** The component-only bridge used to open a hosted plugin editor. */
    void setPluginEditorAction (std::function<void (duet::model::PluginRef)> openEditor);

    /** Project lifecycle facts and Save remain host-owned; the bar presents and
        invokes them through this narrow seam. */
    void setProjectStatus (std::string name, bool dirty);
    void setSaveAction (std::function<void()> save);

    //==============================================================================
    /** What a divider drag means: where the producer has put the boundary, in
        pixels from the shell's own left edge or top edge. The divider bars are
        what call these, and driving them directly is how a drag is asserted.
    */
    void dragBrowserDivider (int x);
    void dragCollaboratorDivider (int x);
    void dragBottomDivider (int y);

    //==============================================================================
    /** The Duet menu, as the button opens it: the panel toggles, and whatever
        the host has added under them.
    */
    [[nodiscard]] juce::PopupMenu duetMenu() const;

    /** Runs the entry the producer chose. Zero is the menu dismissed. */
    void menuItemChosen (int itemId);

    /** The entries the host adds under the shell's own, and what to do when one
        is chosen. Project commands arrive this way (issue ce17ym), because the
        shell knows about panels and the host knows about projects.

        Ids from `firstHostMenuId` up are the host's; below it are the shell's.
    */
    void setHostMenu (std::function<void (juce::PopupMenu&)> build,
                      std::function<void (int)> chosen);

    static constexpr int firstHostMenuId = 100;

    //==============================================================================
    /** The rendering escape hatch (spec 535bbo): puts the shell's surfaces on a
        hardware-accelerated context instead of the software renderer they are
        drawn on by default. The drawing is the same drawing — only who
        rasterises it changes.
    */
    void setHardwareAccelerated (bool shouldBeAccelerated);

    [[nodiscard]] bool isHardwareAccelerated() const { return accelerated; }

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    /** The heights the shell's own chrome is drawn to, in logical units. */
    static constexpr int transportStripHeight = 44;
    static constexpr int dividerThickness = 6;

private:
    class TransportStrip;
    class BottomPanel;
    class Divider;

    void appearanceChanged() override;
    void showDuetMenu();
    void refreshDuetButton();

    /** Shows one engine notice as a transient line low in the window, replacing
        whatever notice was up: the engine's word reaches the producer without a
        dialog, and without the default bubble pinned to the mouse.
    */
    void showEngineNotice (const std::string& message);

    /** What an "Ask Collaborator" entry does once the surface it was chosen on
        has said what the ask is about: the panel opens if it was closed and the
        keyboard goes to the composer, with nothing sent (spec js437t, story 9).
    */
    void askCollaborator();

    Appearance& appearance;
    ViewState& view;

    /** One table for the whole window: the shell's panel keys, and the zoom keys
        of the surfaces that draw musical time.
    */
    Shortcuts shortcuts;

    /** The arrangement's view-model. The shell owns it because the shell owns
        the surface that paints it, and it is what later surfaces on the same
        timeline will read.
    */
    ArrangementView arrangementView { view };
    PianoRoll pianoRoll { view, arrangementView.selection() };
    Mixer mixer;
    Browser browserModel;
    TransportBar transport { view };

    /** The Collaborator panel's own state. What answers it is the host's, the
        Collaborator service living a layer up from the interface.
    */
    CollaboratorPanel collaboratorPanel;

    /** The pending Suggestions. The shell owns them because all three surfaces
        that show a Suggestion — the panel's card, the timeline's ghosts and the
        mixer's ghost handles — have to be reading the same one; what makes one
        is the host's, and arrives as the source.
    */
    Suggestions suggestions;

    juce::DrawableButton duetButton { "Duet menu", juce::DrawableButton::ImageFitted };
    juce::Label engineNotice;

    /** Which showing of the notice the hide timer belongs to, so a notice that
        replaced an earlier one is not taken down on the earlier one's clock.
    */
    int engineNoticeGeneration = 0;

    std::unique_ptr<TransportStrip> transportStrip;
    std::unique_ptr<ArrangementCanvas> arrangement;
    std::unique_ptr<BrowserCanvas> browserDock;
    std::unique_ptr<CollaboratorPanelCanvas> collaboratorDock;
    std::unique_ptr<BottomPanel> bottom;
    std::unique_ptr<Divider> browserDivider;
    std::unique_ptr<Divider> collaboratorDivider;
    std::unique_ptr<Divider> bottomDivider;
    std::unique_ptr<AcceleratedSurface> hardwareContext;

    std::function<void()> saveAction;
    std::function<void (duet::model::PluginRef)> pluginEditorAction;
    std::function<void (juce::PopupMenu&)> buildHostMenu;
    std::function<void (int)> hostMenuItemChosen;
    bool accelerated = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainShell)
};
} // namespace duet::gui
