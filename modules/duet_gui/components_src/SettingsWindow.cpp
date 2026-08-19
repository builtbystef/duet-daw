#include <duet/gui/SettingsWindow.h>

#include <duet/gui/GraphiteLookAndFeel.h>
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
    constexpr int windowHeight = 220;
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

    int idFor (ThemePreference preference)
    {
        for (std::size_t index = 0; index < themeChoices.size(); ++index)
            if (themeChoices.at (index).first == preference)
                return static_cast<int> (index) + 1;

        return 1;
    }

    /** The Interface tab: the two rows this slice ships. */
    class InterfaceTab final : public juce::Component, private Appearance::Listener
    {
    public:
        explicit InterfaceTab (Appearance& lookAndScale) : appearance (lookAndScale)
        {
            themeLabel.setText ("Theme", juce::dontSendNotification);
            scaleLabel.setText ("Interface scale", juce::dontSendNotification);

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

            for (auto* child : std::initializer_list<juce::Component*> {
                     &themeLabel, &scaleLabel, &themeBox, &scaleSlider })
                addAndMakeVisible (*child);

            appearance.addListener (this);
        }

        ~InterfaceTab() override { appearance.removeListener (this); }

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
        }

    private:
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
        juce::Label themeLabel;
        juce::Label scaleLabel;
        juce::ComboBox themeBox;
        juce::Slider scaleSlider;
    };

    /** The window's content: the tabs, and the appearance they are measured in. */
    class SettingsContent final : public juce::Component, private Appearance::Listener
    {
    public:
        explicit SettingsContent (Appearance& lookAndScale) : appearance (lookAndScale)
        {
            tabs.addTab ("Interface",
                         juce::Colours::transparentBlack,
                         new InterfaceTab { appearance },
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

SettingsWindow::SettingsWindow (Appearance& lookAndScale, std::function<void()> onClose)
    : DocumentWindow ("Settings",
                      juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (
                          juce::ResizableWindow::backgroundColourId),
                      closeButton),
      closed (std::move (onClose))
{
    setUsingNativeTitleBar (true);
    setContentOwned (new SettingsContent { lookAndScale }, true);
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
