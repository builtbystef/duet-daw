#include "GuiTestSupport.h"

#include <duet/gui/Appearance.h>
#include <duet/gui/MainShell.h>
#include <duet/gui/Shortcuts.h>
#include <duet/gui/ViewState.h>
#include <duet/model/Session.h>

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <string>
#include <vector>

using duet::gui::Appearance;
using duet::gui::BottomTab;
using duet::gui::Command;
using duet::gui::MainShell;
using duet::gui::ViewState;
using duet::testing::StoredSettings;
using duet::testing::surfaceOf;

namespace
{
/** A shell of the size the main window opens at, with everything it reads. */
struct OpenShell
{
    OpenShell() { shell.setBounds (0, 0, 1600, 980); }

    [[nodiscard]] const juce::Component& surface (const char* surfaceId) const
    {
        const auto* found = surfaceOf (shell, surfaceId);

        REQUIRE (found != nullptr);
        return *found;
    }

    juce::ScopedJuceInitialiser_GUI juce;
    StoredSettings store;
    Appearance appearance { store, true };
    ViewState view;
    MainShell shell { appearance, view };
};

/** The texts of a menu's items, in the order it offers them. */
[[nodiscard]] std::vector<juce::String> itemsOf (const juce::PopupMenu& menu)
{
    std::vector<juce::String> texts;

    for (juce::PopupMenu::MenuItemIterator item { menu }; item.next();)
        texts.push_back (item.getItem().text);

    return texts;
}

/** Whether a menu shows an item as the state it is in. */
[[nodiscard]] bool isTicked (const juce::PopupMenu& menu, const juce::String& text)
{
    for (juce::PopupMenu::MenuItemIterator item { menu }; item.next();)
        if (item.getItem().text == text)
            return item.getItem().isTicked;

    return false;
}
} // namespace

TEST_CASE ("the window shows the transport strip, the arrangement, both docks and the bottom panel")
{
    const OpenShell open;

    for (const auto* area : { duet::gui::surfaceId::transport,
                              duet::gui::surfaceId::arrangement,
                              duet::gui::surfaceId::browser,
                              duet::gui::surfaceId::collaborator,
                              duet::gui::surfaceId::bottomPanel })
    {
        const auto& surface = open.surface (area);

        REQUIRE (surface.isVisible());
        REQUIRE_FALSE (surface.getBounds().isEmpty());
    }
}

TEST_CASE ("dragging a divider resizes the dock beside it")
{
    OpenShell open;

    open.shell.dragBrowserDivider (260);

    REQUIRE (open.view.browserWidthPx() == 260);
    REQUIRE (open.surface (duet::gui::surfaceId::browser).getWidth() == 260);

    // The Collaborator's divider is dragged from the same edge the mouse is on,
    // and its dock grows the other way.
    open.shell.dragCollaboratorDivider (1600 - 320);

    REQUIRE (open.view.collaboratorWidthPx() == 320);
    REQUIRE (open.surface (duet::gui::surfaceId::collaborator).getWidth() == 320);

    open.shell.dragBottomDivider (980 - 300);

    REQUIRE (open.view.bottomHeightPx() == 300);
    REQUIRE (open.surface (duet::gui::surfaceId::bottomPanel).getHeight() == 300);
}

TEST_CASE ("a dock collapses and reopens at the size it had")
{
    OpenShell open;

    open.shell.dragBrowserDivider (260);

    const auto arrangementWithTheBrowserOpen =
        open.surface (duet::gui::surfaceId::arrangement).getWidth();

    open.shell.perform (Command::toggleBrowser);

    REQUIRE_FALSE (open.surface (duet::gui::surfaceId::browser).isVisible());
    REQUIRE (open.surface (duet::gui::surfaceId::arrangement).getWidth()
             > arrangementWithTheBrowserOpen);

    open.shell.perform (Command::toggleBrowser);

    REQUIRE (open.surface (duet::gui::surfaceId::browser).isVisible());
    REQUIRE (open.surface (duet::gui::surfaceId::browser).getWidth() == 260);
    REQUIRE (open.surface (duet::gui::surfaceId::arrangement).getWidth()
             == arrangementWithTheBrowserOpen);
}

TEST_CASE ("switching the bottom tab lays the newly shown surface out, bounds or no bounds")
{
    OpenShell open;

    const auto& pianoRoll = open.surface (duet::gui::surfaceId::pianoRoll);
    const auto& mixer = open.surface (duet::gui::surfaceId::mixer);
    const auto panelBefore = open.surface (duet::gui::surfaceId::bottomPanel).getBounds();

    REQUIRE (pianoRoll.isVisible());
    REQUIRE_FALSE (pianoRoll.getBounds().isEmpty());

    open.shell.perform (Command::showMixer);

    // Nothing about the switch moves the panel, so nothing about the switch
    // calls its resized(): the surface coming to the front is laid out because
    // the switch lays it out (prototype finding, r4m858).
    REQUIRE (open.surface (duet::gui::surfaceId::bottomPanel).getBounds() == panelBefore);
    REQUIRE (mixer.isVisible());
    REQUIRE_FALSE (pianoRoll.isVisible());
    REQUIRE (mixer.getBounds() == pianoRoll.getBounds());

    open.shell.perform (Command::showPianoRoll);

    REQUIRE (pianoRoll.isVisible());
    REQUIRE_FALSE (mixer.isVisible());
    REQUIRE_FALSE (pianoRoll.getBounds().isEmpty());
}

TEST_CASE ("selecting a bottom tab opens the panel it is in")
{
    OpenShell open;

    open.shell.perform (Command::toggleBottomPanel);
    REQUIRE_FALSE (open.view.bottomVisible());

    open.shell.perform (Command::showMixer);

    REQUIRE (open.view.bottomVisible());
    REQUIRE (open.view.bottomTab() == BottomTab::mixer);
    REQUIRE (open.surface (duet::gui::surfaceId::mixer).isVisible());
}

TEST_CASE ("the panel keys drive the shell")
{
    OpenShell open;

    REQUIRE (open.shell.keyPressed (juce::KeyPress { 'b' }));
    REQUIRE_FALSE (open.view.browserVisible());

    REQUIRE (open.shell.keyPressed (juce::KeyPress { 'c' }));
    REQUIRE_FALSE (open.view.collaboratorVisible());

    REQUIRE (open.shell.keyPressed (juce::KeyPress { 'x' }));
    REQUIRE (open.view.bottomTab() == BottomTab::mixer);

    REQUIRE (open.shell.keyPressed (juce::KeyPress { 'p' }));
    REQUIRE (open.view.bottomTab() == BottomTab::pianoRoll);

    REQUIRE (open.shell.keyPressed (juce::KeyPress { 'e' }));
    REQUIRE_FALSE (open.view.bottomVisible());

    // A key the shell has nothing registered for is left for whatever else
    // wants it.
    REQUIRE_FALSE (open.shell.keyPressed (juce::KeyPress { 'q' }));
}

TEST_CASE ("global transport keys reach the open session without entering undo")
{
    OpenShell open;
    const auto editFile =
        std::filesystem::temp_directory_path()
        / ("duet-gui-" + std::to_string (juce::Random::getSystemRandom().nextInt64())
           + ".tracktionedit");
    duet::model::Session session { editFile };
    open.shell.setSession (&session);
    const auto undoBefore = session.undoNames();

    REQUIRE (open.shell.keyPressed (juce::KeyPress { 'l' }));
    REQUIRE (session.isLooping());
    REQUIRE (open.shell.keyPressed (juce::KeyPress { 'm' }));
    REQUIRE (session.metronomeEnabled());
    REQUIRE (session.undoNames() == undoBefore);
}

TEST_CASE ("the Duet menu carries the panel toggles, and shows which are open")
{
    OpenShell open;

    const auto menu = open.shell.duetMenu();
    const auto texts = itemsOf (menu);

    REQUIRE (std::find (texts.begin(), texts.end(), "Browser") != texts.end());
    REQUIRE (std::find (texts.begin(), texts.end(), "Collaborator") != texts.end());
    REQUIRE (std::find (texts.begin(), texts.end(), "Bottom Panel") != texts.end());
    REQUIRE (std::find (texts.begin(), texts.end(), "Piano Roll") != texts.end());
    REQUIRE (std::find (texts.begin(), texts.end(), "Mixer") != texts.end());

    REQUIRE (isTicked (menu, "Browser"));

    open.shell.perform (Command::toggleBrowser);

    REQUIRE_FALSE (isTicked (open.shell.duetMenu(), "Browser"));
}

TEST_CASE ("the host puts its own entries under the shell's")
{
    OpenShell open;
    int chosen = 0;

    open.shell.setHostMenu ([] (juce::PopupMenu& menu)
                            { menu.addItem (MainShell::firstHostMenuId, "Open Project..."); },
                            [&chosen] (int id) { chosen = id; });

    const auto texts = itemsOf (open.shell.duetMenu());

    REQUIRE (std::find (texts.begin(), texts.end(), "Open Project...") != texts.end());

    open.shell.menuItemChosen (MainShell::firstHostMenuId);

    REQUIRE (chosen == MainShell::firstHostMenuId);
}

TEST_CASE ("a project opening under the shell lays it out on that project's view")
{
    OpenShell open;

    // What opening a project does: the view is replaced, and the shell is told.
    open.view.setBrowserWidthPx (260);
    open.view.setCollaboratorVisible (false);
    open.view.setBottomHeightPx (320);
    open.view.setBottomTab (BottomTab::mixer);
    open.shell.viewStateChanged();

    REQUIRE (open.surface (duet::gui::surfaceId::browser).getWidth() == 260);
    REQUIRE_FALSE (open.surface (duet::gui::surfaceId::collaborator).isVisible());
    REQUIRE (open.surface (duet::gui::surfaceId::bottomPanel).getHeight() == 320);
    REQUIRE (open.surface (duet::gui::surfaceId::mixer).isVisible());
}

TEST_CASE ("the surfaces are on the software renderer until something asks for the other one")
{
    OpenShell open;

    REQUIRE_FALSE (open.shell.isHardwareAccelerated());

    open.shell.setHardwareAccelerated (true);

    REQUIRE (open.shell.isHardwareAccelerated());

    // The escape hatch changes who rasterises the same drawing, and nothing
    // about the layout it rasterises.
    REQUIRE (open.surface (duet::gui::surfaceId::arrangement).isVisible());
    REQUIRE_FALSE (open.surface (duet::gui::surfaceId::arrangement).getBounds().isEmpty());

    open.shell.setHardwareAccelerated (false);

    REQUIRE_FALSE (open.shell.isHardwareAccelerated());
}

TEST_CASE ("the window's zoom keys reach the timeline")
{
    OpenShell open;

    // The whole chain, from the key the producer presses: the window's one
    // keyboard table, the arrangement's view-model, and the project's view.
    REQUIRE (open.shell.keyPressed (juce::KeyPress { '-' }));

    const auto zoomedOut = open.view.hZoomPxPerBeat();

    REQUIRE (zoomedOut < ViewState::defaultZoomPxPerBeat);

    REQUIRE (
        open.shell.keyPressed (juce::KeyPress { '+', juce::ModifierKeys::shiftModifier, '+' }));

    REQUIRE (open.view.hZoomPxPerBeat() > zoomedOut);
}

TEST_CASE ("the arrangement is the ruler and the timeline under it")
{
    const OpenShell open;

    const auto& arrangement = open.surface (duet::gui::surfaceId::arrangement);
    const auto& ruler = open.surface (duet::gui::surfaceId::arrangementRuler);

    REQUIRE (ruler.isVisible());
    REQUIRE (ruler.getWidth() == arrangement.getWidth());
    REQUIRE (ruler.getY() == 0);
    REQUIRE (ruler.getHeight() < arrangement.getHeight());
}
