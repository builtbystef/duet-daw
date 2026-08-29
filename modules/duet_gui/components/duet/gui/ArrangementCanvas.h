#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/ArrangementView.h>

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
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
    /** @param askTheCollaborator  what the Collaborator's own menu entry does
                                    once this surface has said what the ask is
                                    about: the shell opens the panel and puts
                                    the keyboard in the composer, neither of
                                    which is this surface's to do.
    */
    ArrangementCanvas (Appearance& lookAndScale,
                       ArrangementView& arrangement,
                       std::function<void (duet::model::ClipRef)> showPianoRoll = {},
                       std::function<void()> askTheCollaborator = {});

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
    void perform (Command command);

    [[nodiscard]] ArrangementView& model() noexcept { return view; }
    [[nodiscard]] const ArrangementView& model() const noexcept { return view; }

    //==============================================================================
    /** The context menus this surface offers, and what choosing an entry does.

        They are built and answered apart so that what a menu holds is a thing a
        test can read: a popup on screen is JUCE's, and what the entries mean is
        this surface's. Zero is the menu dismissed, and does nothing.
    */
    [[nodiscard]] juce::PopupMenu clipMenu() const;
    void clipMenuChosen (duet::model::ClipRef clip, duet::model::TrackRef track, int result);
    [[nodiscard]] juce::PopupMenu trackMenu() const;
    void trackMenuChosen (duet::model::TrackRef track, int result);
    [[nodiscard]] static juce::PopupMenu emptyTimelineMenu();
    void emptyTimelineMenuChosen (duet::model::TrackRef track, double atBeats, int result);

    /** The Collaborator's entry, on the menus of the things it can be asked
        about: a clip and a track, and nothing else (spec js437t, story 9).
    */
    static constexpr int askCollaboratorId = 7;

    //==============================================================================
    /** The chrome's measurements, in logical units. */
    static constexpr int rulerHeight = 24;
    static constexpr int playheadThickness = 2;
    static constexpr int trackHeaderWidth = 188;
    static constexpr int headerControlWidth = 24;

    /** The triangle at the left of a track header that opens its automation
        area, and how wide a point is drawn in a lane.
    */
    static constexpr int disclosureWidth = 18;
    static constexpr int automationPointSize = 7;

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

    /** Where a pointer landed in a track's automation area: which lane, and how
        far down that area it was, which is the coordinate the lanes count in.
    */
    struct LaneHit
    {
        TrackDrawing track;
        AutomationLaneDrawing lane;
        int xInTimeline = 0;
        int yInArea = 0;
    };

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
    [[nodiscard]] std::optional<LaneHit> laneAt (int x, int y);
    void paintTrackHeader (juce::Graphics& g, const TrackDrawing& track, juce::Rectangle<int> row);
    void paintAutomation (juce::Graphics& g, const TrackDrawing& track, int timelineTop);
    void mouseDownOnLane (const juce::MouseEvent& event, const LaneHit& hit);
    void showLaneMenu (duet::model::TrackRef track, int laneIndex);
    [[nodiscard]] static std::optional<ClipDrawing> clipAt (const TrackDrawing& track, int x);
    [[nodiscard]] ClipGestureKind gestureKindFor (const juce::MouseEvent& event,
                                                  const TrackDrawing& track,
                                                  const ClipDrawing& clip) const;
    void showAddTrackMenu();
    void mouseDownOnTimeline (const juce::MouseEvent& event, const TrackDrawing& track);
    void mouseDownOnTrackHeader (const juce::MouseEvent& event,
                                 const TrackDrawing& track,
                                 int timelineY);
    void beginRename (duet::model::TrackRef track);
    void beginClipRename (duet::model::ClipRef clip);
    void showTrackMenu (duet::model::TrackRef track);
    void showClipMenu (duet::model::ClipRef clip);
    void showEmptyTimelineMenu (duet::model::TrackRef track, double atBeats);
    [[nodiscard]] static juce::PopupMenu colourMenu();
    void commitRename();

    Appearance& appearance;
    ArrangementView& view;
    std::function<void()> askCollaborator;
    std::unique_ptr<Ruler> ruler;
    juce::AudioFormatManager audioFormats;
    juce::AudioThumbnailCache thumbnailCache { 32 };
    std::unordered_map<duet::model::ClipRef, std::unique_ptr<juce::AudioThumbnail>> thumbnails;
    std::unique_ptr<juce::TextEditor> nameEditor;
    duet::model::TrackRef editingTrack = duet::model::noTrack;
    duet::model::ClipRef editingClip = duet::model::noClip;
    duet::model::TrackRef draggedTrack = duet::model::noTrack;
    duet::model::TrackRef resizingTrack = duet::model::noTrack;
    bool pointDragged = false;

    /** Where the automation area of the track being dragged in starts, in this
        component's coordinates: a drag that wanders out of its lane is still a
        drag in that lane, and the height it reads has to be measured from the
        same place throughout.
    */
    int pointGestureAreaTop = 0;
    int dragStartY = 0;
    int resizeStartHeight = 0;
    juce::Point<int> rubberStart;
    juce::Rectangle<int> rubberBand;
    bool rubberBanding = false;
    bool clipDragged = false;
    int playheadX = 0;
    std::function<void (duet::model::ClipRef)> openPianoRoll;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementCanvas)
};
} // namespace duet::gui
