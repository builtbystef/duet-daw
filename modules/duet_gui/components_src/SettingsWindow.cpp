#include <duet/gui/SettingsWindow.h>

#include <duet/gui/AutosaveSettings.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/ProjectsSettings.h>
#include <duet/gui/Rendering.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <array>
#include <initializer_list>
#include <utility>

namespace duet::gui
{
namespace
{
    constexpr int windowWidth = 420;
    constexpr int windowHeight = 330;
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

    /** The Interface tab. */
    class InterfaceTab final : public juce::Component,
                               private Appearance::Listener,
                               private juce::FilenameComponentListener
    {
    public:
        InterfaceTab (Appearance& lookAndScale,
                      Settings& store,
                      const std::filesystem::path& defaultProjectsDirectory,
                      std::function<void (bool)> renderingChanged)
            : appearance (lookAndScale), settings (store),
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
                                                                         &renderingLabel,
                                                                         &themeBox,
                                                                         &scaleSlider,
                                                                         &projectsDirectory,
                                                                         &autosaveBox,
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
            layOutRow (renderingLabel, renderingButton);
        }

    private:
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
        juce::ToggleButton renderingButton;
    };

    /** The window's content: the tabs, and the appearance they are measured in. */
    class SettingsContent final : public juce::Component, private Appearance::Listener
    {
    public:
        SettingsContent (Appearance& lookAndScale,
                         Settings& store,
                         const std::filesystem::path& defaultProjectsDirectory,
                         std::function<void (bool)> renderingChanged)
            : appearance (lookAndScale)
        {
            tabs.addTab (
                "Interface",
                juce::Colours::transparentBlack,
                new InterfaceTab {
                    appearance, store, defaultProjectsDirectory, std::move (renderingChanged) },
                true);

            addAndMakeVisible (tabs);
            appearance.addListener (this);
            setSize (appearance.scaled (windowWidth), appearance.scaled (windowHeight));
        }

        ~SettingsContent() override { appearance.removeListener (this); }

        SettingsContent (const SettingsContent&) = delete;
        SettingsContent& operator= (const SettingsContent&) = delete;

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

            // A same-bounds setSize skips resized(), and the tab bar's depth is
            // measured in logical units too (prototype finding, r4m858).
            resized();
            repaint();
        }

        Appearance& appearance;
        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    };
} // namespace

SettingsWindow::SettingsWindow (Appearance& lookAndScale,
                                Settings& store,
                                const std::filesystem::path& defaultProjectsDirectory,
                                std::function<void (bool)> renderingChanged,
                                std::function<void()> onClose)
    : DocumentWindow ("Settings",
                      juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (
                          juce::ResizableWindow::backgroundColourId),
                      closeButton),
      closed (std::move (onClose))
{
    setUsingNativeTitleBar (true);
    setContentOwned (
        new SettingsContent {
            lookAndScale, store, defaultProjectsDirectory, std::move (renderingChanged) },
        true);
    setResizable (true, false);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

SettingsWindow::~SettingsWindow() = default;

void SettingsWindow::closeButtonPressed()
{
    if (closed)
        closed();
}
} // namespace duet::gui
