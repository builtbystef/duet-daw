#include "PropertyStorageSettings.h"

#include <duet/persistence/Project.h>

#include <duet/gui/Appearance.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/MainShell.h>
#include <duet/gui/Rendering.h>
#include <duet/gui/SessionClock.h>
#include <duet/gui/SettingsWindow.h>
#include <duet/gui/ViewState.h>
#include <duet/gui/WindowGeometry.h>

#include <juce_gui_extra/juce_gui_extra.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <utility>

namespace duet::app
{
namespace
{
    constexpr int titleRefreshMs = 400;

    std::filesystem::path toPath (const juce::File& file)
    {
        return std::filesystem::path { file.getFullPathName().toStdString() };
    }

    /** juce::String reads 8-bit data as ASCII unless it is told otherwise, and
        the shell's own text is UTF-8.
    */
    juce::String text (const char* utf8) { return juce::String { juce::CharPointer_UTF8 (utf8) }; }

    /** The Duet menu's entries that are the host's rather than the shell's.

        Ids from `firstHostMenuId` up are the host's, which is what keeps them
        clear of the panel toggles the shell puts above them.
    */
    enum HostMenuId : std::uint8_t
    {
        newProject = duet::gui::MainShell::firstHostMenuId,
        openProject,
        saveProject,
        playProject,
        stopProject,
        openSettings
    };
} // namespace

/** What the main window holds: the interface, and the project it is open on.

    The shell knows about panels and the producer's view of them; this knows
    about projects. The two meet in two places — the Duet menu, where the host's
    project commands go under the shell's panel toggles, and the view state,
    which the shell writes the producer's layout into and which this hands to the
    persistence facade to capture as a save begins.

    The project commands here are scaffolding: the untitled-project flow, Save
    As, Recent and the close prompt are issue ce17ym's, and Play and Stop belong
    to the transport bar (issue 1fumn6). They are in the menu so that the app can
    still open a project and be heard until those slices land.
*/
class ShellHost final : public juce::Component, private juce::Timer
{
public:
    ShellHost (duet::gui::Appearance& lookAndScale,
               duet::gui::Settings& store,
               std::function<void (const juce::String&)> titleChanged)
        : appearance (lookAndScale), settings (store), reportTitle (std::move (titleChanged))
    {
        shell.setHostMenu ([this] (juce::PopupMenu& menu) { addHostEntries (menu); },
                           [this] (int itemId) { hostItemChosen (itemId); });

        // The software renderer is the default, and this is the producer's own
        // answer from the last launch (spec 535bbo).
        shell.setHardwareAccelerated (duet::gui::hardwareAccelerationEnabled (settings));

        addAndMakeVisible (shell);
        refreshTitle();
        startTimer (titleRefreshMs);
    }

    ~ShellHost() override
    {
        // The shell reads the clock, and the clock reads the session: it stops
        // reading before either of them goes.
        shell.setTimelineClock (nullptr);
    }

    /** Puts the keys on the shell. The panel keys are the shell's, and a window
        that has just opened has given the focus to nothing.
    */
    void takeKeyboardFocus() { shell.grabKeyboardFocus(); }

    void resized() override { shell.setBounds (getLocalBounds()); }

private:
    void timerCallback() override { refreshTitle(); }

    void addHostEntries (juce::PopupMenu& menu) const
    {
        menu.addItem (HostMenuId::newProject, "New Project...");
        menu.addItem (HostMenuId::openProject, "Open Project...");
        menu.addItem (HostMenuId::saveProject, "Save Project", project != nullptr);
        menu.addSeparator();
        menu.addItem (HostMenuId::playProject, "Play", project != nullptr);
        menu.addItem (HostMenuId::stopProject, "Stop", project != nullptr);
        menu.addSeparator();
        menu.addItem (HostMenuId::openSettings, "Settings...");
    }

    void hostItemChosen (int itemId)
    {
        switch (itemId)
        {
            case HostMenuId::newProject:
                chooseFolder (true);
                break;

            case HostMenuId::openProject:
                chooseFolder (false);
                break;

            case HostMenuId::saveProject:
                if (project != nullptr)
                    project->save();
                break;

            case HostMenuId::playProject:
                if (project != nullptr)
                    project->session().startPlayback();
                break;

            case HostMenuId::stopProject:
                if (project != nullptr)
                    project->session().stopPlayback();
                break;

            default:
                openSettingsWindow();
                break;
        }

        refreshTitle();
    }

    void openSettingsWindow()
    {
        if (settingsWindow != nullptr)
        {
            settingsWindow->toFront (true);
            return;
        }

        settingsWindow = std::make_unique<duet::gui::SettingsWindow> (
            appearance,
            settings,
            [this] (bool accelerated) { shell.setHardwareAccelerated (accelerated); },
            [this] { settingsWindow.reset(); });
    }

    void chooseFolder (bool forNewProject)
    {
        const auto* const title = forNewProject ? "New project folder" : "Open a project folder";
        const auto browserFlags = juce::FileBrowserComponent::canSelectDirectories
                                  | (forNewProject ? juce::FileBrowserComponent::saveMode
                                                   : juce::FileBrowserComponent::openMode);

        chooser = std::make_unique<juce::FileChooser> (
            title, juce::File::getSpecialLocation (juce::File::userMusicDirectory));

        chooser->launchAsync (browserFlags,
                              [this, forNewProject] (const juce::FileChooser& chosen)
                              {
                                  const auto folder = chosen.getResult();

                                  if (folder == juce::File {})
                                      return;

                                  openFolder (toPath (folder), forNewProject);
                              });
    }

    void openFolder (const std::filesystem::path& folder, bool forNewProject)
    {
        // The clock reads the session, so it goes before the session does, and
        // the shell is told before either — a surface never holds a clock past
        // the project it belongs to.
        shell.setTimelineClock (nullptr);
        clock.reset();

        // The old project goes first: a session holds an engine, and an engine
        // holds the audio device.
        project.reset();

        project = forNewProject ? duet::persistence::Project::create (folder)
                                : duet::persistence::Project::open (folder);

        if (project == nullptr)
        {
            problem =
                forNewProject ? "Could not create a project there" : "No project to open there";
            refreshTitle();
            return;
        }

        problem = {};

        // The producer's layout for this project, and the arrangement that gets
        // it back onto disk: the view is asked for as a save begins and never
        // written by a gesture, so nothing about resizing a panel makes the
        // project dirty or reaches the undo history.
        view = duet::gui::ViewState {};
        view.readFrom (project->viewState());
        project->onCaptureViewState ([this] { return view.toData(); });

        // The timeline is drawn against the project's tempo and metre, and the
        // playhead against its transport.
        clock = std::make_unique<duet::gui::SessionClock> (project->session());
        shell.setTimelineClock (clock.get());

        if (forNewProject)
        {
            // The shell has no way to bring audio into a project yet, so a new
            // one is given the phrase the walking skeleton played.
            project->session().loadDemoContent();
            project->save();
        }

        refreshTitle();
    }

    void refreshTitle() { reportTitle (titleText()); }

    [[nodiscard]] juce::String titleText() const
    {
        if (problem.isNotEmpty())
            return text ("Duet — ") + problem;

        if (project == nullptr)
            return "Duet";

        // The dirty marker: the project has changes that are not on disk. An
        // asterisk because the marker has to survive whatever font the window
        // manager draws its title bar in, and a bullet does not.
        return text ("Duet — ") + juce::String (project->folder().filename().string())
               + (project->hasUnsavedChanges() ? juce::String (" *") : juce::String());
    }

    duet::gui::Appearance& appearance;
    duet::gui::Settings& settings;
    std::function<void (const juce::String&)> reportTitle;

    /** The open project's view. One per project, and the shell lays itself out
        from this one for the run of the app.
    */
    duet::gui::ViewState view;
    duet::gui::MainShell shell { appearance, view };

    std::unique_ptr<duet::persistence::Project> project;
    std::unique_ptr<duet::gui::SessionClock> clock;
    std::unique_ptr<duet::gui::SettingsWindow> settingsWindow;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String problem;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShellHost)
};

/** The one main window. Its geometry is app-global: where the producer put it is
    where it opens next time, whichever project opens in it (spec 535bbo).
*/
class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name,
                duet::gui::Appearance& lookAndScale,
                duet::gui::Settings& store)
        : DocumentWindow (name,
                          juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (
                              juce::ResizableWindow::backgroundColourId),
                          allButtons),
          settings (store)
    {
        setUsingNativeTitleBar (true);

        auto content = std::make_unique<ShellHost> (
            lookAndScale, store, [this] (const juce::String& title) { setName (title); });

        host = content.get();
        setContentOwned (content.release(), false);
        setResizable (true, false);

        if (const auto stored = duet::gui::storedWindowBounds (settings); stored.has_value())
        {
            setBounds (stored->x, stored->y, stored->width, stored->height);
        }
        else
        {
            // The default is in the desktop's own pixels and is not scaled: the
            // interface scale is what the chrome inside the window is measured
            // in, and a window bigger than the screen it opens on is a window
            // with its docks off both edges.
            const auto screen = juce::Desktop::getInstance()
                                    .getDisplays()
                                    .getPrimaryDisplay()
                                    ->userBounds.toNearestInt();

            centreWithSize (juce::jmin (duet::gui::defaultWindowWidth, screen.getWidth()),
                            juce::jmin (duet::gui::defaultWindowHeight, screen.getHeight()));
        }

        // Only from here on: the geometry above is what was stored, and storing
        // it back while it is being applied would write the window manager's
        // opinion over the producer's.
        restored = true;

        setVisible (true);
        host->takeKeyboardFocus();
    }

    ~MainWindow() override = default;

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    void moved() override
    {
        DocumentWindow::moved();
        rememberWhereItIs();
    }

    void resized() override
    {
        DocumentWindow::resized();
        rememberWhereItIs();
    }

private:
    void rememberWhereItIs()
    {
        if (! restored)
            return;

        const auto bounds = getBounds();

        duet::gui::storeWindowBounds (
            settings, { bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight() });
    }

    duet::gui::Settings& settings;
    ShellHost* host = nullptr;
    bool restored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

/** The application, and where the two things the whole interface reads live: the
    app-global settings store, and the appearance over it.

    Both outlive every project and every window, which is why they are here and
    not on a surface. The desktop's dark setting arrives here too, and is handed
    to the appearance — a producer on Follow OS gets the flip without a restart.
*/
class DuetApplication final : public juce::JUCEApplication, private juce::DarkModeSettingListener
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& /*commandLine*/) override
    {
        auto& desktop = juce::Desktop::getInstance();

        settings = std::make_unique<PropertyStorageSettings>();
        appearance =
            std::make_unique<duet::gui::Appearance> (*settings, desktop.isDarkModeActive());
        look = std::make_unique<duet::gui::GraphiteLookAndFeel> (*appearance);

        juce::LookAndFeel::setDefaultLookAndFeel (look.get());
        desktop.addDarkModeSettingListener (this);

        window = std::make_unique<MainWindow> (getApplicationName(), *appearance, *settings);
    }

    void shutdown() override
    {
        juce::Desktop::getInstance().removeDarkModeSettingListener (this);
        window.reset();

        // The look and feel goes before the appearance it listens to, and after
        // the windows that are drawn with it.
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        look.reset();
        appearance.reset();
        settings.reset();
    }

    void systemRequestedQuit() override { quit(); }

private:
    void darkModeSettingChanged() override
    {
        if (appearance != nullptr)
            appearance->systemDarkModeChanged (juce::Desktop::getInstance().isDarkModeActive());
    }

    std::unique_ptr<PropertyStorageSettings> settings;
    std::unique_ptr<duet::gui::Appearance> appearance;
    std::unique_ptr<duet::gui::GraphiteLookAndFeel> look;
    std::unique_ptr<MainWindow> window;
};
} // namespace duet::app

START_JUCE_APPLICATION (duet::app::DuetApplication)
