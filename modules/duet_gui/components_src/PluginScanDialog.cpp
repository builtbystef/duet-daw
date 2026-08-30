#include <duet/gui/PluginScanDialog.h>

#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace duet::gui
{
namespace
{
    constexpr int windowWidth = 460;
    constexpr int windowHeight = 360;

    /** How often the window scans one more plugin.

        A step is one plugin asked what it is, in a process of its own, and the
        message loop turns between two of them — which is what makes this a scan
        the producer watches rather than a window that has stopped answering.
    */
    constexpr int stepIntervalMs = 20;
} // namespace

//==============================================================================
/** The scan, stepped, and what it found. */
class PluginScanPanel::Body final : public juce::Component,
                                    private juce::Timer,
                                    private Appearance::Listener,
                                    private juce::ListBoxModel
{
public:
    Body (Appearance& lookAndScale, PluginScan& scanning)
        : appearance (lookAndScale), model (scanning)
    {
        title.setText ("VST3 plugins", juce::dontSendNotification);
        status.setText ("Ready to scan", juce::dontSendNotification);

        results.setModel (this);
        results.setRowHeight (appearance.scaled (metrics::rowHeight));

        scanButton.setButtonText ("Scan");
        scanButton.onClick = [this] { beginScan(); };

        stopButton.setButtonText ("Stop");
        stopButton.setEnabled (false);
        stopButton.onClick = [this] { stopScan(); };

        progress.setPercentageDisplay (true);

        for (auto* child : std::initializer_list<juce::Component*> {
                 &title, &status, &progress, &results, &scanButton, &stopButton })
            addAndMakeVisible (*child);

        appearance.addListener (this);
        setWantsKeyboardFocus (true);
        setSize (appearance.scaled (windowWidth), appearance.scaled (windowHeight));
        refreshRows();
    }

    ~Body() override
    {
        stopTimer();
        appearance.removeListener (this);
    }

    Body (const Body&) = delete;
    Body& operator= (const Body&) = delete;

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
        status.setBounds (area.removeFromTop (rowHeight));
        area.removeFromTop (gap);
        progress.setBounds (area.removeFromTop (rowHeight));
        area.removeFromTop (gap);

        auto buttons = area.removeFromBottom (rowHeight);
        scanButton.setBounds (buttons.removeFromRight (buttons.getWidth() / 3));
        buttons.removeFromRight (gap);
        stopButton.setBounds (buttons.removeFromRight (buttons.getWidth() / 2));
        area.removeFromBottom (gap);

        results.setRowHeight (rowHeight);
        results.setBounds (area);
    }

    /** One line of the results: a plugin that was found, or a module that was
        not taken.
    */
    struct Row
    {
        std::string text;
        bool rejected = false;
    };

    void beginScan()
    {
        if (! model.start())
        {
            status.setText ("Nowhere to scan", juce::dontSendNotification);

            return;
        }

        scanButton.setEnabled (false);
        stopButton.setEnabled (true);
        startTimer (stepIntervalMs);
        showWhereItIs();
    }

    /** One step, and whether the scan goes on: what the timer does, and what a
        case that is driving the scan itself does.
    */
    bool stepScan()
    {
        if (model.step())
        {
            showWhereItIs();

            return true;
        }

        finishedScanning();

        return false;
    }

    void stopScan()
    {
        model.cancel();
        finishedScanning();
    }

    [[nodiscard]] juce::String statusText() const { return status.getText(); }

    [[nodiscard]] std::vector<std::string> resultLines() const
    {
        std::vector<std::string> lines;
        lines.reserve (rows.size());

        for (const auto& row : rows)
            lines.push_back (row.text);

        return lines;
    }

private:
    void timerCallback() override { static_cast<void> (stepScan()); }

    void showWhereItIs()
    {
        howFar = model.progress();
        progress.repaint();

        const auto plugin = model.scanningNow();

        status.setText (plugin.empty() ? juce::String { "Scanning..." }
                                       : "Scanning " + juce::String { plugin.filename().string() },
                        juce::dontSendNotification);
    }

    void finishedScanning()
    {
        stopTimer();
        howFar = model.progress();
        progress.repaint();
        scanButton.setEnabled (true);
        stopButton.setEnabled (false);
        refreshRows();

        status.setText (juce::String { static_cast<int> (rows.size() - model.rejected().size()) }
                            + " found, " + juce::String { (int) model.rejected().size() }
                            + " not taken",
                        juce::dontSendNotification);
    }

    void refreshRows()
    {
        rows.clear();

        for (const auto& plugin : model.found())
            rows.push_back ({ plugin.name + "  " + plugin.manufacturer, false });

        // What the scanner would not take, named rather than silently missing:
        // a plugin that crashed it is a line here.
        for (const auto& file : model.rejected())
            rows.push_back ({ file.filename().string() + "  (not taken)", true });

        results.updateContent();
        results.repaint();
    }

    int getNumRows() override { return static_cast<int> (rows.size()); }

    void paintListBoxItem (int rowNumber,
                           juce::Graphics& g,
                           int width,
                           int height,
                           bool rowIsSelected) override
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int> (rows.size()))
            return;

        const auto& row = rows[static_cast<std::size_t> (rowNumber)];

        if (rowIsSelected)
        {
            g.setColour (toJuce (appearance.colour (ColourToken::surfaceInteractive)));
            g.fillRect (0, 0, width, height);
        }

        g.setColour (toJuce (appearance.colour (row.rejected ? ColourToken::semanticWarning
                                                             : ColourToken::textPrimary)));
        g.setFont (interFont (appearance.scaled (typography::body)));
        g.drawText (juce::String { row.text },
                    juce::Rectangle<int> { 0, 0, width, height },
                    juce::Justification::centredLeft,
                    true);
    }

    void appearanceChanged() override
    {
        sendLookAndFeelChange();
        setSize (appearance.scaled (windowWidth), appearance.scaled (windowHeight));

        // The panel is what the window is sized to, so the new measurement has
        // to reach it (the same finding as the Settings window's).
        if (auto* panel = getParentComponent())
            panel->setSize (getWidth(), getHeight());

        resized();
        repaint();
    }

    Appearance& appearance;
    PluginScan& model;

    juce::Label title;
    juce::Label status;
    double howFar = 0.0;
    juce::ProgressBar progress { howFar };
    juce::ListBox results;
    juce::TextButton scanButton;
    juce::TextButton stopButton;
    std::vector<Row> rows;
};

//==============================================================================
PluginScanPanel::PluginScanPanel (Appearance& lookAndScale,
                                  PluginScan& scanning,
                                  std::function<void()> onClose)
    : body (std::make_unique<Body> (lookAndScale, scanning)), dismiss (std::move (onClose))
{
    addAndMakeVisible (*body);
    setWantsKeyboardFocus (true);
    setSize (body->getWidth(), body->getHeight());
}

PluginScanPanel::~PluginScanPanel() = default;

void PluginScanPanel::resized() { body->setBounds (getLocalBounds()); }

bool PluginScanPanel::keyPressed (const juce::KeyPress& key)
{
    if (key != juce::KeyPress::escapeKey)
        return false;

    // Nothing steps the scan once this has gone, so it stops here rather than
    // being left half-walked.
    body->stopScan();

    if (dismiss)
        dismiss();

    return true;
}

void PluginScanPanel::beginScan() { body->beginScan(); }

bool PluginScanPanel::stepScan() { return body->stepScan(); }

juce::String PluginScanPanel::statusText() const { return body->statusText(); }

std::vector<std::string> PluginScanPanel::resultLines() const { return body->resultLines(); }

//==============================================================================
PluginScanDialog::PluginScanDialog (Appearance& lookAndScale,
                                    PluginScan& scanning,
                                    std::function<void()> onClose)
    : DocumentWindow ("Plugin Scan",
                      juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (
                          juce::ResizableWindow::backgroundColourId),
                      closeButton),
      closed (std::move (onClose))
{
    setUsingNativeTitleBar (true);

    auto panel = std::make_unique<PluginScanPanel> (
        lookAndScale, scanning, [this] { closeButtonPressed(); });
    setContentOwned (panel.release(), true);
    setResizable (true, false);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);

    if (auto* content = getContentComponent())
        content->grabKeyboardFocus();
}

PluginScanDialog::~PluginScanDialog() = default;

void PluginScanDialog::closeButtonPressed()
{
    if (closed)
        closed();
}
} // namespace duet::gui
