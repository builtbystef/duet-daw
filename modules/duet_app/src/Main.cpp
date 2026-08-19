#include <duet/persistence/Project.h>

#include <duet/model/Session.h>

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <filesystem>
#include <memory>
#include <string_view>

namespace duet::app
{
namespace
{
    constexpr int windowWidth = 720;
    constexpr int windowHeight = 380;
    constexpr int statusRefreshMs = 100;
    constexpr int rowHeight = 40;
    constexpr int buttonWidth = 110;
    constexpr int boxWidth = 190;
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
        nextEditButton.onClick = [this] { stepThroughTheVocabulary(); };
        undoButton.onClick = [this] { undoOneStep(); };
        playButton.onClick = [this]
        { withProject ([] (auto& p) { p.session().startPlayback(); }); };
        stopButton.onClick = [this] { withProject ([] (auto& p) { p.session().stopPlayback(); }); };
        armButton.onClick = [this] { armTheLastTrack(); };
        recordButton.onClick = [this]
        { withProject ([] (auto& p) { p.session().startRecording(); }); };
        inputBox.onChange = [this] { chooseInput(); };
        monitorBox.onChange = [this] { chooseMonitoring(); };

        for (const auto& [mode, name] : monitorModes)
            monitorBox.addItem (juce::String (std::string { name }), static_cast<int> (mode) + 1);

        armButton.setClickingTogglesState (true);

        for (auto* button : { &newButton,
                              &openButton,
                              &saveButton,
                              &addTrackButton,
                              &playButton,
                              &stopButton,
                              &recordButton,
                              &armButton,
                              &nextEditButton,
                              &undoButton })
            addAndMakeVisible (*button);

        for (auto* box : { &inputBox, &monitorBox })
            addAndMakeVisible (*box);

        for (auto* label : { &projectLabel, &deviceLabel, &transportLabel, &vocabularyLabel })
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

        auto recording = area.removeFromTop (rowHeight);
        inputBox.setBounds (recording.removeFromLeft (boxWidth).reduced (2));
        monitorBox.setBounds (recording.removeFromLeft (boxWidth).reduced (2));
        for (auto* button : { &armButton, &recordButton })
            button->setBounds (recording.removeFromLeft (buttonWidth).reduced (2));

        auto vocabulary = area.removeFromTop (rowHeight);
        for (auto* button : { &nextEditButton, &undoButton })
            button->setBounds (vocabulary.removeFromLeft (buttonWidth).reduced (2));

        for (auto* label : { &projectLabel, &deviceLabel, &transportLabel, &vocabularyLabel })
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
        demoStep = 0;
        demoBus = duet::model::noTrack;
        demoReverbBus = duet::model::noTrack;
        demoReverb = duet::model::noPlugin;
        lastDemoStep = {};

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
                open.session().performAction ("Add a track",
                                              [number] (auto& ops) {
                                                  ops.createTrack (duet::model::TrackKind::audio,
                                                                   "Track "
                                                                       + std::to_string (number));
                                              });
            });

        refresh();
    }

    /** The track a take is recorded into: the last one added, which is the one
        Add Track just made.
    */
    [[nodiscard]] duet::model::TrackRef recordTrack() const
    {
        if (project == nullptr)
            return duet::model::noTrack;

        const auto tracks = project->session().tracks();

        return tracks.empty() ? duet::model::noTrack : tracks.back().track;
    }

    void chooseInput()
    {
        withProject (
            [this] (auto& open)
            {
                const auto chosen = inputBox.getSelectedId();
                const auto inputs = open.session().availableInputs();

                if (chosen < 1 || chosen > static_cast<int> (inputs.size()))
                    return;

                const auto input = inputs[static_cast<std::size_t> (chosen - 1)].input;
                open.session().setTrackInput (recordTrack(), input);
                monitorBox.setSelectedId (static_cast<int> (open.session().inputMonitoring (input))
                                              + 1,
                                          juce::dontSendNotification);
            });

        refresh();
    }

    void chooseMonitoring()
    {
        withProject (
            [this] (auto& open)
            {
                const auto chosen = monitorBox.getSelectedId();

                if (chosen < 1 || chosen > static_cast<int> (monitorModes.size()))
                    return;

                open.session().setInputMonitoring (
                    open.session().track (recordTrack()).input,
                    monitorModes.at (static_cast<std::size_t> (chosen - 1)).first);
            });

        refresh();
    }

    void armTheLastTrack()
    {
        withProject (
            [this] (auto& open)
            { open.session().setTrackRecordArmed (recordTrack(), armButton.getToggleState()); });

        refresh();
    }

    /** Fills the input list once a project is open, keeping whatever the
        producer chose selected.
    */
    void listInputs()
    {
        if (project == nullptr || inputBox.getNumItems() > 0)
            return;

        int id = 1;

        for (const auto& input : project->session().availableInputs())
            inputBox.addItem (juce::String (input.name)
                                  + (input.kind == duet::model::InputKind::midi ? " (MIDI)" : ""),
                              id++);
    }

    void refresh()
    {
        const auto hasProject = project != nullptr;

        for (auto* button : { &saveButton,
                              &addTrackButton,
                              &playButton,
                              &stopButton,
                              &recordButton,
                              &armButton,
                              &nextEditButton,
                              &undoButton })
            button->setEnabled (hasProject);

        for (auto* box : { &inputBox, &monitorBox })
            box->setEnabled (hasProject);

        listInputs();

        if (hasProject)
            armButton.setToggleState (project->session().track (recordTrack()).recordArmed,
                                      juce::dontSendNotification);

        nextEditButton.setEnabled (hasProject && demoStep < demoStepNames.size());

        vocabularyLabel.setText (vocabularyText(), juce::dontSendNotification);
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

    /** One Action from each domain of the vocabulary, one to a press.

        Scaffolding, and deliberately so: the producer-facing surfaces are issue
        535bbo's, and until they arrive this is how the vocabulary is listened to
        with the transport rolling. Press Play, then press this, and each step
        lands in the sound without a gap. Undo takes them back one at a time.
    */
    /** The monitoring modes, in the order the box lists them. */
    static const std::array<std::pair<duet::model::InputMonitoring, std::string_view>, 3>
        monitorModes;

    static constexpr std::array<std::string_view, 7> demoStepNames { "Add notes",
                                                                     "Duplicate and loop the clip",
                                                                     "Route into a group bus",
                                                                     "Set the mixer and a send",
                                                                     "Add a reverb and set it",
                                                                     "Draw a volume curve",
                                                                     "Change the tempo" };

    /** Takes the demo back one press, and the walk back with it.

        The counter is what names the next step and what decides whether there
        is one, so an undo that left it alone would leave the label describing a
        step that had just been taken away — and the next press would run the
        step after it against a project missing what it needs.
    */
    void undoOneStep()
    {
        withProject (
            [this] (auto& open)
            {
                if (open.session().undo() && demoStep > 0)
                    --demoStep;
            });

        refresh();
    }

    void stepThroughTheVocabulary()
    {
        withProject (
            [this] (auto& open)
            {
                auto& session = open.session();
                const auto tracks = session.tracks();

                if (tracks.empty() || tracks.front().clips.empty())
                    return;

                const auto track = tracks.front().track;
                const auto clip = tracks.front().clips.front().clip;
                const auto name = juce::String (std::string { demoStepNames.at (demoStep) });

                switch (demoStep)
                {
                    case 0:
                        session.performAction (
                            demoStepNames.at (0),
                            [clip] (auto& ops)
                            {
                                for (int note = 0; note < 8; ++note)
                                    ops.addNote (clip, 45 + note * 2, note * 1.0, 0.9, 90);
                            });
                        break;

                    case 1:
                        session.performAction (demoStepNames.at (1),
                                               [&] (auto& ops)
                                               {
                                                   ops.setClipLoop (clip, true, 8.0);

                                                   // Bar 9 and not bar 5. The
                                                   // copy is meant to be out of
                                                   // earshot so that what this
                                                   // step demonstrates is the
                                                   // loop, and bar 5 is exactly
                                                   // where the transport wraps —
                                                   // near enough to the seam that
                                                   // its opening note is on top
                                                   // of it.
                                                   ops.duplicateClip (clip,
                                                                      duet::model::noTrack,
                                                                      session.barStartSeconds (9));
                                               });
                        break;

                    case 2:
                        session.performAction (demoStepNames.at (2),
                                               [&] (auto& ops)
                                               {
                                                   demoBus = ops.createTrack (
                                                       duet::model::TrackKind::group, "Bus");
                                                   ops.setTrackOutput (track, demoBus);
                                               });
                        break;

                    case 3:
                        session.performAction (demoStepNames.at (3),
                                               [&] (auto& ops)
                                               {
                                                   ops.setTrackVolumeDb (track, -6.0);
                                                   ops.setTrackPan (track, -0.4);

                                                   // Its own bus, and not the one the track already
                                                   // outputs to: a send into that bus would be a
                                                   // second copy of a signal it is carrying anyway,
                                                   // which is both a meaningless routing and 6 dB
                                                   // nobody asked for.
                                                   demoReverbBus = ops.createTrack (
                                                       duet::model::TrackKind::group, "Reverb");
                                                   ops.setSend (track, demoReverbBus, -12.0);
                                               });
                        break;

                    case 4:
                        session.performAction (
                            demoStepNames.at (4),
                            [&] (auto& ops)
                            {
                                // After the return and not in front of it. The
                                // return is what puts the send into the bus, and
                                // it goes in at the head of the chain, so a
                                // reverb at position 0 would sit upstream of the
                                // only thing feeding it and process silence.
                                demoReverb = ops.addPlugin (
                                    demoReverbBus, duet::model::BuiltinPlugin::reverb, 1);
                                ops.setPluginParameter (demoReverb, "room size", 0.9);

                                // All wet: what the bus carries is the send, and
                                // the dry signal is already on its way out
                                // through the group bus.
                                ops.setPluginParameter (demoReverb, "wet level", 1.0);
                                ops.setPluginParameter (demoReverb, "dry level", 0.0);
                            });
                        break;

                    case 5:
                        session.performAction (
                            demoStepNames.at (5),
                            [track] (auto& ops)
                            {
                                ops.setAutomationPoints (
                                    duet::model::AutomationTarget::trackVolumeOf (track),
                                    { { 0.0, -24.0 }, { 4.0, -6.0 }, { 8.0, -24.0 } });
                            });
                        break;

                    default:
                        session.performAction (demoStepNames.at (6),
                                               [] (auto& ops)
                                               {
                                                   ops.setTempo (140.0);
                                                   ops.setTimeSignature (6, 8);
                                               });
                        break;
                }

                lastDemoStep = name;
                ++demoStep;
            });

        refresh();
    }

    [[nodiscard]] juce::String vocabularyText() const
    {
        if (project == nullptr)
            return {};

        if (demoStep >= demoStepNames.size())
            return text ("Vocabulary demo done — ") + lastDemoStep;

        return text ("Next edit: ") + juce::String (std::string { demoStepNames.at (demoStep) });
    }

    [[nodiscard]] juce::String transportText() const
    {
        if (project == nullptr)
            return "Stopped";

        auto& session = project->session();

        const auto* state = "Stopped";

        if (session.isRecording())
            state = "Recording";
        else if (session.isPlaying())
            state = "Playing";

        return juce::String (state) + text (" — ")
               + juce::String (session.playbackPositionSeconds(), 2) + " s" + text (" — into ")
               + juce::String (session.track (recordTrack()).name);
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
    juce::TextButton recordButton { "Record" };
    juce::TextButton armButton { "Arm" };
    juce::ComboBox inputBox;
    juce::ComboBox monitorBox;
    juce::TextButton nextEditButton { "Next Edit" };
    juce::TextButton undoButton { "Undo" };
    juce::Label projectLabel;
    juce::Label deviceLabel;
    juce::Label transportLabel;
    juce::Label vocabularyLabel;
    std::size_t demoStep = 0;
    duet::model::TrackRef demoBus = duet::model::noTrack;
    duet::model::TrackRef demoReverbBus = duet::model::noTrack;
    duet::model::PluginRef demoReverb = duet::model::noPlugin;
    juce::String lastDemoStep;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

const std::array<std::pair<duet::model::InputMonitoring, std::string_view>, 3>
    MainComponent::monitorModes { { { duet::model::InputMonitoring::off, "Monitor: off" },
                                    { duet::model::InputMonitoring::whileArmed,
                                      "Monitor: while armed" },
                                    { duet::model::InputMonitoring::on, "Monitor: on" } } };

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
