#include <duet/gui/SettingsWindow.h>

#include <duet/gui/AutosaveSettings.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/ProjectsSettings.h>
#include <duet/gui/Rendering.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <array>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

namespace duet::gui
{
namespace
{
    constexpr int windowWidth = 420;
    constexpr int windowHeight = 470;
    constexpr int labelWidth = 130;
    constexpr int tabBarHeight = 28;
    constexpr double scaleStep = 0.05;

    /** The theme choices, in the order the box offers them. The ids are the
        positions in this list, and JUCE reserves zero for "nothing chosen".
    */
    constexpr std::array<std::pair<ThemePreference, const char*>, 3> themeChoices {
        { { ThemePreference::followSystem, "Follow OS" },
          { ThemePreference::dark, "Dark" },
          { ThemePreference::light, "Light" } }
    };

    constexpr std::array<std::pair<duet::persistence::AutosaveInterval, const char*>, 4>
        autosaveChoices { { { duet::persistence::AutosaveInterval::off, "Off" },
                            { duet::persistence::AutosaveInterval::twoMinutes, "2 minutes" },
                            { duet::persistence::AutosaveInterval::fiveMinutes, "5 minutes" },
                            { duet::persistence::AutosaveInterval::tenMinutes, "10 minutes" } } };

    int idFor (ThemePreference preference)
    {
        for (std::size_t index = 0; index < themeChoices.size(); ++index)
            if (themeChoices.at (index).first == preference)
                return static_cast<int> (index) + 1;

        return 1;
    }

    int idFor (duet::persistence::AutosaveInterval interval)
    {
        for (std::size_t index = 0; index < autosaveChoices.size(); ++index)
            if (autosaveChoices.at (index).first == interval)
                return static_cast<int> (index) + 1;

        return static_cast<int> (autosaveChoices.size());
    }

    /** How tall the sample-folder list is, in logical units: enough rows to see
        a small library's folders without the tab growing with the list.
    */
    constexpr int sampleFolderListHeight = 84;

    /** The Interface tab. */
    class InterfaceTab final : public juce::Component,
                               private Appearance::Listener,
                               private juce::FilenameComponentListener,
                               private juce::ListBoxModel
    {
    public:
        InterfaceTab (Appearance& lookAndScale,
                      Settings& store,
                      Browser& dock,
                      const std::filesystem::path& defaultProjectsDirectory,
                      std::function<void (bool)> renderingChanged)
            : appearance (lookAndScale), settings (store), browser (dock),
              reportRendering (std::move (renderingChanged)),
              projectsDirectory (
                  "projectsDirectory",
                  juce::File {
                      duet::gui::projectsDirectory (store, defaultProjectsDirectory).string() },
                  true,
                  true,
                  true,
                  {},
                  {},
                  "Choose a projects directory")
        {
            themeLabel.setText ("Theme", juce::dontSendNotification);
            scaleLabel.setText ("Interface scale", juce::dontSendNotification);
            projectsLabel.setText ("Projects directory", juce::dontSendNotification);
            autosaveLabel.setText ("Autosave", juce::dontSendNotification);
            renderingLabel.setText ("Rendering", juce::dontSendNotification);

            for (std::size_t index = 0; index < themeChoices.size(); ++index)
                themeBox.addItem (themeChoices.at (index).second, static_cast<int> (index) + 1);

            themeBox.setSelectedId (idFor (appearance.themePreference()),
                                    juce::dontSendNotification);
            themeBox.onChange = [this]
            {
                const auto chosen = static_cast<std::size_t> (themeBox.getSelectedId() - 1);

                if (chosen < themeChoices.size())
                    appearance.setThemePreference (themeChoices.at (chosen).first);
            };

            scaleSlider.setSliderStyle (juce::Slider::LinearHorizontal);
            scaleSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
            scaleSlider.setRange (
                Appearance::minimumInterfaceScale, Appearance::maximumInterfaceScale, scaleStep);
            scaleSlider.setTextValueSuffix ("x");
            scaleSlider.setNumDecimalPlacesToDisplay (2);
            scaleSlider.setValue (appearance.interfaceScale(), juce::dontSendNotification);
            scaleSlider.onValueChange = [this]
            { appearance.setInterfaceScale (scaleSlider.getValue()); };

            projectsDirectory.addListener (this);

            for (std::size_t index = 0; index < autosaveChoices.size(); ++index)
                autosaveBox.addItem (autosaveChoices.at (index).second,
                                     static_cast<int> (index) + 1);

            autosaveBox.setSelectedId (idFor (autosaveInterval (settings)),
                                       juce::dontSendNotification);
            autosaveBox.onChange = [this]
            {
                const auto chosen = static_cast<std::size_t> (autosaveBox.getSelectedId() - 1);

                if (chosen < autosaveChoices.size())
                    setAutosaveInterval (settings, autosaveChoices.at (chosen).first);
            };

            sampleFoldersLabel.setText ("Sample folders", juce::dontSendNotification);
            sampleFolderList.setModel (this);
            sampleFolderList.setRowHeight (appearance.scaled (metrics::rowHeight));

            addFolder.setButtonText ("Add...");
            addFolder.onClick = [this] { chooseSampleFolder(); };

            removeFolder.setButtonText ("Remove");
            removeFolder.onClick = [this]
            {
                const auto chosen = sampleFolderList.getSelectedRow();
                const auto folders = browser.sampleFolders();

                if (chosen >= 0 && chosen < static_cast<int> (folders.size()))
                {
                    browser.removeSampleFolder (folders[static_cast<std::size_t> (chosen)]);
                    sampleFoldersChanged();
                }
            };

            renderingButton.setButtonText ("Hardware acceleration");
            renderingButton.setToggleState (hardwareAccelerationEnabled (settings),
                                            juce::dontSendNotification);
            renderingButton.onClick = [this]
            {
                const auto enabled = renderingButton.getToggleState();

                setHardwareAccelerationEnabled (settings, enabled);

                if (reportRendering)
                    reportRendering (enabled);
            };

            for (auto* child : std::initializer_list<juce::Component*> { &themeLabel,
                                                                         &scaleLabel,
                                                                         &projectsLabel,
                                                                         &autosaveLabel,
                                                                         &sampleFoldersLabel,
                                                                         &renderingLabel,
                                                                         &themeBox,
                                                                         &scaleSlider,
                                                                         &projectsDirectory,
                                                                         &autosaveBox,
                                                                         &sampleFolderList,
                                                                         &addFolder,
                                                                         &removeFolder,
                                                                         &renderingButton })
                addAndMakeVisible (*child);

            appearance.addListener (this);
        }

        ~InterfaceTab() override
        {
            projectsDirectory.removeListener (this);
            appearance.removeListener (this);
        }

        InterfaceTab (const InterfaceTab&) = delete;
        InterfaceTab& operator= (const InterfaceTab&) = delete;

        void paint (juce::Graphics& g) override
        {
            g.fillAll (toJuce (appearance.colour (ColourToken::surfaceDefault)));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (appearance.scaled (metrics::panelPadding));
            const auto rowHeight = appearance.scaled (metrics::rowHeight);
            const auto gap = appearance.scaled (metrics::rowGap);
            const auto labels = appearance.scaled (labelWidth);

            auto layOutRow = [&] (juce::Component& label, juce::Component& control)
            {
                auto row = area.removeFromTop (rowHeight);
                label.setBounds (row.removeFromLeft (labels));
                control.setBounds (row);
                area.removeFromTop (gap);
            };

            layOutRow (themeLabel, themeBox);
            layOutRow (scaleLabel, scaleSlider);
            layOutRow (projectsLabel, projectsDirectory);
            layOutRow (autosaveLabel, autosaveBox);

            auto folders = area.removeFromTop (appearance.scaled (sampleFolderListHeight));
            sampleFoldersLabel.setBounds (folders.removeFromLeft (labels));
            auto buttons = folders.removeFromBottom (rowHeight);
            addFolder.setBounds (
                buttons.removeFromLeft (buttons.getWidth() / 2).reduced (0, gap / 2));
            removeFolder.setBounds (buttons.reduced (0, gap / 2));
            sampleFolderList.setBounds (folders);
            sampleFolderList.setRowHeight (rowHeight);
            area.removeFromTop (gap);

            layOutRow (renderingLabel, renderingButton);
        }

    private:
        //==============================================================================
        int getNumRows() override { return static_cast<int> (browser.sampleFolders().size()); }

        void paintListBoxItem (int rowNumber,
                               juce::Graphics& g,
                               int width,
                               int height,
                               bool rowIsSelected) override
        {
            const auto folders = browser.sampleFolders();

            if (rowNumber < 0 || rowNumber >= static_cast<int> (folders.size()))
                return;

            if (rowIsSelected)
            {
                g.setColour (toJuce (appearance.colour (ColourToken::surfaceInteractive)));
                g.fillRect (0, 0, width, height);
            }

            g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
            g.setFont (interFont (appearance.scaled (typography::body)));
            g.drawText (juce::String { folders[static_cast<std::size_t> (rowNumber)].string() },
                        juce::Rectangle<int> { 0, 0, width, height },
                        juce::Justification::centredLeft,
                        true);
        }

        void chooseSampleFolder()
        {
            folderChooser = std::make_unique<juce::FileChooser> ("Choose a sample folder");
            folderChooser->launchAsync (juce::FileBrowserComponent::openMode
                                            | juce::FileBrowserComponent::canSelectDirectories,
                                        [this] (const juce::FileChooser& chooser)
                                        {
                                            const auto chosen = chooser.getResult();

                                            if (chosen == juce::File {})
                                                return;

                                            browser.addSampleFolder (
                                                chosen.getFullPathName().toStdString());
                                            sampleFoldersChanged();
                                        });
        }

        /** What this list shows is the browser's own folders, so the change is
            already in the dock by the time this runs: all that is left is the
            list itself.
        */
        void sampleFoldersChanged()
        {
            sampleFolderList.updateContent();
            sampleFolderList.repaint();
        }

        void filenameComponentChanged (juce::FilenameComponent* component) override
        {
            if (component == &projectsDirectory)
                setProjectsDirectory (settings,
                                      component->getCurrentFile().getFullPathName().toStdString());
        }

        void appearanceChanged() override
        {
            themeBox.setSelectedId (idFor (appearance.themePreference()),
                                    juce::dontSendNotification);
            scaleSlider.setValue (appearance.interfaceScale(), juce::dontSendNotification);

            // The scale is a layout, not a repaint: the rows are measured in
            // logical units, so the change is only on screen once they are laid
            // out against the new one.
            resized();
            repaint();
        }

        Appearance& appearance;
        Settings& settings;
        Browser& browser;
        std::function<void (bool)> reportRendering;
        juce::Label themeLabel;
        juce::Label scaleLabel;
        juce::Label projectsLabel;
        juce::Label autosaveLabel;
        juce::Label renderingLabel;
        juce::ComboBox themeBox;
        juce::Slider scaleSlider;
        juce::FilenameComponent projectsDirectory;
        juce::ComboBox autosaveBox;
        juce::Label sampleFoldersLabel;
        juce::ListBox sampleFolderList;
        juce::TextButton addFolder;
        juce::TextButton removeFolder;
        std::unique_ptr<juce::FileChooser> folderChooser;
        juce::ToggleButton renderingButton;
    };

    /** The Audio tab: one device at a time, and what it costs in latency. */
    class AudioTab final : public juce::Component, private Appearance::Listener
    {
    public:
        AudioTab (Appearance& lookAndScale, AudioMidiSettings& hardware)
            : appearance (lookAndScale), machine (hardware)
        {
            outputLabel.setText ("Output device", juce::dontSendNotification);
            inputLabel.setText ("Input device", juce::dontSendNotification);
            rateLabel.setText ("Sample rate", juce::dontSendNotification);
            bufferLabel.setText ("Buffer size", juce::dontSendNotification);
            latencyLabel.setText ("Latency", juce::dontSendNotification);

            outputBox.onChange = [this]
            {
                chose (outputBox,
                       [this] (const juce::String& name)
                       { return machine.setOutputDevice (name.toStdString()); });
            };

            inputBox.onChange = [this]
            {
                chose (inputBox,
                       [this] (const juce::String& name)
                       { return machine.setInputDevice (name.toStdString()); });
            };

            rateBox.onChange = [this]
            {
                if (rateBox.getSelectedId() > 0)
                    refreshAfter (
                        machine.setSampleRate (static_cast<double> (rateBox.getSelectedId())));
            };

            bufferBox.onChange = [this]
            {
                if (bufferBox.getSelectedId() > 0)
                    refreshAfter (machine.setBufferSize (bufferBox.getSelectedId()));
            };

            for (auto* child : std::initializer_list<juce::Component*> { &outputLabel,
                                                                         &inputLabel,
                                                                         &rateLabel,
                                                                         &bufferLabel,
                                                                         &latencyLabel,
                                                                         &outputBox,
                                                                         &inputBox,
                                                                         &rateBox,
                                                                         &bufferBox,
                                                                         &latency,
                                                                         &problem })
                addAndMakeVisible (*child);

            appearance.addListener (this);
            refresh();
        }

        ~AudioTab() override { appearance.removeListener (this); }

        AudioTab (const AudioTab&) = delete;
        AudioTab& operator= (const AudioTab&) = delete;

        void paint (juce::Graphics& g) override
        {
            g.fillAll (toJuce (appearance.colour (ColourToken::surfaceDefault)));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (appearance.scaled (metrics::panelPadding));
            const auto rowHeight = appearance.scaled (metrics::rowHeight);
            const auto gap = appearance.scaled (metrics::rowGap);
            const auto labels = appearance.scaled (labelWidth);

            const auto layOutRow = [&] (juce::Component& label, juce::Component& control)
            {
                auto row = area.removeFromTop (rowHeight);
                label.setBounds (row.removeFromLeft (labels));
                control.setBounds (row);
                area.removeFromTop (gap);
            };

            layOutRow (outputLabel, outputBox);
            layOutRow (inputLabel, inputBox);
            layOutRow (rateLabel, rateBox);
            layOutRow (bufferLabel, bufferBox);
            layOutRow (latencyLabel, latency);

            problem.setBounds (area.removeFromTop (rowHeight));
        }

        /** Reads the machine again: what a project opening, or a device that
            has just been opened, changes about every row.
        */
        void refresh()
        {
            fill (outputBox, machine.outputDevices(), machine.outputDevice());
            fill (inputBox, machine.inputDevices(), machine.inputDevice());

            fillNumbers (rateBox,
                         machine.sampleRates(),
                         machine.sampleRate(),
                         [] (double rate)
                         { return juce::String { static_cast<int> (rate) } + " Hz"; });

            std::vector<double> buffers;

            for (const auto size : machine.bufferSizes())
                buffers.push_back (static_cast<double> (size));

            fillNumbers (bufferBox,
                         buffers,
                         static_cast<double> (machine.bufferSize()),
                         [] (double size)
                         { return juce::String { static_cast<int> (size) } + " samples"; });

            latency.setText (machine.latencyText(), juce::dontSendNotification);
            problem.setText (machine.lastProblem(), juce::dontSendNotification);
        }

    private:
        static void fill (juce::ComboBox& box,
                          const std::vector<std::string>& names,
                          const std::string& chosen)
        {
            box.clear (juce::dontSendNotification);

            for (std::size_t index = 0; index < names.size(); ++index)
                box.addItem (juce::String { names[index] }, static_cast<int> (index) + 1);

            for (std::size_t index = 0; index < names.size(); ++index)
                if (names[index] == chosen)
                    box.setSelectedId (static_cast<int> (index) + 1, juce::dontSendNotification);
        }

        static void fillNumbers (juce::ComboBox& box,
                                 const std::vector<double>& values,
                                 double chosen,
                                 const std::function<juce::String (double)>& text)
        {
            box.clear (juce::dontSendNotification);

            for (const auto value : values)
                box.addItem (text (value), static_cast<int> (value));

            box.setSelectedId (static_cast<int> (chosen), juce::dontSendNotification);
        }

        void chose (juce::ComboBox& box, const std::function<bool (const juce::String&)>& open)
        {
            if (box.getSelectedId() > 0)
                refreshAfter (open (box.getText()));
        }

        /** Whatever the machine did, the rows say what it is doing now: a device
            that would not open leaves the one that was running selected, and the
            words about it in the row beneath.
        */
        void refreshAfter (bool opened)
        {
            static_cast<void> (opened);

            refresh();
            resized();
        }

        void appearanceChanged() override
        {
            resized();
            repaint();
        }

        Appearance& appearance;
        AudioMidiSettings& machine;
        juce::Label outputLabel;
        juce::Label inputLabel;
        juce::Label rateLabel;
        juce::Label bufferLabel;
        juce::Label latencyLabel;
        juce::ComboBox outputBox;
        juce::ComboBox inputBox;
        juce::ComboBox rateBox;
        juce::ComboBox bufferBox;
        juce::Label latency;
        juce::Label problem;
    };

    /** The MIDI tab: the machine's inputs, and which of them are switched on. */
    class MidiTab final : public juce::Component,
                          private Appearance::Listener,
                          private juce::ListBoxModel
    {
    public:
        MidiTab (Appearance& lookAndScale, AudioMidiSettings& hardware)
            : appearance (lookAndScale), machine (hardware)
        {
            title.setText ("MIDI inputs", juce::dontSendNotification);
            inputList.setModel (this);
            inputList.setRowHeight (appearance.scaled (metrics::rowHeight));

            toggle.setButtonText ("Enabled");
            toggle.onClick = [this] { switchChosenInput(); };

            addAndMakeVisible (title);
            addAndMakeVisible (inputList);
            addAndMakeVisible (toggle);

            appearance.addListener (this);
            refresh();
        }

        ~MidiTab() override { appearance.removeListener (this); }

        MidiTab (const MidiTab&) = delete;
        MidiTab& operator= (const MidiTab&) = delete;

        void paint (juce::Graphics& g) override
        {
            g.fillAll (toJuce (appearance.colour (ColourToken::surfaceDefault)));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (appearance.scaled (metrics::panelPadding));
            const auto rowHeight = appearance.scaled (metrics::rowHeight);
            const auto gap = appearance.scaled (metrics::rowGap);

            title.setBounds (area.removeFromTop (rowHeight));
            area.removeFromTop (gap);
            toggle.setBounds (area.removeFromBottom (rowHeight));
            area.removeFromBottom (gap);
            inputList.setRowHeight (rowHeight);
            inputList.setBounds (area);
        }

        void refresh()
        {
            inputs = machine.midiInputs();
            inputList.updateContent();
            inputList.repaint();
            refreshToggle();
        }

    private:
        int getNumRows() override { return static_cast<int> (inputs.size()); }

        void paintListBoxItem (int rowNumber,
                               juce::Graphics& g,
                               int width,
                               int height,
                               bool rowIsSelected) override
        {
            if (rowNumber < 0 || rowNumber >= static_cast<int> (inputs.size()))
                return;

            const auto& input = inputs[static_cast<std::size_t> (rowNumber)];

            if (rowIsSelected)
            {
                g.setColour (toJuce (appearance.colour (ColourToken::surfaceInteractive)));
                g.fillRect (0, 0, width, height);
            }

            g.setColour (toJuce (appearance.colour (input.enabled ? ColourToken::textPrimary
                                                                  : ColourToken::textDisabled)));
            g.setFont (interFont (appearance.scaled (typography::body)));
            g.drawText (juce::String { input.name },
                        juce::Rectangle<int> { 0, 0, width, height },
                        juce::Justification::centredLeft,
                        true);
        }

        void selectedRowsChanged (int lastRowSelected) override
        {
            static_cast<void> (lastRowSelected);
            refreshToggle();
        }

        void refreshToggle()
        {
            const auto* const chosen = chosenInput();

            toggle.setEnabled (chosen != nullptr);
            toggle.setToggleState (chosen != nullptr && chosen->enabled,
                                   juce::dontSendNotification);
        }

        [[nodiscard]] const duet::model::MidiInputInfo* chosenInput() const
        {
            const auto row = inputList.getSelectedRow();

            if (row < 0 || row >= static_cast<int> (inputs.size()))
                return nullptr;

            return &inputs[static_cast<std::size_t> (row)];
        }

        void switchChosenInput()
        {
            if (const auto* chosen = chosenInput())
                machine.setMidiInputEnabled (chosen->input, toggle.getToggleState());

            refresh();
        }

        void appearanceChanged() override
        {
            resized();
            repaint();
        }

        Appearance& appearance;
        AudioMidiSettings& machine;
        juce::Label title;
        juce::ListBox inputList;
        juce::ToggleButton toggle;
        std::vector<duet::model::MidiInputInfo> inputs;
    };

} // namespace

/** The tabs, and the appearance they are measured in. */
class SettingsPanel::Tabs final : public juce::Component, private Appearance::Listener
{
public:
    Tabs (Appearance& lookAndScale,
          Settings& store,
          Browser& dock,
          AudioMidiSettings& machine,
          const std::filesystem::path& defaultProjectsDirectory,
          std::function<void (bool)> renderingChanged)
        : appearance (lookAndScale), audio (new AudioTab { lookAndScale, machine }),
          midi (new MidiTab { lookAndScale, machine })
    {
        // The tabbed component takes each of the three; the two the machine
        // fills in are kept as well, so that opening the window can read the
        // machine again.
        tabs.addTab (
            "Interface",
            juce::Colours::transparentBlack,
            new InterfaceTab {
                appearance, store, dock, defaultProjectsDirectory, std::move (renderingChanged) },
            true);
        tabs.addTab ("Audio", juce::Colours::transparentBlack, audio, true);
        tabs.addTab ("MIDI", juce::Colours::transparentBlack, midi, true);

        addAndMakeVisible (tabs);
        appearance.addListener (this);
        setWantsKeyboardFocus (true);
        setSize (appearance.scaled (windowWidth), appearance.scaled (windowHeight));
    }

    /** Puts the window on one tab, and reads the machine again as it opens: the
        device the producer is looking at is the device that is running now.
    */
    void showTab (int index)
    {
        tabs.setCurrentTabIndex (index);

        if (audio != nullptr)
            audio->refresh();

        if (midi != nullptr)
            midi->refresh();
    }

    ~Tabs() override { appearance.removeListener (this); }

    Tabs (const Tabs&) = delete;
    Tabs& operator= (const Tabs&) = delete;

    [[nodiscard]] int currentTab() const { return tabs.getCurrentTabIndex(); }

    [[nodiscard]] std::vector<juce::String> tabNames() const
    {
        std::vector<juce::String> names;

        for (const auto& name : tabs.getTabNames())
            names.push_back (name);

        return names;
    }

    void resized() override
    {
        tabs.setTabBarDepth (appearance.scaled (tabBarHeight));
        tabs.setBounds (getLocalBounds());
    }

private:
    void appearanceChanged() override
    {
        // The whole tree, and not only this component: a ComboBox copies the
        // look and feel's colours into the label it draws its text with when
        // it is told the look has changed, and it is told by nothing else —
        // so without this the box keeps painting the old theme's ink over
        // the new theme's surface.
        sendLookAndFeelChange();

        setSize (appearance.scaled (windowWidth), appearance.scaled (windowHeight));

        // The panel is what the window is sized to, so the new measurement has
        // to reach it: a scale the window did not follow is a window with its
        // rows cut off.
        if (auto* panel = getParentComponent())
            panel->setSize (getWidth(), getHeight());

        // A same-bounds setSize skips resized(), and the tab bar's depth is
        // measured in logical units too (prototype finding, r4m858).
        resized();
        repaint();
    }

    Appearance& appearance;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    AudioTab* audio = nullptr;
    MidiTab* midi = nullptr;
};

SettingsPanel::SettingsPanel (Appearance& lookAndScale,
                              Settings& store,
                              Browser& dock,
                              AudioMidiSettings& machine,
                              const std::filesystem::path& defaultProjectsDirectory,
                              std::function<void (bool)> renderingChanged,
                              std::function<void()> onClose)
    : tabs (std::make_unique<Tabs> (lookAndScale,
                                    store,
                                    dock,
                                    machine,
                                    defaultProjectsDirectory,
                                    std::move (renderingChanged))),
      dismiss (std::move (onClose))
{
    addAndMakeVisible (*tabs);
    setWantsKeyboardFocus (true);
    setSize (tabs->getWidth(), tabs->getHeight());
}

SettingsPanel::~SettingsPanel() = default;

void SettingsPanel::resized() { tabs->setBounds (getLocalBounds()); }

bool SettingsPanel::keyPressed (const juce::KeyPress& key)
{
    if (key != juce::KeyPress::escapeKey)
        return false;

    if (dismiss)
        dismiss();

    return true;
}

void SettingsPanel::showTab (int index) { tabs->showTab (index); }

int SettingsPanel::currentTab() const { return tabs->currentTab(); }

std::vector<juce::String> SettingsPanel::tabNames() const { return tabs->tabNames(); }

//==============================================================================
SettingsWindow::SettingsWindow (Appearance& lookAndScale,
                                Settings& store,
                                Browser& dock,
                                AudioMidiSettings& machine,
                                const std::filesystem::path& defaultProjectsDirectory,
                                std::function<void (bool)> renderingChanged,
                                std::function<void()> onClose)
    : DocumentWindow ("Settings",
                      juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (
                          juce::ResizableWindow::backgroundColourId),
                      closeButton),
      closed (std::move (onClose))
{
    auto owned = std::make_unique<SettingsPanel> (lookAndScale,
                                                  store,
                                                  dock,
                                                  machine,
                                                  defaultProjectsDirectory,
                                                  std::move (renderingChanged),
                                                  [this] { closeButtonPressed(); });
    panel = owned.get();

    setUsingNativeTitleBar (true);
    setContentOwned (owned.release(), true);
    setResizable (true, false);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);

    if (auto* held = getContentComponent())
        held->grabKeyboardFocus();
}

SettingsWindow::~SettingsWindow() = default;

void SettingsWindow::closeButtonPressed()
{
    if (closed)
        closed();
}

void SettingsWindow::showTab (int index)
{
    if (panel != nullptr)
        panel->showTab (index);

    toFront (true);
}
} // namespace duet::gui
