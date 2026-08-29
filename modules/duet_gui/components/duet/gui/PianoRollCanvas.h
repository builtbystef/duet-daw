#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/PianoRoll.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace duet::gui
{
/** The thin JUCE surface that paints and forwards the Piano Roll. */
class PianoRollCanvas final : public juce::Component, private juce::Timer
{
public:
    PianoRollCanvas (Appearance& lookAndScale, PianoRoll& pianoRoll);
    ~PianoRollCanvas() override = default;

    [[nodiscard]] duet::model::ClipRef openClipRef() const { return view.openClipRef(); }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;

    /** What a note's context menu holds. Built apart from the popup it is shown
        in, so that what it offers is a thing a test can read.
    */
    [[nodiscard]] static juce::PopupMenu noteMenu();

private:
    void timerCallback() override;
    [[nodiscard]] std::optional<PianoNoteDrawing> noteAt (juce::Point<int> point) const;
    [[nodiscard]] int pitchAt (int y) const;
    [[nodiscard]] juce::Rectangle<int> gridArea() const;
    [[nodiscard]] juce::Rectangle<int> velocityArea() const;
    void showNoteMenu();

    static constexpr int controlsHeight = 28;
    static constexpr int keyboardWidth = 72;
    static constexpr int velocityHeight = 72;

    Appearance& appearance;
    PianoRoll& view;
    duet::model::NoteRef dragged = duet::model::noNote;
    duet::model::NoteRef velocityDragged = duet::model::noNote;
    int velocityTarget = PianoRoll::defaultVelocity;
    juce::ComboBox root;
    juce::ComboBox scale;
    juce::ComboBox noteLength;
    juce::ToggleButton fold { "Fold" };
    juce::Point<int> rubberStart;
    juce::Rectangle<int> rubberBand;
    bool rubberBanding = false;
    bool rubberExtends = false;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollCanvas)
};
} // namespace duet::gui
