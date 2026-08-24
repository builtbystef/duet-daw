#include <duet/gui/ViewState.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace duet::gui
{
namespace
{
    /** The names the VIEW tree holds the view under, exactly as spec 535bbo
        writes them: a project file is read by more than the code that wrote it.
    */
    namespace attribute
    {
        constexpr const char* hZoomPxPerBeat = "hZoomPxPerBeat";
        constexpr const char* hScrollBeats = "hScrollBeats";
        constexpr const char* vScrollPx = "vScrollPx";
        constexpr const char* browserVisible = "browserVisible";
        constexpr const char* collaboratorVisible = "collaboratorVisible";
        constexpr const char* bottomVisible = "bottomVisible";
        constexpr const char* browserWidthPx = "browserWidthPx";
        constexpr const char* collaboratorWidthPx = "collaboratorWidthPx";
        constexpr const char* bottomHeightPx = "bottomHeightPx";
        constexpr const char* bottomTab = "bottomTab";
        constexpr const char* trackRef = "trackRef";
        constexpr const char* heightPx = "heightPx";
        constexpr const char* lanesExpanded = "lanesExpanded";
    } // namespace attribute

    /** The tab, written the way spec 535bbo writes it, so that the file says
        which surface the producer left in front rather than which number it is.
    */
    constexpr const char* pianoRollTabName = "pianoRoll";
    constexpr const char* mixerTabName = "mixer";
} // namespace

//==============================================================================
void ViewState::setHZoomPxPerBeat (double newZoom)
{
    zoomPxPerBeat = std::clamp (newZoom, minimumZoomPxPerBeat, maximumZoomPxPerBeat);
}

void ViewState::setHScrollBeats (double newScroll) { scrollBeats = std::max (0.0, newScroll); }

void ViewState::setVScrollPx (int newScroll) { scrollPx = std::max (0, newScroll); }

void ViewState::setBrowserWidthPx (int newWidth)
{
    browserWidth = std::clamp (newWidth, minimumBrowserWidthPx, maximumBrowserWidthPx);
}

void ViewState::setCollaboratorWidthPx (int newWidth)
{
    collaboratorWidth =
        std::clamp (newWidth, minimumCollaboratorWidthPx, maximumCollaboratorWidthPx);
}

void ViewState::setBottomHeightPx (int newHeight)
{
    bottomHeight = std::clamp (newHeight, minimumBottomHeightPx, maximumBottomHeightPx);
}

//==============================================================================
int ViewState::trackHeightPx (duet::model::TrackRef track) const
{
    const auto row = trackRows.find (track);

    return row == trackRows.end() ? defaultTrackHeightPx : row->second.heightPx;
}

void ViewState::ensureTrack (duet::model::TrackRef track)
{
    if (track != duet::model::noTrack)
        trackRows.try_emplace (track);
}

void ViewState::syncTracks (std::span<const duet::model::TrackRef> tracks)
{
    for (const auto track : tracks)
        ensureTrack (track);

    std::erase_if (trackRows,
                   [tracks] (const auto& row)
                   { return std::find (tracks.begin(), tracks.end(), row.first) == tracks.end(); });
}

void ViewState::removeTrack (duet::model::TrackRef track) { trackRows.erase (track); }

bool ViewState::hasTrack (duet::model::TrackRef track) const { return trackRows.contains (track); }

void ViewState::setTrackHeightPx (duet::model::TrackRef track, int newHeight)
{
    if (track == duet::model::noTrack)
        return;

    trackRows[track].heightPx = std::clamp (newHeight, minimumTrackHeightPx, maximumTrackHeightPx);
}

void ViewState::scaleTrackHeights (double factor)
{
    for (auto& [track, row] : trackRows)
        row.heightPx = std::clamp (static_cast<int> (std::lround (row.heightPx * factor)),
                                   minimumTrackHeightPx,
                                   maximumTrackHeightPx);
}

bool ViewState::lanesExpanded (duet::model::TrackRef track) const
{
    const auto row = trackRows.find (track);

    return row != trackRows.end() && row->second.lanesExpanded;
}

void ViewState::setLanesExpanded (duet::model::TrackRef track, bool shouldBeExpanded)
{
    trackRows[track].lanesExpanded = shouldBeExpanded;
}

//==============================================================================
duet::persistence::DataNode ViewState::toData() const
{
    duet::persistence::DataNode view { std::string { duet::persistence::viewTreeName } };

    view.set (attribute::hZoomPxPerBeat, zoomPxPerBeat);
    view.set (attribute::hScrollBeats, scrollBeats);
    view.set (attribute::vScrollPx, scrollPx);
    view.set (attribute::browserVisible, browserOpen);
    view.set (attribute::collaboratorVisible, collaboratorOpen);
    view.set (attribute::bottomVisible, bottomOpen);
    view.set (attribute::browserWidthPx, browserWidth);
    view.set (attribute::collaboratorWidthPx, collaboratorWidth);
    view.set (attribute::bottomHeightPx, bottomHeight);
    view.set (attribute::bottomTab, tab == BottomTab::mixer ? mixerTabName : pianoRollTabName);

    for (const auto& [track, row] : trackRows)
    {
        duet::persistence::DataNode trackView { trackTreeType };

        trackView.set (attribute::trackRef, static_cast<std::uint64_t> (track));
        trackView.set (attribute::heightPx, row.heightPx);
        trackView.set (attribute::lanesExpanded, row.lanesExpanded);

        view.add (std::move (trackView));
    }

    return view;
}

void ViewState::readFrom (const duet::persistence::DataNode& stored)
{
    setHZoomPxPerBeat (stored.doubleValue (attribute::hZoomPxPerBeat, zoomPxPerBeat));
    setHScrollBeats (stored.doubleValue (attribute::hScrollBeats, scrollBeats));
    setVScrollPx (stored.intValue (attribute::vScrollPx, scrollPx));

    browserOpen = stored.boolValue (attribute::browserVisible, browserOpen);
    collaboratorOpen = stored.boolValue (attribute::collaboratorVisible, collaboratorOpen);
    bottomOpen = stored.boolValue (attribute::bottomVisible, bottomOpen);

    setBrowserWidthPx (stored.intValue (attribute::browserWidthPx, browserWidth));
    setCollaboratorWidthPx (stored.intValue (attribute::collaboratorWidthPx, collaboratorWidth));
    setBottomHeightPx (stored.intValue (attribute::bottomHeightPx, bottomHeight));

    tab = stored.stringValue (attribute::bottomTab, pianoRollTabName) == mixerTabName
              ? BottomTab::mixer
              : BottomTab::pianoRoll;

    for (const auto& trackView : stored.children())
    {
        if (trackView.type() != trackTreeType)
            continue;

        const auto track = static_cast<duet::model::TrackRef> (
            trackView.uint64Value (attribute::trackRef, duet::model::noTrack));

        // A row belongs to a track. One that names none belongs to nothing, and
        // keeping it would give the next track to be created someone else's
        // height.
        if (track == duet::model::noTrack)
            continue;

        setTrackHeightPx (track, trackView.intValue (attribute::heightPx, defaultTrackHeightPx));
        setLanesExpanded (track, trackView.boolValue (attribute::lanesExpanded, false));
    }
}
} // namespace duet::gui
