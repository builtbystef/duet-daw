#include <duet/persistence/Project.h>

#include <juce_gui_extra/juce_gui_extra.h>

#include <filesystem>
#include <memory>

namespace duet::app
{
namespace
{
    constexpr int windowWidth = 720;
    constexpr int windowHeight = 260;
    constexpr int statusRefreshMs = 100;
    constexpr int rowHeight = 40;
    constexpr int buttonWidth = 110;
    constexpr int labelHeight = 26;

    std::filesystem::path toPath (const juce::File& file)
    {
        return std::filesystem::path { file.getFullPathName().toStdString() };
    }

    /** juce::String reads 8-bit data as ASCII unless it is told otherwise, and
        the shell's own text is UTF-8.
    */
    juce::String text (const char* utf8) { return juce::String { juce::CharPointer_UTF8 (utf8) }; }
} // namespace

/** The shell's only surface until the producer-facing interface (issue 535bbo)
    lands: a project lifecycle thin enough to walk the persistence facade end to
    end, one edit that proves an Action reaches disk, and the transport.

    Deliberately throwaway. The Duet-menu lifecycle — Save As, Recent, the close
    prompt, the untitled-folder flow — is issue ce17ym's, and it replaces all of
    this.
*/
class MainComponent final : public juce::Component, private juce::Timer
{
public:
    explicit MainComponent (std::function<void (const juce::String&)> titleChanged)
        : reportTitle (std::move (titleChanged))
    {
        newButton.onClick = [this] { chooseFolder (true); };
        openButton.onClick = [this] { chooseFolder (false); };
        saveButton.onClick = [this] { saveProject(); };
        addTrackButton.onClick = [this] { addTrack(); };
        playButton.onClick = [this]
        { withProject ([] (auto& p) { p.session().startPlayback(); }); };
        stopButton.onClick = [this] { withProject ([] (auto& p) { p.session().stopPlayback(); }); };

        for (auto* button :
             { &newButton, &openButton, &saveButton, &addTrackButton, &playButton, &stopButton })
            addAndMakeVisible (*button);

        for (auto* label : { &projectLabel, &deviceLabel, &transportLabel })
            addAndMakeVisible (*label);

        setSize (windowWidth, windowHeight);
        refresh();
        startTimer (statusRefreshMs);
    }

    ~MainComponent() override = default;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        auto lifecycle = area.removeFromTop (rowHeight);
        for (auto* button : { &newButton, &openButton, &saveButton })
            button->setBounds (lifecycle.removeFromLeft (buttonWidth).reduced (2));

        auto transport = area.removeFromTop (rowHeight);
        for (auto* button : { &addTrackButton, &playButton, &stopButton })
            button->setBounds (transport.removeFromLeft (buttonWidth).reduced (2));

        for (auto* label : { &projectLabel, &deviceLabel, &transportLabel })
            label->setBounds (area.removeFromTop (labelHeight));
    }

private:
    void timerCallback() override { refresh(); }

    template <typename Job>
    void withProject (Job job)
    {
        if (project != nullptr)
            job (*project);
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
        // The old project goes first: a session holds an engine, and an engine
        // holds the audio device.
        project.reset();

        project = forNewProject ? duet::persistence::Project::create (folder)
                                : duet::persistence::Project::open (folder);

        if (project == nullptr)
        {
            problem =
                forNewProject ? "Could not create a project there" : "No project to open there";
            refresh();
            return;
        }

        problem = {};

        if (forNewProject)
        {
            // The shell has no way to bring audio into a project yet, so a new
            // one is given the phrase the walking skeleton played.
            project->session().loadDemoContent();
            project->save();
        }

        refresh();
    }

    void saveProject()
    {
        withProject (
            [this] (auto& open)
            {
                problem = open.save() ? juce::String {} : juce::String ("Could not save");
                refresh();
            });
    }

    void addTrack()
    {
        withProject (
            [] (auto& open)
            {
                const auto number = open.session().audioTrackCount() + 1;
                open.session().performAction (
                    "Add a track",
                    [number] (auto& ops) { ops.addTrack ("Track " + std::to_string (number)); });
            });

        refresh();
    }

    void refresh()
    {
        const auto hasProject = project != nullptr;

        for (auto* button : { &saveButton, &addTrackButton, &playButton, &stopButton })
            button->setEnabled (hasProject);

        projectLabel.setText (projectText(), juce::dontSendNotification);
        deviceLabel.setText (deviceText(), juce::dontSendNotification);
        transportLabel.setText (transportText(), juce::dontSendNotification);
        reportTitle (titleText());
    }

    [[nodiscard]] juce::String titleText() const
    {
        if (project == nullptr)
            return "Duet";

        // The dirty marker: the project has changes that are not on disk. An
        // asterisk because the marker has to survive whatever font the window
        // manager draws its title bar in, and a bullet does not.
        return text ("Duet — ") + juce::String (project->folder().filename().string())
               + (project->hasUnsavedChanges() ? juce::String (" *") : juce::String());
    }

    [[nodiscard]] juce::String projectText() const
    {
        if (problem.isNotEmpty())
            return problem;

        if (project == nullptr)
            return text ("No project — New or Open to start");

        return juce::String (project->folder().string()) + text (" — ")
               + juce::String (project->session().audioTrackCount()) + " tracks"
               + (project->hasUnsavedChanges() ? ", unsaved" : ", saved");
    }

    [[nodiscard]] juce::String deviceText() const
    {
        if (project == nullptr)
            return "No audio device";

        const auto device = project->session().audioDeviceDescription();

        return device.empty() ? juce::String ("No audio device")
                              : juce::String ("Device: ") + juce::String (device);
    }

    [[nodiscard]] juce::String transportText() const
    {
        if (project == nullptr)
            return "Stopped";

        auto& session = project->session();

        return juce::String (session.isPlaying() ? "Playing" : "Stopped") + text (" — ")
               + juce::String (session.playbackPositionSeconds(), 2) + " s";
    }

    std::function<void (const juce::String&)> reportTitle;
    std::unique_ptr<duet::persistence::Project> project;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String problem;

    juce::TextButton newButton { "New" };
    juce::TextButton openButton { "Open" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton addTrackButton { "Add Track" };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::Label projectLabel;
    juce::Label deviceLabel;
    juce::Label transportLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow (const juce::String& name)
        : DocumentWindow (name,
                          juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (
                              juce::ResizableWindow::backgroundColourId),
                          allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (
            new MainComponent { [this] (const juce::String& title) { setName (title); } }, true);
        setResizable (true, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~MainWindow() override = default;

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

class DuetApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& /*commandLine*/) override
    {
        window = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override { window.reset(); }

    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<MainWindow> window;
};
} // namespace duet::app

START_JUCE_APPLICATION (duet::app::DuetApplication)
