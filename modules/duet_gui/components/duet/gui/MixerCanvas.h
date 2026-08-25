#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/Mixer.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace duet::gui
{
/** The thin JUCE surface over Mixer. It paints cached meter scalars only; its
    30 Hz message-thread timer is the sole caller of the model's published meter
    reads and limits those reads to visible strips.
*/
class MixerCanvas final : public juce::Component, private juce::Timer
{
public:
    MixerCanvas (Appearance& lookAndScale,
                 Mixer& mixerModel,
                 std::function<void (duet::model::PluginRef)> openPluginEditor,
                 std::function<void()> mixerChanged);
    ~MixerCanvas() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

    [[nodiscard]] Mixer& model() noexcept { return mixer; }
    [[nodiscard]] const Mixer& model() const noexcept { return mixer; }

private:
    enum class Drag : std::uint8_t
    {
        none,
        fader,
        pan,
        plugin
    };

    void timerCallback() override;
    [[nodiscard]] int stripWidthPx() const;
    [[nodiscard]] int stripIndexAt (juce::Point<int> position) const;
    [[nodiscard]] juce::Rectangle<int> stripBounds (int index) const;
    void showRoutingMenu (duet::model::TrackRef channel, juce::Component* target);
    void showInsertMenu (const MixerStrip& strip, juce::Component* target);

    Appearance& appearance;
    Mixer& mixer;
    std::function<void (duet::model::PluginRef)> openEditor;
    std::function<void()> modelChanged;
    Drag drag = Drag::none;
    duet::model::TrackRef draggedChannel = duet::model::noTrack;
    duet::model::PluginRef draggedPlugin = duet::model::noPlugin;
    int pluginDropPosition = -1;
    int horizontalOffsetPx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerCanvas)
};
} // namespace duet::gui
