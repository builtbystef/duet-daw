#include <duet/model/Session.h>

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

namespace duet::app
{
namespace
{
    constexpr int windowWidth = 620;
    constexpr int windowHeight = 180;
    constexpr int statusRefreshMs = 100;
} // namespace

/** The shell's only surface until the producer-facing interface (issue xxv9ng)
    lands: it starts and stops the transport, and reports what the audio device
    and the transport are doing.
*/
class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent()
    {
        session.loadDemoContent();

        playButton.onClick = [this] { session.startPlayback(); };
        stopButton.onClick = [this] { session.stopPlayback(); };

        for (auto* button : { &playButton, &stopButton })
            addAndMakeVisible (*button);

        deviceLabel.setText (deviceText(), juce::dontSendNotification);
        addAndMakeVisible (deviceLabel);
        addAndMakeVisible (transportLabel);

        setSize (windowWidth, windowHeight);
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

        auto buttons = area.removeFromTop (40);
        playButton.setBounds (buttons.removeFromLeft (120).reduced (2));
        stopButton.setBounds (buttons.removeFromLeft (120).reduced (2));

        deviceLabel.setBounds (area.removeFromTop (30));
        transportLabel.setBounds (area.removeFromTop (30));
    }

private:
    void timerCallback() override
    {
        transportLabel.setText (transportText(), juce::dontSendNotification);
    }

    [[nodiscard]] juce::String deviceText() const
    {
        const auto device = session.audioDeviceDescription();

        return device.empty() ? juce::String ("No audio device")
                              : juce::String ("Device: ") + juce::String (device);
    }

    [[nodiscard]] juce::String transportText() const
    {
        return juce::String (session.isPlaying() ? "Playing" : "Stopped")
               + juce::String::formatted (" — %.2f s", session.playbackPositionSeconds());
    }

    duet::model::Session session;

    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
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
        setContentOwned (new MainComponent(), true);
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
