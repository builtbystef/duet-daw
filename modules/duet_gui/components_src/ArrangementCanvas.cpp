#include <duet/gui/ArrangementCanvas.h>

#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/MainShell.h>
#include <duet/gui/TimelineGeometry.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace duet::gui
{
namespace
{
    /** One notch of a wheel, in the units JUCE reports a wheel in.

        JUCE hands on a fraction rather than a count of notches, and X11's notch
        is 50/256 of it. The view-model's conventions are written in notches,
        which is what a producer turns, so the conversion happens here — at the
        edge, with the platform's number, and once.
    */
    constexpr double wheelNotch = 50.0 / 256.0;

    /** The token a line of that weight is drawn in. */
    [[nodiscard]] ColourToken tokenFor (GridWeight weight)
    {
        switch (weight)
        {
            case GridWeight::bar:
                return ColourToken::gridBar;
            case GridWeight::beat:
                return ColourToken::gridBeat;
            case GridWeight::fine:
            default:
                return ColourToken::gridFine;
        }
    }

    [[nodiscard]] juce::String panLabel (double pan)
    {
        if (pan < -0.25)
            return "L";
        if (pan > 0.25)
            return "R";
        return "C";
    }

    /** How tall a tick of that weight is drawn in the ruler. */
    [[nodiscard]] float tickHeightFor (GridWeight weight)
    {
        switch (weight)
        {
            case GridWeight::bar:
                return ArrangementCanvas::barTickHeight;
            case GridWeight::beat:
                return ArrangementCanvas::beatTickHeight;
            case GridWeight::fine:
            default:
                return ArrangementCanvas::fineTickHeight;
        }
    }
} // namespace

//==============================================================================
/** The ruler above the arrangement: the grid as ticks, and the bars and beats
    the producer counts in, in tabular numerals so that a number that changes
    does not shift the ones beside it.

    Clicking it moves the playhead, and dragging scrubs. Neither is an Action —
    where the producer is listening from is not a change to the project.
*/
class ArrangementCanvas::Ruler final : public juce::Component
{
public:
    Ruler (Appearance& lookAndScale, ArrangementView& arrangement)
        : appearance (lookAndScale), view (arrangement)
    {
        setComponentID (surfaceId::arrangementRuler);
    }

    ~Ruler() override = default;

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds();

        g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
        g.fillRect (area);

        for (const auto& line : view.geometry().gridLines())
        {
            const auto height = juce::roundToInt (static_cast<float> (area.getHeight())
                                                  * tickHeightFor (line.weight));

            g.setColour (toJuce (appearance.colour (tokenFor (line.weight))));
            g.fillRect (appearance.scaled (ArrangementCanvas::trackHeaderWidth) + line.x,
                        area.getBottom() - height,
                        1,
                        height);
        }

        g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
        g.setFont (readoutFont (appearance.scaled (typography::eyebrow)));

        for (const auto& label : view.geometry().rulerLabels())
            g.drawText (juce::String { label.text },
                        area.withX (appearance.scaled (ArrangementCanvas::trackHeaderWidth)
                                    + label.x + appearance.scaled (labelGap))
                            .withWidth (appearance.scaled (labelWidth)),
                        juce::Justification::centredLeft);

        // The playhead's own head. The canvas draws the line down the timeline
        // and the ruler draws the part over itself, because a child component
        // is painted over whatever its parent drew there.
        g.setColour (toJuce (appearance.colour (ColourToken::accentStrong)));
        g.fillRect (appearance.scaled (ArrangementCanvas::trackHeaderWidth) + view.playheadX(),
                    0,
                    std::max (1, appearance.scaled (ArrangementCanvas::playheadThickness)),
                    area.getHeight());

        g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
        g.fillRect (area.withHeight (1));
        g.fillRect (area.withY (area.getBottom() - 1).withHeight (1));
    }

    void mouseDown (const juce::MouseEvent& event) override { scrubTo (event); }
    void mouseDrag (const juce::MouseEvent& event) override { scrubTo (event); }

    /** A wheel over the ruler is a wheel over the arrangement: the ruler is
        part of the same surface, and JUCE offers a wheel to the component under
        the pointer rather than to the one that scrolls.
    */
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override
    {
        if (auto* canvas = getParentComponent(); canvas != nullptr)
            canvas->mouseWheelMove (event.getEventRelativeTo (canvas), wheel);
    }

private:
    void scrubTo (const juce::MouseEvent& event)
    {
        view.clickRuler (event.getPosition().x
                         - appearance.scaled (ArrangementCanvas::trackHeaderWidth));

        if (auto* canvas = getParentComponent(); canvas != nullptr)
            canvas->repaint();
    }

    /** Where a label sits beside the line it belongs to, and how much room it is
        given to be read in — logical units.
    */
    static constexpr int labelGap = 3;
    static constexpr int labelWidth = 44;

    Appearance& appearance;
    ArrangementView& view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ruler)
};

//==============================================================================
ArrangementCanvas::ArrangementCanvas (Appearance& lookAndScale,
                                      ArrangementView& arrangement,
                                      std::function<void (duet::model::ClipRef)> showPianoRoll)
    : appearance (lookAndScale), view (arrangement), openPianoRoll (std::move (showPianoRoll))
{
    setComponentID (surfaceId::arrangement);
    audioFormats.registerBasicFormats();

    ruler = std::make_unique<Ruler> (appearance, view);
    addAndMakeVisible (*ruler);

    startTimerHz (playheadRefreshHz);
}

ArrangementCanvas::~ArrangementCanvas() = default;

//==============================================================================
void ArrangementCanvas::paintTrackHeader (juce::Graphics& g,
                                          const TrackDrawing& track,
                                          juce::Rectangle<int> row)
{
    const auto headerWidth = appearance.scaled (trackHeaderWidth);
    const auto header = row.withWidth (headerWidth);
    g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
    g.fillRect (header);
    g.setColour (
        toJuce (appearance.colour (trackColourToken (static_cast<std::size_t> (track.colour)))));
    g.fillRect (header.withWidth (appearance.scaled (4)));

    // The triangle that opens the track's automation area: pointing right
    // while it is closed, and down while it is open.
    const auto disclosure = header.withX (header.getX() + appearance.scaled (6))
                                .withWidth (appearance.scaled (disclosureWidth))
                                .withHeight (appearance.scaled (disclosureWidth))
                                .withY (row.getY() + appearance.scaled (6));
    const auto arrow = disclosure.reduced (appearance.scaled (5)).toFloat();
    juce::Path triangle;

    if (track.automationHeight > 0)
    {
        triangle.addTriangle (arrow.getX(),
                              arrow.getY(),
                              arrow.getRight(),
                              arrow.getY(),
                              arrow.getCentreX(),
                              arrow.getBottom());
    }
    else
    {
        triangle.addTriangle (arrow.getX(),
                              arrow.getY(),
                              arrow.getX(),
                              arrow.getBottom(),
                              arrow.getRight(),
                              arrow.getCentreY());
    }

    g.setColour (toJuce (appearance.colour (track.automationHeight > 0 ? ColourToken::textSecondary
                                                                       : ColourToken::textMuted)));
    g.fillPath (triangle);

    g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
    g.setFont (interFont (appearance.scaled (typography::body), true));
    g.drawText (track.name,
                header.reduced (appearance.scaled (12), 0)
                    .withTrimmedLeft (appearance.scaled (disclosureWidth))
                    .withTrimmedRight (appearance.scaled (headerControlWidth * 4)),
                juce::Justification::centredLeft);

    const auto controlWidth = appearance.scaled (headerControlWidth);
    const auto paintControl = [&] (int fromRight, const juce::String& text, bool active)
    {
        const auto control =
            header.withX (header.getRight() - controlWidth * fromRight).withWidth (controlWidth);
        g.setColour (toJuce (
            appearance.colour (active ? ColourToken::accentStrong : ColourToken::textMuted)));
        g.drawText (text, control, juce::Justification::centred);
    };
    paintControl (4, panLabel (track.pan), false);
    paintControl (3, "M", track.muted);
    paintControl (2, "S", track.soloed);
    paintControl (1, "R", track.recordArmed);

    g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
    g.fillRect (row.withY (row.getBottom() - 1).withHeight (1));
    g.fillRect (header.withX (header.getRight() - 1).withWidth (1));
}

void ArrangementCanvas::paint (juce::Graphics& g)
{
    const auto timeline = timelineArea();

    g.setColour (toJuce (appearance.colour (ColourToken::surfaceCanvas)));
    g.fillRect (timeline);

    for (const auto& line : view.geometry().gridLines())
    {
        g.setColour (toJuce (appearance.colour (tokenFor (line.weight))));
        g.fillRect (appearance.scaled (trackHeaderWidth) + line.x,
                    timeline.getY(),
                    1,
                    timeline.getHeight());
    }

    const auto headerWidth = appearance.scaled (trackHeaderWidth);
    const auto timelineTop = timeline.getY();

    for (const auto& track : view.tracks())
    {
        const juce::Rectangle<int> row { 0, timelineTop + track.y, getWidth(), track.height };

        if (! row.intersects (timeline))
            continue;

        paintTrackHeader (g, track, row);

        for (const auto& clip : track.clips)
        {
            const juce::Rectangle<int> clipArea {
                headerWidth + clip.x,
                row.getY() + appearance.scaled (6),
                clip.width,
                std::max (1, row.getHeight() - appearance.scaled (12))
            };
            const auto clipColour = clip.colour.value_or (track.colour);
            const auto fill =
                appearance.colour (trackColourToken (static_cast<std::size_t> (clipColour)));
            g.setColour (toJuce ({ fill.red, fill.green, fill.blue, 190 }));
            g.fillRoundedRectangle (clipArea.toFloat(),
                                    static_cast<float> (appearance.scaled (metrics::radiusSmall)));

            g.saveState();
            g.reduceClipRegion (clipArea);
            g.setColour (toJuce (appearance.colour (ColourToken::onTrack)));

            if (clip.holdsMidi)
            {
                for (const auto& note : clip.notes)
                {
                    const auto noteY =
                        clipArea.getBottom() - 2
                        - (note.pitch * std::max (1, clipArea.getHeight() - 4) / 128);
                    g.fillRect (headerWidth + note.x,
                                noteY,
                                note.width,
                                std::max (1, appearance.scaled (2)));
                }
            }
            else if (! clip.sourceFile.empty())
            {
                auto& thumbnail = thumbnails[clip.clip];

                if (thumbnail == nullptr)
                {
                    thumbnail =
                        std::make_unique<juce::AudioThumbnail> (256, audioFormats, thumbnailCache);
                    thumbnail->addChangeListener (this);
                    thumbnail->setSource (
                        new juce::FileInputSource (juce::File { clip.sourceFile.string() }));
                }

                thumbnail->drawChannels (
                    g, clipArea.reduced (2), 0.0, thumbnail->getTotalLength(), 0.7F);
            }

            g.restoreState();
            if (view.selection().contains ({ SelectionKind::clip, clip.clip }))
            {
                g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
                g.drawRoundedRectangle (
                    clipArea.toFloat(),
                    static_cast<float> (appearance.scaled (metrics::radiusSmall)),
                    2.0F);
            }
            g.drawText (clip.name,
                        clipArea.reduced (appearance.scaled (5), appearance.scaled (2)),
                        juce::Justification::topLeft);
        }

        paintAutomation (g, track, timelineTop);
    }

    const juce::Rectangle<int> addRow {
        0, timelineTop + view.addTrackRowY(), headerWidth, ArrangementView::addTrackRowHeightPx
    };
    g.setColour (toJuce (appearance.colour (ColourToken::surfaceInteractive)));
    g.fillRect (addRow);
    g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
    g.drawText ("+ Add Track", addRow, juce::Justification::centred);

    // The playhead down the timeline; the ruler draws the part over itself.
    g.setColour (toJuce (appearance.colour (ColourToken::accentStrong)));
    g.fillRect (playheadColumn (view.playheadX()));

    if (rubberBanding)
    {
        g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
        g.drawRect (rubberBand, 1);
    }

    g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
    g.drawRect (getLocalBounds(), 1);
}

void ArrangementCanvas::paintAutomation (juce::Graphics& g,
                                         const TrackDrawing& track,
                                         int timelineTop)
{
    if (track.automationHeight <= 0)
        return;

    const auto headerWidth = appearance.scaled (trackHeaderWidth);
    const auto areaTop = timelineTop + track.automationY;
    const auto pointSize = appearance.scaled (automationPointSize);

    for (const auto& lane : view.automation().lanes (track.track))
    {
        const juce::Rectangle<int> laneRow { 0, areaTop + lane.y, getWidth(), lane.height };

        if (! laneRow.intersects (timelineArea()))
            continue;

        const auto laneHeader = laneRow.withWidth (headerWidth);

        g.setColour (toJuce (appearance.colour (ColourToken::surfaceDefault)));
        g.fillRect (laneRow.withTrimmedLeft (headerWidth));
        g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
        g.fillRect (laneHeader);

        // The lane's picker, which is its name: clicking it offers the curves
        // this track has to draw.
        g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
        g.setFont (interFont (appearance.scaled (typography::eyebrow), false));
        g.drawText (
            lane.targetName,
            laneHeader.reduced (appearance.scaled (12 + disclosureWidth), appearance.scaled (2)),
            juce::Justification::centredLeft);

        g.setColour (toJuce (appearance.colour (ColourToken::accentDefault)));

        // The curve: straight segments between the points, and a flat run out
        // to either edge, which is what the engine plays there.
        std::optional<juce::Point<float>> previous;

        for (const auto& point : lane.points)
        {
            const juce::Point<float> here { static_cast<float> (headerWidth + point.x),
                                            static_cast<float> (areaTop + point.y) };

            if (previous.has_value())
                g.drawLine ({ *previous, here }, 1.5F);
            else
                g.drawLine (static_cast<float> (headerWidth), here.y, here.x, here.y, 1.5F);

            previous = here;
        }

        if (previous.has_value())
            g.drawLine (
                previous->x, previous->y, static_cast<float> (getWidth()), previous->y, 1.5F);

        for (const auto& point : lane.points)
            g.fillRect (juce::Rectangle<int> { headerWidth + point.x - pointSize / 2,
                                               areaTop + point.y - pointSize / 2,
                                               pointSize,
                                               pointSize });

        g.setColour (toJuce (appearance.colour (ColourToken::borderSubtle)));
        g.fillRect (laneRow.withY (laneRow.getBottom() - 1).withHeight (1));
        g.fillRect (laneHeader.withX (laneHeader.getRight() - 1).withWidth (1));
    }
}

void ArrangementCanvas::resized()
{
    auto area = getLocalBounds();

    ruler->setBounds (area.removeFromTop (appearance.scaled (rulerHeight)));
    view.setWidthPx (std::max (0, getWidth() - appearance.scaled (trackHeaderWidth)));
    view.setHeightPx (area.getHeight());
}

//==============================================================================
void ArrangementCanvas::mouseWheelMove (const juce::MouseEvent& event,
                                        const juce::MouseWheelDetails& wheel)
{
    ScrollGesture gesture;

    gesture.deltaX = wheel.deltaX / wheelNotch;
    gesture.deltaY = wheel.deltaY / wheelNotch;
    gesture.pointerX = std::max (0, event.getPosition().x - appearance.scaled (trackHeaderWidth));
    gesture.ctrl = event.mods.isCtrlDown();
    gesture.shift = event.mods.isShiftDown();

    if (wheel.isReversed)
    {
        gesture.deltaX = -gesture.deltaX;
        gesture.deltaY = -gesture.deltaY;
    }

    view.scroll (gesture);
    repaint();
}

void ArrangementCanvas::mouseDown (const juce::MouseEvent& event)
{
    const auto timelineY = event.y - timelineArea().getY();

    if (const auto lane = laneAt (event.x, timelineY); lane.has_value())
    {
        mouseDownOnLane (event, *lane);
        repaint();
        return;
    }

    const auto row = trackAt (timelineY);

    if (! row.has_value())
    {
        const auto addTop = view.addTrackRowY();
        if (event.x < appearance.scaled (trackHeaderWidth) && timelineY >= addTop
            && timelineY < addTop + ArrangementView::addTrackRowHeightPx)
            showAddTrackMenu();
        return;
    }

    if (event.x >= appearance.scaled (trackHeaderWidth))
        mouseDownOnTimeline (event, *row);
    else
        mouseDownOnTrackHeader (event, *row, timelineY);
}

void ArrangementCanvas::showAddTrackMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Audio");
    menu.addItem (2, "MIDI");
    menu.showMenuAsync ({},
                        [safe = juce::Component::SafePointer { this }] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            const auto kind = result == 2 ? duet::model::TrackKind::midi
                                                          : duet::model::TrackKind::audio;
                            safe->view.addTrack (kind);
                            safe->repaint();
                        });
}

void ArrangementCanvas::mouseDownOnTimeline (const juce::MouseEvent& event,
                                             const TrackDrawing& track)
{
    view.focusTrack (track.track);
    const auto timelineX = event.x - appearance.scaled (trackHeaderWidth);
    const auto clip = clipAt (track, timelineX);

    if (event.mods.isPopupMenu())
    {
        if (clip.has_value())
        {
            if (! view.selection().contains ({ SelectionKind::clip, clip->clip }))
                view.selection().click (
                    { SelectionKind::clip, clip->clip }, view.allClipItems(), false, false);
            showClipMenu (clip->clip);
        }
        else
            showEmptyTimelineMenu (track.track, view.geometry().xToBeats (timelineX));
        repaint();
        return;
    }

    if (clip.has_value())
    {
        view.selection().click ({ SelectionKind::clip, clip->clip },
                                view.allClipItems(),
                                event.mods.isCtrlDown(),
                                event.mods.isShiftDown());
        view.beginClipGesture (clip->clip, gestureKindFor (event, track, *clip));
        clipDragged = false;
    }
    else
    {
        if (! event.mods.isCtrlDown())
            view.selection().clear();
        rubberStart = event.getPosition();
        rubberBand = { rubberStart.x, rubberStart.y, 0, 0 };
        rubberBanding = true;
    }
    repaint();
}

ClipGestureKind ArrangementCanvas::gestureKindFor (const juce::MouseEvent& event,
                                                   const TrackDrawing& track,
                                                   const ClipDrawing& clip) const
{
    const auto timelineX = event.x - appearance.scaled (trackHeaderWidth);
    const auto fromLeft = timelineX - clip.x;
    const auto fromRight = clip.x + clip.width - timelineX;
    const auto inLoopHandle =
        fromRight <= appearance.scaled (10)
        && event.y - timelineArea().getY() - track.y <= appearance.scaled (16);
    if (inLoopHandle)
        return ClipGestureKind::loop;
    if (fromLeft <= appearance.scaled (6))
        return ClipGestureKind::trimLeft;
    if (fromRight <= appearance.scaled (6))
        return ClipGestureKind::trimRight;
    return ClipGestureKind::move;
}

void ArrangementCanvas::mouseDownOnTrackHeader (const juce::MouseEvent& event,
                                                const TrackDrawing& track,
                                                int timelineY)
{
    if (event.mods.isPopupMenu())
    {
        showTrackMenu (track.track);
        return;
    }

    const auto headerWidth = appearance.scaled (trackHeaderWidth);
    const auto controlWidth = appearance.scaled (headerControlWidth);
    if (event.x < appearance.scaled (6 + disclosureWidth))
        view.automation().toggleExpanded (track.track);
    else if (event.x >= headerWidth - controlWidth)
        view.toggleRecordArm (track.track);
    else if (event.x >= headerWidth - controlWidth * 2)
        view.toggleSolo (track.track);
    else if (event.x >= headerWidth - controlWidth * 3)
        view.toggleMute (track.track);
    else if (event.x >= headerWidth - controlWidth * 4)
        view.cyclePan (track.track);
    else if (std::abs (timelineY - (track.y + track.height)) <= appearance.scaled (5))
    {
        resizingTrack = track.track;
        dragStartY = timelineY;
        resizeStartHeight = track.height;
    }
    else
    {
        draggedTrack = track.track;
        dragStartY = timelineY;
    }

    repaint();
}

void ArrangementCanvas::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto timelineY = event.y - timelineArea().getY();

    if (const auto lane = laneAt (event.x, timelineY); lane.has_value())
    {
        if (event.x >= appearance.scaled (trackHeaderWidth))
        {
            view.automation().addPoint (lane->track.track,
                                        lane->lane.index,
                                        view.geometry().xToBeats (lane->xInTimeline),
                                        lane->yInArea,
                                        event.mods.isAltDown());
            repaint();
        }

        return;
    }

    const auto row = trackAt (timelineY);
    if (! row.has_value())
        return;

    const auto headerWidth = appearance.scaled (trackHeaderWidth);
    if (event.x >= appearance.scaled (6 + disclosureWidth)
        && event.x < appearance.scaled (trackHeaderWidth - headerControlWidth * 4))
    {
        beginRename (row->track);
        return;
    }

    if (event.x < headerWidth)
        return;

    const auto timelineX = event.x - headerWidth;
    if (const auto clip = clipAt (*row, timelineX); clip.has_value())
    {
        if (view.isMidiClip (clip->clip) && openPianoRoll)
            openPianoRoll (clip->clip);
    }
    else if (row->kind == duet::model::TrackKind::midi)
        (void) view.createMidiClip (row->track, view.geometry().xToBeats (timelineX));

    repaint();
}

void ArrangementCanvas::mouseDrag (const juce::MouseEvent& event)
{
    if (view.automation().hasPointGesture())
    {
        pointDragged = event.getDistanceFromDragStart() > appearance.scaled (2);
        view.automation().updatePointGesture (
            view.geometry().xToBeats (event.x - appearance.scaled (trackHeaderWidth)),
            event.y - pointGestureAreaTop,
            event.mods.isAltDown());
        repaint();
        return;
    }

    if (resizingTrack != duet::model::noTrack)
        view.resizeTrack (resizingTrack,
                          resizeStartHeight + event.y - timelineArea().getY() - dragStartY);
    else if (view.hasClipGesture())
    {
        clipDragged = event.getDistanceFromDragStart() > appearance.scaled (4);
        const auto row = trackAt (event.y - timelineArea().getY());
        view.updateClipGesture (
            view.geometry().xToBeats (event.x - appearance.scaled (trackHeaderWidth)),
            row.has_value() ? row->track : duet::model::noTrack,
            event.mods.isAltDown(),
            event.mods.isCtrlDown());
    }
    else if (rubberBanding)
    {
        const auto left = std::min (rubberStart.x, event.x);
        const auto top = std::min (rubberStart.y, event.y);
        rubberBand = {
            left, top, std::abs (event.x - rubberStart.x), std::abs (event.y - rubberStart.y)
        };
    }

    repaint();
}

void ArrangementCanvas::mouseUp (const juce::MouseEvent& event)
{
    if (view.automation().hasPointGesture())
    {
        // A press that never moved is not a drag, and emits no Action.
        if (pointDragged)
            (void) view.automation().completePointGesture();
        else
            view.automation().cancelPointGesture();

        pointDragged = false;
        repaint();
        return;
    }

    if (draggedTrack != duet::model::noTrack
        && std::abs (event.y - timelineArea().getY() - dragStartY) > appearance.scaled (4))
    {
        const auto rows = view.tracks();
        const auto y = event.y - timelineArea().getY();
        auto index = static_cast<int> (rows.size());

        for (std::size_t candidate = 0; candidate < rows.size(); ++candidate)
            if (y < rows[candidate].y + rows[candidate].height / 2)
            {
                index = static_cast<int> (candidate);
                break;
            }

        view.reorderTrack (draggedTrack, index);
    }

    if (view.hasClipGesture())
    {
        if (clipDragged)
        {
            const auto row = trackAt (event.y - timelineArea().getY());
            view.updateClipGesture (
                view.geometry().xToBeats (event.x - appearance.scaled (trackHeaderWidth)),
                row.has_value() ? row->track : duet::model::noTrack,
                event.mods.isAltDown(),
                event.mods.isCtrlDown());
            (void) view.completeClipGesture();
        }
        else
            view.cancelClipGesture();
    }

    if (rubberBanding)
        view.rubberBand ({ rubberBand.getX() - appearance.scaled (trackHeaderWidth),
                           rubberBand.getY() - timelineArea().getY(),
                           rubberBand.getWidth(),
                           rubberBand.getHeight() },
                         event.mods.isCtrlDown());

    draggedTrack = duet::model::noTrack;
    resizingTrack = duet::model::noTrack;
    rubberBanding = false;
    clipDragged = false;
    repaint();
}

std::optional<TrackDrawing> ArrangementCanvas::trackAt (int y)
{
    const auto rows = view.tracks();
    const auto found =
        std::find_if (rows.begin(),
                      rows.end(),
                      [y] (const auto& row) { return y >= row.y && y < row.y + row.height; });

    return found == rows.end() ? std::nullopt : std::optional { *found };
}

std::optional<ArrangementCanvas::LaneHit> ArrangementCanvas::laneAt (int x, int y)
{
    for (const auto& row : view.tracks())
    {
        if (row.automationHeight <= 0 || y < row.automationY
            || y >= row.automationY + row.automationHeight)
            continue;

        const auto yInArea = y - row.automationY;

        for (const auto& lane : view.automation().lanes (row.track))
            if (yInArea >= lane.y && yInArea < lane.y + lane.height)
                return LaneHit { row, lane, x - appearance.scaled (trackHeaderWidth), yInArea };
    }

    return std::nullopt;
}

void ArrangementCanvas::showLaneMenu (duet::model::TrackRef track, int laneIndex)
{
    const auto targets = view.automation().targetsFor (track);

    juce::PopupMenu menu;
    auto item = 1;

    for (const auto& target : targets)
        menu.addItem (item++, target.name);

    menu.addSeparator();
    menu.addItem (item++, "Add Lane");
    menu.addItem (item, "Remove Lane");

    const auto addLaneItem = static_cast<int> (targets.size()) + 1;

    menu.showMenuAsync (
        {},
        [safe = juce::Component::SafePointer { this }, track, laneIndex, targets, addLaneItem] (
            int result)
        {
            if (safe == nullptr || result == 0)
                return;

            if (result == addLaneItem)
                (void) safe->view.automation().addLane (track);
            else if (result > addLaneItem)
                safe->view.automation().removeLane (track, laneIndex);
            else
                safe->view.automation().setLaneTarget (
                    track, laneIndex, targets[static_cast<std::size_t> (result - 1)].target);

            safe->repaint();
        });
}

void ArrangementCanvas::mouseDownOnLane (const juce::MouseEvent& event, const LaneHit& hit)
{
    view.focusTrack (hit.track.track);

    if (event.x < appearance.scaled (trackHeaderWidth))
    {
        if (! event.mods.isPopupMenu())
            showLaneMenu (hit.track.track, hit.lane.index);
        return;
    }

    if (event.mods.isPopupMenu())
    {
        // A point comes off where there is one; empty lane space is not a
        // point, and right-clicking it changes nothing.
        if (view.automation().removePoint (
                hit.track.track, hit.lane.index, hit.xInTimeline, hit.yInArea))
            repaint();
        return;
    }

    if (const auto point = pointIndexAt (hit.lane, hit.xInTimeline, hit.yInArea); point.has_value())
    {
        view.automation().beginPointGesture (hit.track.track, hit.lane.index, *point);
        pointGestureAreaTop = timelineArea().getY() + hit.track.automationY;
        pointDragged = false;
    }
}

std::optional<ClipDrawing> ArrangementCanvas::clipAt (const TrackDrawing& track, int x)
{
    const auto found =
        std::find_if (track.clips.begin(),
                      track.clips.end(),
                      [x] (const auto& clip) { return x >= clip.x && x < clip.x + clip.width; });
    return found == track.clips.end() ? std::nullopt : std::optional { *found };
}

void ArrangementCanvas::perform (Command command)
{
    if (command == Command::rename && view.selection().items().size() == 1)
    {
        const auto item = view.selection().items().front();
        if (item.kind == SelectionKind::clip)
            beginClipRename (item.ref);
    }
    else
        view.perform (command);

    repaint();
}

void ArrangementCanvas::beginRename (duet::model::TrackRef track)
{
    const auto rows = view.tracks();
    const auto found = std::find_if (
        rows.begin(), rows.end(), [track] (const auto& r) { return r.track == track; });

    if (found == rows.end())
        return;

    editingTrack = track;
    nameEditor = std::make_unique<juce::TextEditor>();
    nameEditor->setText (found->name, false);
    nameEditor->setBounds (
        appearance.scaled (8),
        timelineArea().getY() + found->y + appearance.scaled (6),
        appearance.scaled (trackHeaderWidth - headerControlWidth * 4 - 16),
        std::min (appearance.scaled (28), found->height - appearance.scaled (12)));
    nameEditor->onReturnKey = [this] { commitRename(); };
    nameEditor->onFocusLost = [this] { commitRename(); };
    nameEditor->onEscapeKey = [this]
    {
        editingTrack = duet::model::noTrack;
        nameEditor->onReturnKey = nullptr;
        nameEditor->onFocusLost = nullptr;
        nameEditor->onEscapeKey = nullptr;
        removeChildComponent (nameEditor.get());
        nameEditor.reset();
        grabKeyboardFocus();
        repaint();
    };
    addAndMakeVisible (*nameEditor);
    nameEditor->grabKeyboardFocus();
    nameEditor->selectAll();
}

void ArrangementCanvas::beginClipRename (duet::model::ClipRef clip)
{
    for (const auto& row : view.tracks())
        for (const auto& drawing : row.clips)
            if (drawing.clip == clip)
            {
                editingClip = clip;
                nameEditor = std::make_unique<juce::TextEditor>();
                nameEditor->setText (drawing.name, false);
                nameEditor->setBounds (
                    appearance.scaled (trackHeaderWidth) + drawing.x,
                    timelineArea().getY() + row.y + appearance.scaled (6),
                    drawing.width,
                    std::min (appearance.scaled (28), row.height - appearance.scaled (12)));
                nameEditor->onReturnKey = [this] { commitRename(); };
                nameEditor->onFocusLost = [this] { commitRename(); };
                nameEditor->onEscapeKey = [this]
                {
                    editingClip = duet::model::noClip;
                    nameEditor->onReturnKey = nullptr;
                    nameEditor->onFocusLost = nullptr;
                    nameEditor->onEscapeKey = nullptr;
                    removeChildComponent (nameEditor.get());
                    nameEditor.reset();
                    grabKeyboardFocus();
                    repaint();
                };
                addAndMakeVisible (*nameEditor);
                nameEditor->grabKeyboardFocus();
                nameEditor->selectAll();
                return;
            }
}

void ArrangementCanvas::commitRename()
{
    if (nameEditor == nullptr)
        return;

    const auto track = editingTrack;
    const auto clip = editingClip;
    const auto name = nameEditor->getText().toStdString();
    editingTrack = duet::model::noTrack;
    editingClip = duet::model::noClip;
    nameEditor->onReturnKey = nullptr;
    nameEditor->onFocusLost = nullptr;
    nameEditor->onEscapeKey = nullptr;
    removeChildComponent (nameEditor.get());
    nameEditor.reset();

    if (track != duet::model::noTrack)
        view.renameTrack (track, name);
    else if (clip != duet::model::noClip)
        view.renameSelectedClip (name);

    grabKeyboardFocus();
    repaint();
}

void ArrangementCanvas::showClipMenu (duet::model::ClipRef clip)
{
    auto targetTrack = duet::model::noTrack;
    for (const auto& row : view.tracks())
        if (std::any_of (row.clips.begin(),
                         row.clips.end(),
                         [clip] (const auto& candidate) { return candidate.clip == clip; }))
            targetTrack = row.track;

    juce::PopupMenu colours;
    constexpr std::array<const char*, duet::gui::trackColourCount> colourNames {
        "Orange", "Coral", "Mint", "Cyan", "Yellow", "Red", "Purple", "Blue"
    };
    for (std::size_t index = 0; index < colourNames.size(); ++index)
        colours.addItem (100 + static_cast<int> (index), colourNames.at (index));

    juce::PopupMenu menu;
    menu.addItem (1, "Cut");
    menu.addItem (2, "Copy");
    menu.addItem (3, "Paste");
    menu.addItem (4, "Duplicate");
    menu.addItem (5, "Delete");
    menu.addItem (6, "Rename");
    menu.addSubMenu ("Colour", colours);
    menu.showMenuAsync (
        {},
        [safe = juce::Component::SafePointer { this }, clip, targetTrack] (int result)
        {
            if (safe == nullptr || result == 0)
                return;
            if (result == 1)
                safe->view.cutSelected();
            else if (result == 2)
                safe->view.copySelected();
            else if (result == 3)
            {
                safe->view.focusTrack (targetTrack);
                safe->view.perform (Command::paste);
            }
            else if (result == 4)
                safe->view.duplicateSelected();
            else if (result == 5)
                safe->view.deleteSelected();
            else if (result == 6)
                safe->beginClipRename (clip);
            else if (result >= 100 && result < 108)
                safe->view.setSelectedClipColour (
                    static_cast<duet::model::TrackColour> (result - 100));
            safe->repaint();
        });
}

void ArrangementCanvas::showEmptyTimelineMenu (duet::model::TrackRef track, double atBeats)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Paste");
    menu.showMenuAsync ({},
                        [safe = juce::Component::SafePointer { this }, track, atBeats] (int result)
                        {
                            if (safe != nullptr && result == 1)
                            {
                                (void) safe->view.paste (atBeats, track);
                                safe->repaint();
                            }
                        });
}

void ArrangementCanvas::showTrackMenu (duet::model::TrackRef track)
{
    juce::PopupMenu colours;
    constexpr std::array<const char*, duet::gui::trackColourCount> colourNames {
        "Orange", "Coral", "Mint", "Cyan", "Yellow", "Red", "Purple", "Blue"
    };

    for (std::size_t index = 0; index < colourNames.size(); ++index)
        colours.addItem (100 + static_cast<int> (index), colourNames.at (index));

    juce::PopupMenu menu;
    menu.addItem (1, "Rename");
    menu.addItem (2, "Duplicate");
    menu.addItem (3, "Delete");
    menu.addSubMenu ("Colour", colours);
    menu.showMenuAsync ({},
                        [safe = juce::Component::SafePointer { this }, track] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result == 1)
                                safe->beginRename (track);
                            else if (result == 2)
                                safe->view.duplicateTrack (track);
                            else if (result == 3)
                                safe->view.deleteTrack (track);
                            else if (result >= 100 && result < 108)
                                safe->view.setTrackColour (
                                    track, static_cast<duet::model::TrackColour> (result - 100));

                            safe->repaint();
                        });
}

//==============================================================================
void ArrangementCanvas::timerCallback()
{
    // What the engine has published, and no lock to read it behind.
    if (view.followPlayback())
        repaint();
    movePlayheadTo (view.playheadX());
}

void ArrangementCanvas::changeListenerCallback ([[maybe_unused]] juce::ChangeBroadcaster* source)
{
    repaint();
}

void ArrangementCanvas::movePlayheadTo (int x)
{
    if (x == playheadX)
        return;

    const auto was = playheadColumn (playheadX);

    playheadX = x;

    const auto inTheRuler = [this] (juce::Rectangle<int> column)
    { return column.withY (0).withHeight (ruler->getHeight()); };

    repaint (was);
    repaint (playheadColumn (playheadX));
    ruler->repaint (inTheRuler (was));
    ruler->repaint (inTheRuler (playheadColumn (playheadX)));
}

juce::Rectangle<int> ArrangementCanvas::timelineArea() const
{
    return getLocalBounds().withTrimmedTop (appearance.scaled (rulerHeight));
}

juce::Rectangle<int> ArrangementCanvas::playheadColumn (int x) const
{
    const auto timeline = timelineArea();
    const auto thickness = std::max (1, appearance.scaled (playheadThickness));

    return {
        appearance.scaled (trackHeaderWidth) + x, timeline.getY(), thickness, timeline.getHeight()
    };
}
} // namespace duet::gui
