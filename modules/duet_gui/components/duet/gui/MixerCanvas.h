#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/Browser.h>
#include <duet/gui/Mixer.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace duet::gui
{
/** The thin JUCE surface over Mixer. It paints cached meter scalars only; its
    30 Hz message-thread timer is the sole caller of the model's published meter
    reads and limits those reads to visible strips.
*/
class MixerCanvas final : public juce::Component,
                          public juce::DragAndDropTarget,
                          private juce::Timer
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

    //==============================================================================
    /** The left dock a drop onto this surface comes out of, or nothing for a
        surface nothing can be dropped on. It is read and never owned.
    */
    void setBrowser (Browser* dock) { browser = dock; }

    /** What a drag out of the browser does when it lands on a strip: the device
        goes into that strip's insert chain at the place it was dropped, and a
        drop between two plugins means between those two.
    */
    [[nodiscard]] bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;

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

    /** What a pending Suggestion puts on one strip: the level it proposes, as a
        line down the fader's row at the place it would sit, and the A/B chip
        while it is heard.
    */
    void paintSuggestion (juce::Graphics& g,
                          duet::model::TrackRef channel,
                          juce::Rectangle<int> track,
                          juce::Rectangle<int> row,
                          juce::Rectangle<int> chip);

    Appearance& appearance;
    Mixer& mixer;
    Browser* browser = nullptr;
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
