#include <duet/app/Collaborator.h>
#include <duet/app/ProjectLifecycle.h>
#include <duet/app/PropertyStorageSettings.h>
#include <duet/model/Session.h>
#include <duet/persistence/Project.h>

#include <duet/gui/Appearance.h>
#include <duet/gui/AutosaveSettings.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/MainShell.h>
#include <duet/gui/PluginEditorManager.h>
#include <duet/gui/ProjectsSettings.h>
#include <duet/gui/Rendering.h>
#include <duet/gui/SessionClock.h>
#include <duet/gui/SettingsWindow.h>
#include <duet/gui/Text.h>
#include <duet/gui/ViewState.h>
#include <duet/gui/WindowGeometry.h>

#include <juce_gui_extra/juce_gui_extra.h>

#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace duet::app
{
namespace
{
    constexpr int titleRefreshMs = 400;

    /** How long the Collaborator service's thread waits for the message thread
        to answer a project read.
    */
    constexpr int marshalTimeoutMs = 5000;

    std::filesystem::path toPath (const juce::File& file)
    {
        return std::filesystem::path { file.getFullPathName().toStdString() };
    }

    /** The Duet menu's entries that are the host's rather than the shell's.

        Ids from `firstHostMenuId` up are the host's, which is what keeps them
        clear of the panel toggles the shell puts above them.
    */
    enum HostMenuId : std::uint8_t
    {
        newProject = duet::gui::MainShell::firstHostMenuId,
        openProject,
        saveProject,
        saveProjectAs,
        playProject,
        stopProject,
        openSettings,
        firstRecentProject = 200
    };

    std::filesystem::path defaultProjectsDirectory()
    {
        return toPath (juce::File::getSpecialLocation (juce::File::userHomeDirectory)) / "Music"
               / "Duet Projects";
    }

    /** How the Collaborator service is configured for this launch.

        The socket goes in a folder of its own under the system temp directory,
        named for this process so that two Duets never share one, and the path
        is kept short because `sun_path` is 108 bytes. The sidecar is the one
        that ships beside the application (ADR 0003).
    */
    duet::collab::CollaboratorService::Configuration collaboratorConfiguration()
    {
        const auto temporary = toPath (juce::File::getSpecialLocation (juce::File::tempDirectory));
        const auto folder = temporary / ("duet-" + std::to_string (::getpid()));

        std::error_code ignored;
        std::filesystem::create_directories (folder, ignored);

        duet::collab::CollaboratorService::Configuration configuration;
        configuration.socketPath = folder / "collab.sock";
        configuration.sidecar.executable =
            toPath (juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                        .getParentDirectory()
                        .getChildFile ("duet-sidecar"));

        return configuration;
    }

    /** Where the renders a measured or estimated answer is read off are kept:
        beside the socket rather than in the producer's project folder, since
        nothing about them belongs to the project.
    */
    std::filesystem::path renderFolderFor (const std::filesystem::path& socketPath)
    {
        const auto folder = socketPath.parent_path() / "renders";

        std::error_code ignored;
        std::filesystem::create_directories (folder, ignored);

        return folder;
    }

    /** Runs one piece of work on the message thread and waits for it to have
        run: what the Collaborator service's thread reads the project through.
    */
    void runOnMessageThread (const std::function<void()>& work)
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            work();
            return;
        }

        juce::WaitableEvent done;

        juce::MessageManager::callAsync (
            [&work, &done]
            {
                work();
                done.signal();
            });

        // Bounded, because the caller is the service thread and a tool read
        // that never comes back is a run that never ends. What a read that did
        // not run answers is that the project could not be read.
        done.wait (marshalTimeoutMs);
    }
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
        : appearance (lookAndScale), settings (store),
          lifecycle (store, defaultProjectsDirectory()), reportTitle (std::move (titleChanged))
    {
        shell.setPluginEditorAction ([this] (duet::model::PluginRef plugin)
                                     { pluginEditors.open (plugin); });
        shell.setHostMenu ([this] (juce::PopupMenu& menu) { addHostEntries (menu); },
                           [this] (int itemId) { hostItemChosen (itemId); });
        shell.setSaveAction (
            [this]
            {
                lifecycle.save();
                refreshTitle();
            });

        // The software renderer is the default, and this is the producer's own
        // answer from the last launch (spec 535bbo).
        shell.setHardwareAccelerated (duet::gui::hardwareAccelerationEnabled (settings));

        startCollaborator();

        addAndMakeVisible (shell);
        launchInitialProject();
        refreshTitle();
        startTimer (titleRefreshMs);
    }

    ~ShellHost() override
    {
        // The service's thread is what calls into the Collaborator, and nothing
        // waits for a call already inside a tool, so the service stops before
        // the Collaborator that answers it goes.
        collaboratorService.stop();
        collaborator.setSession (nullptr, {});

        // The socket is the service's to remove; the folder it stood in, and
        // the renders beside it, are this launch's and go with it.
        std::error_code ignored;
        std::filesystem::remove_all (collaboration.socketPath.parent_path(), ignored);

        // The shell and plugin windows stop reading the session before it goes,
        // and the surfaces stop reading a manager that has gone with it.
        shell.pendingSuggestions().setSource (nullptr);
        pluginEditors.setSession (nullptr);
        shell.setTimelineClock (nullptr);
        shell.setSession (nullptr);
    }

    /** Puts the keys on the shell. The panel keys are the shell's, and a window
        that has just opened has given the focus to nothing.
    */
    void takeKeyboardFocus() { shell.grabKeyboardFocus(); }

    void requestClose (std::function<void()> close)
    {
        askAboutUnsavedChanges (
            [this, close = std::move (close)] (UnsavedDecision decision)
            {
                if (lifecycle.mayClose (decision))
                    close();
                else
                    refreshTitle();
            });
    }

    void resized() override { shell.setBounds (getLocalBounds()); }

private:
    void timerCallback() override
    {
        syncAutosaveInterval();

        if (auto* project = lifecycle.projectOrNull())
            project->autosaveTick();

        refreshTitle();
    }

    void syncAutosaveInterval()
    {
        auto* project = lifecycle.projectOrNull();

        if (project == nullptr)
            return;

        const auto stored = duet::gui::autosaveInterval (settings);

        if (project->autosaveInterval() != stored)
            project->setAutosaveInterval (stored);
    }

    void addHostEntries (juce::PopupMenu& menu)
    {
        const auto hasProject = lifecycle.projectOrNull() != nullptr;
        menu.addItem (HostMenuId::newProject, "New");
        menu.addItem (HostMenuId::openProject, "Open...");
        menu.addItem (HostMenuId::saveProject, "Save", hasProject);
        menu.addItem (HostMenuId::saveProjectAs, "Save As...", hasProject);

        juce::PopupMenu recentMenu;
        const auto recent = lifecycle.recentProjects();

        for (std::size_t index = 0; index < recent.size(); ++index)
            recentMenu.addItem (HostMenuId::firstRecentProject + static_cast<int> (index),
                                juce::String { recent.at (index).filename().string() });

        menu.addSubMenu ("Recent", recentMenu, ! recent.empty());
        menu.addSeparator();
        menu.addItem (HostMenuId::playProject, "Play", hasProject);
        menu.addItem (HostMenuId::stopProject, "Stop", hasProject);
        menu.addSeparator();
        menu.addItem (HostMenuId::openSettings, "Settings...");
    }

    void hostItemChosen (int itemId)
    {
        if (itemId >= HostMenuId::firstRecentProject)
        {
            const auto recent = lifecycle.recentProjects();
            const auto index = static_cast<std::size_t> (itemId - HostMenuId::firstRecentProject);

            if (index < recent.size())
                beginOpen (recent.at (index));

            return;
        }

        switch (itemId)
        {
            case HostMenuId::newProject:
                askAboutUnsavedChanges ([this] (UnsavedDecision decision)
                                        { replaceWithUntitled (decision); });
                break;

            case HostMenuId::openProject:
                askAboutUnsavedChanges ([this] (UnsavedDecision decision)
                                        { chooseProjectToOpen (decision); });
                break;

            case HostMenuId::saveProject:
                lifecycle.save();
                break;

            case HostMenuId::saveProjectAs:
                chooseSaveAsDestination();
                break;

            case HostMenuId::playProject:
                if (auto* project = lifecycle.projectOrNull())
                    project->session().startPlayback();
                break;

            case HostMenuId::stopProject:
                if (auto* project = lifecycle.projectOrNull())
                    project->session().stopPlayback();
                break;

            case HostMenuId::openSettings:
                openSettingsWindow();
                break;

            default:
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
            defaultProjectsDirectory(),
            [this] (bool accelerated) { shell.setHardwareAccelerated (accelerated); },
            [this] { settingsWindow.reset(); });
    }

    void launchInitialProject()
    {
        const auto startup = lifecycle.startupProjectFolder();

        if (startup.has_value() && duet::persistence::Project::recoveryAvailable (*startup))
        {
            offerRecovery (
                [this] (duet::persistence::RecoveryChoice choice)
                {
                    lifecycle.launch (choice);
                    attachProject();
                });
            return;
        }

        lifecycle.launch();
        attachProject();
    }

    void askAboutUnsavedChanges (std::function<void (UnsavedDecision)> continueWith)
    {
        auto* project = lifecycle.projectOrNull();

        if (project == nullptr || ! project->hasUnsavedChanges())
        {
            continueWith (UnsavedDecision::discard);
            return;
        }

        const juce::Component::SafePointer<ShellHost> host { this };
        const auto options =
            juce::MessageBoxOptions {}
                .withIconType (juce::MessageBoxIconType::QuestionIcon)
                .withTitle ("Save changes?")
                .withMessage ("Save changes to " + juce::String { lifecycle.projectName() } + "?")
                .withButton ("Save")
                .withButton ("Discard")
                .withButton ("Cancel");

        juce::AlertWindow::showAsync (options,
                                      [host, continueWith = std::move (continueWith)] (int result)
                                      {
                                          if (host == nullptr)
                                              return;

                                          auto decision = UnsavedDecision::cancel;

                                          if (result == 1)
                                              decision = UnsavedDecision::save;
                                          else if (result == 2)
                                              decision = UnsavedDecision::discard;

                                          continueWith (decision);
                                      });
    }

    void replaceWithUntitled (UnsavedDecision decision)
    {
        if (decision == UnsavedDecision::cancel)
            return;

        detachProject();
        const auto changed = lifecycle.createNew (decision);
        attachProject();

        if (! changed)
            showLifecycleError ("Could not create project");
    }

    void chooseProjectToOpen (UnsavedDecision decision)
    {
        if (decision == UnsavedDecision::cancel)
            return;

        chooser = std::make_unique<juce::FileChooser> (
            "Open a project folder",
            juce::File {
                duet::gui::projectsDirectory (settings, defaultProjectsDirectory()).string() });
        chooser->launchAsync (juce::FileBrowserComponent::canSelectDirectories
                                  | juce::FileBrowserComponent::openMode,
                              [this, decision] (const juce::FileChooser& chosen)
                              {
                                  if (chosen.getResult() != juce::File {})
                                      openAfterDecision (toPath (chosen.getResult()), decision);
                              });
    }

    void beginOpen (const std::filesystem::path& folder)
    {
        askAboutUnsavedChanges ([this, folder] (UnsavedDecision decision)
                                { openAfterDecision (folder, decision); });
    }

    void openAfterDecision (const std::filesystem::path& folder, UnsavedDecision decision)
    {
        if (decision == UnsavedDecision::cancel)
            return;

        if (duet::persistence::Project::recoveryAvailable (folder))
        {
            offerRecovery ([this, folder, decision] (duet::persistence::RecoveryChoice choice)
                           { replaceWithProject (folder, choice, decision); });
            return;
        }

        replaceWithProject (folder, duet::persistence::RecoveryChoice::decline, decision);
    }

    void replaceWithProject (const std::filesystem::path& folder,
                             duet::persistence::RecoveryChoice recoveryChoice,
                             UnsavedDecision decision)
    {
        detachProject();
        const auto changed = lifecycle.open (folder, recoveryChoice, decision);
        attachProject();

        if (! changed)
            showLifecycleError ("Could not open project");
    }

    void chooseSaveAsDestination()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Save project as",
            juce::File {
                duet::gui::projectsDirectory (settings, defaultProjectsDirectory()).string() });
        chooser->launchAsync (
            juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::saveMode,
            [this] (const juce::FileChooser& chosen)
            {
                if (chosen.getResult() == juce::File {})
                    return;

                detachProject();
                const auto changed = lifecycle.saveAs (toPath (chosen.getResult()));
                attachProject();

                if (! changed)
                    showLifecycleError ("Could not save project as");
            });
    }

    void offerRecovery (std::function<void (duet::persistence::RecoveryChoice)> continueWith)
    {
        const juce::Component::SafePointer<ShellHost> host { this };
        const auto options =
            juce::MessageBoxOptions {}
                .withIconType (juce::MessageBoxIconType::QuestionIcon)
                .withTitle ("Restore autosaved project?")
                .withMessage ("Duet found newer autosaved changes. Restore them, or open the last "
                              "explicitly saved project?")
                .withButton ("Restore")
                .withButton ("Open Saved");

        juce::AlertWindow::showAsync (
            options,
            [host, continueWith = std::move (continueWith)] (int result)
            {
                if (host != nullptr)
                    continueWith (result == 1 ? duet::persistence::RecoveryChoice::restore
                                              : duet::persistence::RecoveryChoice::decline);
            });
    }

    void detachProject()
    {
        // Whatever was pending goes with the project it was made against, and
        // the surfaces are told so before they are asked to draw again.
        collaborator.setSession (nullptr, {});
        pluginEditors.setSession (nullptr);
        shell.setTimelineClock (nullptr);
        shell.setSession (nullptr);
        clock.reset();
    }

    void attachProject()
    {
        auto* project = lifecycle.projectOrNull();

        if (project == nullptr)
        {
            problem = juce::String { lifecycle.lastError() };
            refreshTitle();
            return;
        }

        problem = {};
        view = duet::gui::ViewState {};
        view.readFrom (project->viewState());
        project->onCaptureViewState ([this] { return view.toData(); });

        clock = std::make_unique<duet::gui::SessionClock> (project->session());
        pluginEditors.setSession (&project->session());

        // The Collaborator first: its manager is what the shell's Suggestions
        // read, and the shell reads them as it takes the project.
        collaborator.setSession (&project->session(), renderFolder);
        shell.setSession (&project->session());
        shell.setTimelineClock (clock.get());
        shell.viewStateChanged();
        refreshTitle();
    }

    /** Puts the socket in place and the Collaborator on the panel.

        A service that cannot start is not a reason to keep the producer out of
        their project: what it costs is that every run fails with one line, and
        the rest of the DAW is untouched (spec js437t).
    */
    void startCollaborator()
    {
        shell.collaborator().setSource (&collaborator);

        // One Suggestion manager behind all three surfaces that show a
        // Suggestion: the card in the conversation, the ghosts on the timeline
        // and the ghost marks in the mixer.
        shell.pendingSuggestions().setSource (&collaborator.suggestionSurfaces());

        try
        {
            collaboratorService.start();
        }
        catch (const std::exception& failure)
        {
            static_cast<void> (failure);
        }
    }

    /** What a Task Run carries about the producer at the moment it starts.

        Clips first, because a clip selection is what the producer made
        deliberately, and the track they are working on otherwise — the same
        answer the panel's own chip is made of.
    */
    [[nodiscard]] duet::collab::OpeningContext openingContext() const
    {
        duet::collab::OpeningContext context;

        if (const auto clips = shell.selectedClips(); ! clips.empty())
        {
            context.selection = duet::collab::SelectionKind::clips;

            for (const auto clip : clips)
                context.selectionIds.push_back (duet::collab::toolId::forClip (clip));
        }
        else if (const auto track = shell.focusedTrack(); track != duet::model::noTrack)
        {
            context.selection = duet::collab::SelectionKind::tracks;
            context.selectionIds.push_back (duet::collab::toolId::forTrack (track));
        }

        if (clock != nullptr)
        {
            // Bars and beats as the producer reads them: both count from one.
            const auto perBar = std::max (1.0, clock->beatsPerBar());
            const auto beats = std::max (0.0, clock->playheadBeats());

            context.playheadBar = static_cast<int> (std::floor (beats / perBar)) + 1;
            context.playheadBeat = std::fmod (beats, perBar) + 1.0;
        }

        if (const auto* project = lifecycle.projectOrNull())
            context.transportPlaying = project->session().isPlaying();

        return context;
    }

    void showLifecycleError (const char* title)
    {
        const auto message = juce::String { lifecycle.lastError() };
        problem = message;

        if (message.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, title, message);

        refreshTitle();
    }

    void refreshTitle()
    {
        if (const auto* project = lifecycle.projectOrNull())
            shell.setProjectStatus (lifecycle.projectName(), project->hasUnsavedChanges());
        else
            shell.setProjectStatus ({}, false);

        reportTitle (titleText());
    }

    [[nodiscard]] juce::String titleText() const
    {
        if (problem.isNotEmpty())
            return duet::gui::utf8 ("Duet — ") + problem;

        const auto* project = lifecycle.projectOrNull();

        if (project == nullptr)
            return "Duet";

        // The dirty marker: the project has changes that are not on disk. An
        // asterisk because the marker has to survive whatever font the window
        // manager draws its title bar in, and a bullet does not.
        return duet::gui::utf8 ("Duet — ") + juce::String { lifecycle.projectName() }
               + (project->hasUnsavedChanges() ? juce::String (" *") : juce::String());
    }

    duet::gui::Appearance& appearance;
    duet::gui::Settings& settings;
    ProjectLifecycle lifecycle;
    std::function<void (const juce::String&)> reportTitle;

    /** The open project's view. One per project, and the shell lays itself out
        from this one for the run of the app.
    */
    duet::gui::ViewState view;
    duet::gui::PluginEditorManager pluginEditors { settings };
    duet::gui::MainShell shell { appearance, view };

    /** The DAW half of the AI seam, and what puts it on the panel. The service
        is declared first so that it is built first and torn down last: the
        Collaborator registers on it and is what its thread calls into.
    */
    duet::collab::CollaboratorService::Configuration collaboration = collaboratorConfiguration();
    duet::collab::CollaboratorService collaboratorService { collaboration };
    std::filesystem::path renderFolder = renderFolderFor (collaboration.socketPath);
    duet::app::Collaborator collaborator {
        collaboratorService,
        shell.collaborator(),
        [this] { return openingContext(); },
        duet::app::Collaborator::MessageThread {
            [] (const std::function<void()>& work) { runOnMessageThread (work); },
            [] (std::function<void()> work)
            { juce::MessageManager::callAsync (std::move (work)); } }
    };

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

    void closeButtonPressed() override { requestClose(); }

    void requestClose()
    {
        host->requestClose ([] { juce::JUCEApplication::quit(); });
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

    void initialise (const juce::String& commandLine) override
    {
        if (duet::model::Session::startPluginScanChild (commandLine.toStdString()))
            return;

        auto& desktop = juce::Desktop::getInstance();

        settings = std::make_unique<PropertyStorageSettings>();

        // Which Duet last wrote the store. The shell is where this is stamped
        // because the version is the application's, and the store outlives
        // every project that runs under it.
        settings->setValue ("duetApplicationVersion", JUCE_APPLICATION_VERSION_STRING);

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

    void systemRequestedQuit() override
    {
        if (window != nullptr)
            window->requestClose();
        else
            quit();
    }

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
