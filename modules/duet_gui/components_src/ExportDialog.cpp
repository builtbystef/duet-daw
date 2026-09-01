#include <duet/gui/ExportDialog.h>

#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

namespace duet::gui
{
namespace
{
    constexpr int windowWidth = 440;
    constexpr int windowHeight = 330;
    constexpr int labelWidth = 120;

    /** How often the window looks at a render running somewhere else. Fast
        enough that a progress bar moves, and slow enough that watching one costs
        nothing.
    */
    constexpr int progressRefreshMs = 50;

    /** The formats the dialog offers, in the order the box lists them. The ids
        are the positions in this list, JUCE reserving zero for "nothing
        chosen".
    */
    constexpr std::array<std::pair<duet::model::ExportFormat, const char*>, 3> formatChoices {
        { { duet::model::ExportFormat::wav, "WAV" },
          { duet::model::ExportFormat::aiff, "AIFF" },
          { duet::model::ExportFormat::flac, "FLAC" } }
    };

    int idFor (duet::model::ExportFormat format)
    {
        for (std::size_t index = 0; index < formatChoices.size(); ++index)
            if (formatChoices.at (index).first == format)
                return static_cast<int> (index) + 1;

        return 1;
    }

    juce::String rateText (double rate) { return juce::String { static_cast<int> (rate) } + " Hz"; }
} // namespace

//==============================================================================
/** The rows, and the render they start. */
class ExportPanel::Rows final : public juce::Component,
                                private juce::Timer,
                                private Appearance::Listener,
                                private juce::FilenameComponentListener
{
public:
    Rows (Appearance& lookAndScale, Export& exporting)
        : appearance (lookAndScale), model (exporting),
          destination ("destination",
                       juce::File { model.destinationFolder().string() },
                       true,
                       true,
                       true,
                       {},
                       {},
                       "Choose where the file goes")
    {
        nameLabel.setText ("Name", juce::dontSendNotification);
        destinationLabel.setText ("Destination", juce::dontSendNotification);
        formatLabel.setText ("Format", juce::dontSendNotification);
        depthLabel.setText ("Bit depth", juce::dontSendNotification);
        rateLabel.setText ("Sample rate", juce::dontSendNotification);
        rangeLabel.setText ("Bars", juce::dontSendNotification);
        normaliseLabel.setText ("Normalise", juce::dontSendNotification);

        fileName.setText (model.name(), false);
        fileName.onTextChange = [this] { model.setName (fileName.getText().toStdString()); };

        destination.addListener (this);

        for (std::size_t index = 0; index < formatChoices.size(); ++index)
            formatBox.addItem (formatChoices.at (index).second, static_cast<int> (index) + 1);

        formatBox.setSelectedId (idFor (model.format()), juce::dontSendNotification);
        formatBox.onChange = [this]
        {
            const auto chosen = static_cast<std::size_t> (formatBox.getSelectedId() - 1);

            if (chosen < formatChoices.size())
                model.setFormat (formatChoices.at (chosen).first);

            // The depth a format cannot write is not offered by it, and the
            // view-model has already moved the producer's choice to one it can.
            refreshDepths();
        };

        for (const auto rate : Export::availableSampleRates())
            rateBox.addItem (rateText (rate), static_cast<int> (rate));

        rateBox.setSelectedId (static_cast<int> (model.sampleRate()), juce::dontSendNotification);
        rateBox.onChange = [this]
        { model.setSampleRate (static_cast<double> (rateBox.getSelectedId())); };

        depthBox.onChange = [this] { model.setBitDepth (depthBox.getSelectedId()); };
        refreshDepths();

        firstBar.setText (juce::String { model.firstBar() }, false);
        lastBar.setText (juce::String { model.lastBar() }, false);
        toLabel.setText ("to", juce::dontSendNotification);
        toLabel.setJustificationType (juce::Justification::centred);

        for (auto* bars : std::initializer_list<juce::TextEditor*> { &firstBar, &lastBar })
        {
            bars->setInputRestrictions (5, "0123456789");
            bars->onTextChange = [this] { rangeTyped(); };
        }

        normaliseButton.setButtonText ("To " + juce::String { duet::model::exportNormaliseTargetDb }
                                       + " dB");
        normaliseButton.setToggleState (model.normalise(), juce::dontSendNotification);
        normaliseButton.onClick = [this] { model.setNormalise (normaliseButton.getToggleState()); };

        progress.setPercentageDisplay (false);
        status.setText ("", juce::dontSendNotification);

        exportButton.setButtonText ("Export");
        exportButton.onClick = [this] { beginExport(); };

        cancelButton.setButtonText ("Cancel");
        cancelButton.onClick = [this] { model.cancel(); };
        cancelButton.setEnabled (false);

        for (auto* child : std::initializer_list<juce::Component*> {
                 &nameLabel,       &destinationLabel, &formatLabel, &depthLabel,   &rateLabel,
                 &rangeLabel,      &normaliseLabel,   &fileName,    &destination,  &formatBox,
                 &depthBox,        &rateBox,          &firstBar,    &toLabel,      &lastBar,
                 &normaliseButton, &progress,         &status,      &exportButton, &cancelButton })
            addAndMakeVisible (*child);

        // The bar means an export is running; an idle dialog has no use for an
        // empty one.
        progress.setVisible (false);

        appearance.addListener (this);
        setWantsKeyboardFocus (true);
        setSize (appearance.scaled (windowWidth), appearance.scaled (windowHeight));
    }

    ~Rows() override
    {
        stopTimer();
        destination.removeListener (this);
        appearance.removeListener (this);
    }

    Rows (const Rows&) = delete;
    Rows& operator= (const Rows&) = delete;

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

        layOutRow (nameLabel, fileName);
        layOutRow (destinationLabel, destination);
        layOutRow (formatLabel, formatBox);
        layOutRow (depthLabel, depthBox);
        layOutRow (rateLabel, rateBox);

        auto bars = area.removeFromTop (rowHeight);
        rangeLabel.setBounds (bars.removeFromLeft (labels));
        const auto barWidth = bars.getWidth() / 3;
        firstBar.setBounds (bars.removeFromLeft (barWidth));
        toLabel.setBounds (bars.removeFromLeft (barWidth / 2));
        lastBar.setBounds (bars.removeFromLeft (barWidth));
        area.removeFromTop (gap);

        layOutRow (normaliseLabel, normaliseButton);

        auto buttons = area.removeFromBottom (rowHeight);
        exportButton.setBounds (buttons.removeFromRight (appearance.scaled (labelWidth)));
        buttons.removeFromRight (gap);
        cancelButton.setBounds (buttons.removeFromRight (appearance.scaled (labelWidth)));
        area.removeFromBottom (gap);

        status.setBounds (area.removeFromBottom (rowHeight));
        progress.setBounds (area.removeFromBottom (rowHeight));
    }

    void beginExport()
    {
        if (! model.start())
        {
            status.setText ("There is nothing to export", juce::dontSendNotification);

            return;
        }

        howFar = 0.0;
        exportButton.setEnabled (false);
        cancelButton.setEnabled (true);
        progress.setVisible (true);
        status.setText ("Exporting...", juce::dontSendNotification);
        startTimer (progressRefreshMs);
    }

    void cancelExport() { model.cancel(); }

    [[nodiscard]] juce::String statusText() const { return status.getText(); }

private:
    void timerCallback() override
    {
        howFar = model.progress();
        progress.repaint();

        if (model.isRunning())
            return;

        stopTimer();
        exportButton.setEnabled (true);
        cancelButton.setEnabled (false);
        progress.setVisible (false);

        switch (model.state())
        {
            case ExportState::written:
                status.setText ("Wrote " + juce::String { model.writtenFile().string() },
                                juce::dontSendNotification);
                break;

            case ExportState::cancelled:
                status.setText ("Cancelled", juce::dontSendNotification);
                break;

            case ExportState::failed:
                status.setText ("Nothing could be written", juce::dontSendNotification);
                break;

            case ExportState::idle:
            case ExportState::running:
                break;
        }
    }

    void refreshDepths()
    {
        depthBox.clear (juce::dontSendNotification);

        for (const auto depth : model.availableBitDepths())
            depthBox.addItem (juce::String { depth } + " bit", depth);

        depthBox.setSelectedId (model.bitDepth(), juce::dontSendNotification);
    }

    void rangeTyped()
    {
        model.setRange (firstBar.getText().getIntValue(), lastBar.getText().getIntValue());
    }

    void filenameComponentChanged (juce::FilenameComponent* component) override
    {
        if (component == &destination)
            model.setDestinationFolder (
                component->getCurrentFile().getFullPathName().toStdString());
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
    Export& model;

    juce::Label nameLabel;
    juce::Label destinationLabel;
    juce::Label formatLabel;
    juce::Label depthLabel;
    juce::Label rateLabel;
    juce::Label rangeLabel;
    juce::Label normaliseLabel;
    juce::Label toLabel;
    juce::Label status;

    juce::TextEditor fileName;
    juce::FilenameComponent destination;
    juce::ComboBox formatBox;
    juce::ComboBox depthBox;
    juce::ComboBox rateBox;
    juce::TextEditor firstBar;
    juce::TextEditor lastBar;
    juce::ToggleButton normaliseButton;

    double howFar = 0.0;
    juce::ProgressBar progress { howFar };

    juce::TextButton exportButton;
    juce::TextButton cancelButton;
};

//==============================================================================
ExportPanel::ExportPanel (Appearance& lookAndScale,
                          Export& exporting,
                          std::function<void()> onClose)
    : rows (std::make_unique<Rows> (lookAndScale, exporting)), dismiss (std::move (onClose))
{
    addAndMakeVisible (*rows);
    setWantsKeyboardFocus (true);
    setSize (rows->getWidth(), rows->getHeight());
}

ExportPanel::~ExportPanel() = default;

void ExportPanel::resized() { rows->setBounds (getLocalBounds()); }

bool ExportPanel::keyPressed (const juce::KeyPress& key)
{
    if (key != juce::KeyPress::escapeKey)
        return false;

    // Dismissing the dialog is abandoning the render it was watching, and a
    // cancelled export leaves nothing behind.
    rows->cancelExport();

    if (dismiss)
        dismiss();

    return true;
}

void ExportPanel::beginExport() { rows->beginExport(); }

juce::String ExportPanel::statusText() const { return rows->statusText(); }

//==============================================================================
ExportDialog::ExportDialog (Appearance& lookAndScale,
                            Export& exporting,
                            std::function<void()> onClose)
    : DocumentWindow ("Export / Bounce",
                      juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (
                          juce::ResizableWindow::backgroundColourId),
                      closeButton),
      closed (std::move (onClose))
{
    setUsingNativeTitleBar (true);

    auto panel =
        std::make_unique<ExportPanel> (lookAndScale, exporting, [this] { closeButtonPressed(); });
    setContentOwned (panel.release(), true);
    setResizable (true, false);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);

    if (auto* content = getContentComponent())
        content->grabKeyboardFocus();
}

ExportDialog::~ExportDialog() = default;

void ExportDialog::closeButtonPressed()
{
    if (closed)
        closed();
}
} // namespace duet::gui
