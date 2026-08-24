#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/ArrangementView.h>

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <optional>
#include <unordered_map>

namespace duet::gui
{
/** The arrangement, on screen: the ruler along the top and the timeline under
    it, with the playhead over both.

    The thin half of the surface. It paints what the arrangement view-model says
    and hands it what the producer does, and it decides nothing of its own: where
    a beat is, which lines the zoom has room for, and what a wheel gesture means
    are all the view-model's (spec 535bbo).

    The playhead repaints from what the engine has already published, on a timer
    and never from a lock: the transport is asked where it is, and asking is a
    read of a value the audio thread put there.
*/
class ArrangementCanvas final : public juce::Component,
                                private juce::Timer,
                                private juce::ChangeListener
{
public:
    ArrangementCanvas (Appearance& lookAndScale, ArrangementView& arrangement);

    ~ArrangementCanvas() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

    //==============================================================================
    /** The chrome's measurements, in logical units. */
    static constexpr int rulerHeight = 24;
    static constexpr int playheadThickness = 2;
    static constexpr int trackHeaderWidth = 188;
    static constexpr int headerControlWidth = 24;

    /** How tall a bar tick, a beat tick and a fine tick are drawn in the ruler,
        as a fraction of its height.
    */
    static constexpr float barTickHeight = 1.0F;
    static constexpr float beatTickHeight = 0.45F;
    static constexpr float fineTickHeight = 0.25F;

    /** How often the playhead is asked where it is while the transport rolls. */
    static constexpr int playheadRefreshHz = 30;

private:
    class Ruler;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    /** Repaints the column the playhead was in and the one it is in now, and
        nothing else: the arrangement is a large surface and the playhead is a
        line two pixels wide.
    */
    void movePlayheadTo (int x);

    [[nodiscard]] juce::Rectangle<int> timelineArea() const;
    [[nodiscard]] juce::Rectangle<int> playheadColumn (int x) const;
    [[nodiscard]] std::optional<TrackDrawing> trackAt (int y);
    void beginRename (duet::model::TrackRef track);
    void showTrackMenu (duet::model::TrackRef track);
    void commitRename();

    Appearance& appearance;
    ArrangementView& view;
    std::unique_ptr<Ruler> ruler;
    juce::AudioFormatManager audioFormats;
    juce::AudioThumbnailCache thumbnailCache { 32 };
    std::unordered_map<duet::model::ClipRef, std::unique_ptr<juce::AudioThumbnail>> thumbnails;
    std::unique_ptr<juce::TextEditor> nameEditor;
    duet::model::TrackRef editingTrack = duet::model::noTrack;
    duet::model::TrackRef draggedTrack = duet::model::noTrack;
    duet::model::TrackRef resizingTrack = duet::model::noTrack;
    int dragStartY = 0;
    int resizeStartHeight = 0;
    int playheadX = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementCanvas)
};
} // namespace duet::gui
