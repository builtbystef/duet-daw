#include <duet/gui/Suggestions.h>

#include <duet/gui/ArrangementView.h>
#include <duet/gui/Mixer.h>
#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::gui::ArrangementView;
using duet::gui::GhostClip;
using duet::gui::GhostFader;
using duet::gui::Mixer;
using duet::gui::ScriptedSuggestions;
using duet::gui::SuggestionCardView;
using duet::gui::SuggestionElementView;
using duet::gui::Suggestions;
using duet::gui::ViewState;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** A Suggestion of three Elements, the shape the worked examples name. */
[[nodiscard]] SuggestionCardView threeElements()
{
    SuggestionCardView card;
    card.id = "sug-1";
    card.summary = "Tighten the intro";

    SuggestionElementView moved;
    moved.description = "Move the hats two bars later";
    moved.clips.push_back (GhostClip { 7, "Hats", 4.0, 2.0, true });

    SuggestionElementView doubled;
    doubled.description = "Double the bass under it";
    doubled.clips.push_back (GhostClip { 9, "Bass", 0.0, 4.0, false });

    SuggestionElementView lifted;
    lifted.description = "Bring the drums up to -3.0 dB";
    lifted.faders.push_back (GhostFader { 7, -3.0 });

    card.elements = { moved, doubled, lifted };
    return card;
}

/** A source that answers with what a test put in it and remembers what it was
    asked, so that what the view-model does with a gesture can be read off it.
*/
class RecordedSource final : public Suggestions::Source
{
public:
    std::vector<SuggestionCardView> cards;
    std::vector<std::size_t> auditioned;
    std::vector<std::size_t> accepted;
    std::string rejected;
    int auditionCalls = 0;
    int stopCalls = 0;

    std::vector<SuggestionCardView> pending() override { return cards; }

    bool audition (std::string_view /*id*/, const std::vector<std::size_t>& elements) override
    {
        ++auditionCalls;
        auditioned = elements;
        return ! elements.empty();
    }

    void stopAudition() override { ++stopCalls; }

    bool accept (std::string_view id, const std::vector<std::size_t>& elements) override
    {
        accepted = elements;

        for (auto& card : cards)
            if (card.id == id)
            {
                std::vector<SuggestionElementView> left;

                for (std::size_t index = 0; index < card.elements.size(); ++index)
                    if (std::find (elements.begin(), elements.end(), index) == elements.end())
                        left.push_back (card.elements[index]);

                card.elements = left;
            }

        std::erase_if (cards, [] (const auto& card) { return card.elements.empty(); });
        return true;
    }

    void reject (std::string_view id) override
    {
        rejected = std::string { id };
        std::erase_if (cards, [id] (const auto& card) { return card.id == id; });
    }
};
} // namespace

TEST_CASE ("a Suggestion arrives with every Element ticked, and unticking one takes it out of "
           "what is auditioned and applied")
{
    RecordedSource source;
    source.cards = { threeElements() };

    Suggestions suggestions;
    suggestions.setSource (&source);
    suggestions.refresh();

    REQUIRE (suggestions.cards().size() == 1);
    REQUIRE (suggestions.checkedElements ("sug-1") == std::vector<std::size_t> { 0, 1, 2 });

    suggestions.setChecked ("sug-1", 1, false);

    REQUIRE (suggestions.isChecked ("sug-1", 0));
    REQUIRE_FALSE (suggestions.isChecked ("sug-1", 1));
    REQUIRE (suggestions.checkedElements ("sug-1") == std::vector<std::size_t> { 0, 2 });

    REQUIRE (suggestions.audition ("sug-1"));
    REQUIRE (source.auditioned == std::vector<std::size_t> { 0, 2 });

    // A tick the producer has made is theirs, and a refresh that reads the
    // Suggestion again does not put it back.
    suggestions.refresh();

    REQUIRE_FALSE (suggestions.isChecked ("sug-1", 1));
}

TEST_CASE ("an Element the producer has unticked is drawn at the excluded intensity, in its row "
           "and in its ghosts")
{
    RecordedSource source;
    source.cards = { threeElements() };

    Suggestions suggestions;
    suggestions.setSource (&source);
    suggestions.refresh();

    REQUIRE_THAT (suggestions.intensityOf ("sug-1", 1), WithinAbs (1.0, 1e-9));

    suggestions.setChecked ("sug-1", 1, false);

    REQUIRE_THAT (suggestions.intensityOf ("sug-1", 1),
                  WithinAbs (Suggestions::excludedIntensity, 1e-9));
    REQUIRE_THAT (suggestions.intensityOf ("sug-1", 0), WithinAbs (1.0, 1e-9));
}

TEST_CASE ("Audition takes the wash off a Suggestion's ghosts, and leaving it puts it back")
{
    RecordedSource source;
    source.cards = { threeElements() };

    Suggestions suggestions;
    suggestions.setSource (&source);
    suggestions.refresh();

    REQUIRE_THAT (suggestions.fillAlphaOf ("sug-1"),
                  WithinAbs (Suggestions::pendingFillAlpha, 1e-9));
    REQUIRE_FALSE (suggestions.auditioning().has_value());

    REQUIRE (suggestions.audition ("sug-1"));

    REQUIRE (suggestions.isAuditioning ("sug-1"));
    REQUIRE (suggestions.hearingProposed());

    // The Audition puts the suggested clip into the project, so the ghost has
    // the real thing under it and carries no wash of its own: what marks it
    // while it is heard is the solid border and the badge.
    REQUIRE_THAT (suggestions.fillAlphaOf ("sug-1"),
                  WithinAbs (Suggestions::auditionFillAlpha, 1e-9));
    REQUIRE_THAT (suggestions.fillAlphaOf ("sug-1"), WithinAbs (0.0, 1e-9));

    suggestions.stopAudition();

    REQUIRE_FALSE (suggestions.auditioning().has_value());
    REQUIRE (source.stopCalls == 1);
    REQUIRE_THAT (suggestions.fillAlphaOf ("sug-1"),
                  WithinAbs (Suggestions::pendingFillAlpha, 1e-9));
}

TEST_CASE ("A/B swaps the heard side without leaving the Audition")
{
    RecordedSource source;
    source.cards = { threeElements() };

    Suggestions suggestions;
    suggestions.setSource (&source);
    suggestions.refresh();
    REQUIRE (suggestions.audition ("sug-1"));

    suggestions.toggleAB();

    // A is the project as it stands, so what is heard is what the model holds
    // with the Audition reverted — and the Audition itself is still open.
    REQUIRE_FALSE (suggestions.hearingProposed());
    REQUIRE (suggestions.isAuditioning ("sug-1"));
    REQUIRE (source.stopCalls == 1);

    suggestions.toggleAB();

    REQUIRE (suggestions.hearingProposed());
    REQUIRE (suggestions.isAuditioning ("sug-1"));
    REQUIRE (source.auditionCalls == 2);
}

TEST_CASE ("Accept applies the ticked Elements and leaves the rest of the card as it was")
{
    RecordedSource source;
    source.cards = { threeElements() };

    Suggestions suggestions;
    suggestions.setSource (&source);
    suggestions.refresh();
    suggestions.setChecked ("sug-1", 1, false);

    REQUIRE (suggestions.accept ("sug-1"));

    REQUIRE (source.accepted == std::vector<std::size_t> { 0, 2 });
    REQUIRE (suggestions.cards().size() == 1);

    const auto* card = suggestions.card ("sug-1");

    REQUIRE (card != nullptr);
    REQUIRE (card->elements.size() == 1);
    REQUIRE (card->elements.front().description == "Double the bass under it");

    // What is left is what the producer said no to, so it is still unticked and
    // still drawn at the excluded intensity: the rest of the card as it was.
    REQUIRE_FALSE (suggestions.isChecked ("sug-1", 0));
    REQUIRE (suggestions.checkedElements ("sug-1").empty());
}

TEST_CASE ("Reject takes the whole Suggestion down")
{
    RecordedSource source;
    source.cards = { threeElements() };

    Suggestions suggestions;
    suggestions.setSource (&source);
    suggestions.refresh();
    REQUIRE (suggestions.audition ("sug-1"));

    suggestions.reject ("sug-1");

    REQUIRE (source.rejected == "sug-1");
    REQUIRE (suggestions.cards().empty());
    REQUIRE (suggestions.card ("sug-1") == nullptr);
    REQUIRE_FALSE (suggestions.auditioning().has_value());
}

TEST_CASE ("a Suggestion with nothing ticked has nothing to hear")
{
    RecordedSource source;
    source.cards = { threeElements() };

    Suggestions suggestions;
    suggestions.setSource (&source);
    suggestions.refresh();

    for (std::size_t element = 0; element < 3; ++element)
        suggestions.setChecked ("sug-1", element, false);

    REQUIRE_FALSE (suggestions.audition ("sug-1"));
    REQUIRE_FALSE (suggestions.auditioning().has_value());
}

TEST_CASE ("unticking the last Element mid-Audition silences it and leaves the Audition open")
{
    RecordedSource source;
    source.cards = { threeElements() };

    Suggestions suggestions;
    suggestions.setSource (&source);

    REQUIRE (suggestions.audition ("sug-1"));

    const auto stoppedBefore = source.stopCalls;

    for (std::size_t element = 0; element < 3; ++element)
        suggestions.setChecked ("sug-1", element, false);

    // The last untick takes the sound off; the Audition is still the place the
    // producer is standing, so ticking one back puts it on again.
    REQUIRE (source.stopCalls == stoppedBefore + 1);
    REQUIRE (suggestions.isAuditioning ("sug-1"));
    REQUIRE (suggestions.hearingProposed());

    suggestions.setChecked ("sug-1", 1, true);

    REQUIRE (source.auditioned == std::vector<std::size_t> { 1 });
}

TEST_CASE ("a pending Suggestion's clips are ghosts on the timeline, where its operations put them")
{
    const TempProject project;
    Session session { project.editFile() };
    TrackRef first = duet::model::noTrack;
    TrackRef second = duet::model::noTrack;
    session.performAction ("Tracks",
                           [&] (auto& ops)
                           {
                               first = ops.createTrack (TrackKind::midi, "First");
                               second = ops.createTrack (TrackKind::midi, "Second");
                               ops.insertMidiClip (first, "Hats", 0.0, 2.0);
                           });

    ViewState view;
    view.setHZoomPxPerBeat (20.0);

    ArrangementView arrangement { view };
    arrangement.setSession (&session);
    arrangement.setWidthPx (800);

    RecordedSource source;
    SuggestionCardView card;
    card.id = "sug-1";
    card.summary = "Tighten the intro";

    SuggestionElementView moved;
    moved.description = "Move the hats two bars later";
    moved.clips.push_back (GhostClip { second, "Hats", 4.0, 2.0, true });
    card.elements = { moved };
    source.cards = { card };

    Suggestions suggestions;
    suggestions.setSource (&source);
    arrangement.setSuggestions (&suggestions);

    const auto ghosts = arrangement.ghosts();

    REQUIRE (ghosts.size() == 1);

    // 120 BPM, so four seconds is eight beats, and two seconds is four: the
    // ghost is where the operation puts it and as long as the operation makes
    // it.
    REQUIRE (ghosts.front().x == arrangement.geometry().beatsToX (8.0));
    REQUIRE (ghosts.front().width
             == arrangement.geometry().beatsToX (12.0) - arrangement.geometry().beatsToX (8.0));
    REQUIRE (ghosts.front().name == "Hats");
    REQUIRE (ghosts.front().suggestion == "sug-1");

    // It hangs off the track its operation names, in that row and no other.
    const auto rows = arrangement.tracks();
    const auto row =
        std::find_if (rows.begin(),
                      rows.end(),
                      [second] (const auto& drawing) { return drawing.track == second; });

    REQUIRE (row != rows.end());
    REQUIRE (ghosts.front().y == row->y);
    REQUIRE (ghosts.front().height == row->height);
}

TEST_CASE ("a ghost is not a clip: the smart tool never reaches one")
{
    const TempProject project;
    Session session { project.editFile() };
    TrackRef track = duet::model::noTrack;
    session.performAction ("Track",
                           [&] (auto& ops) { track = ops.createTrack (TrackKind::midi, "First"); });

    ViewState view;
    view.setHZoomPxPerBeat (20.0);

    ArrangementView arrangement { view };
    arrangement.setSession (&session);
    arrangement.setWidthPx (800);

    RecordedSource source;
    SuggestionCardView card;
    card.id = "sug-1";
    card.summary = "Add a fill";

    SuggestionElementView added;
    added.description = "Add a fill on the last bar";
    added.clips.push_back (GhostClip { track, "Fill", 4.0, 2.0, true });
    card.elements = { added };
    source.cards = { card };

    Suggestions suggestions;
    suggestions.setSource (&source);
    arrangement.setSuggestions (&suggestions);

    const auto ghosts = arrangement.ghosts();
    REQUIRE (ghosts.size() == 1);

    // Nothing the producer can select is there: a ghost is a drawing, so the
    // one thing the surface says about the pointer over it is that it is over
    // one.
    REQUIRE (arrangement.allClipItems().empty());
    REQUIRE (arrangement.ghostAt (ghosts.front().x + 2, ghosts.front().y + 2));
    REQUIRE_FALSE (arrangement.ghostAt (ghosts.front().x - 20, ghosts.front().y + 2));
}

TEST_CASE ("a suggested level is a ghost handle beside the fader, and the A/B chip belongs to the "
           "Audition")
{
    const TempProject project;
    Session session { project.editFile() };
    TrackRef drums = duet::model::noTrack;
    TrackRef bass = duet::model::noTrack;
    session.performAction ("Tracks",
                           [&] (auto& ops)
                           {
                               drums = ops.createTrack (TrackKind::audio, "Drums");
                               bass = ops.createTrack (TrackKind::audio, "Bass");
                               ops.setTrackVolumeDb (drums, -6.0);
                           });

    RecordedSource source;
    SuggestionCardView card;
    card.id = "sug-1";
    card.summary = "Lift the drums";

    SuggestionElementView lifted;
    lifted.description = "Bring the drums up to -3.0 dB";
    lifted.faders.push_back (GhostFader { drums, -3.0 });
    card.elements = { lifted };
    source.cards = { card };

    Suggestions suggestions;
    suggestions.setSource (&source);

    Mixer mixer;
    mixer.setSession (&session);
    mixer.setSuggestions (&suggestions);

    const auto ghost = mixer.ghostFader (drums);

    REQUIRE (ghost.has_value());
    REQUIRE_THAT (ghost.value_or (duet::gui::GhostFaderDrawing {}).db, WithinAbs (-3.0, 1e-9));

    // The real fader has not moved: a Suggestion is data until it is accepted.
    REQUIRE_THAT (mixer.strip (drums).volumeDb, WithinAbs (-6.0, 1e-9));
    REQUIRE_FALSE (mixer.ghostFader (bass).has_value());
    REQUIRE_FALSE (mixer.auditionChip (drums).visible);

    REQUIRE (suggestions.audition ("sug-1"));

    REQUIRE (mixer.auditionChip (drums).visible);
    REQUIRE (mixer.auditionChip (drums).proposedHeard);
    REQUIRE_FALSE (mixer.auditionChip (bass).visible);

    suggestions.toggleAB();

    REQUIRE (mixer.auditionChip (drums).visible);
    REQUIRE_FALSE (mixer.auditionChip (drums).proposedHeard);

    suggestions.stopAudition();

    REQUIRE_FALSE (mixer.auditionChip (drums).visible);
    REQUIRE (mixer.ghostFader (drums).has_value());
}

namespace
{
/** A project with something to suggest a change to, and the development
    Suggestion made over it.
*/
struct FabricatedSuggestion
{
    FabricatedSuggestion() : session (project.editFile())
    {
        session.performAction ("Keys",
                               [&] (auto& ops)
                               {
                                   track = ops.createTrack (TrackKind::midi, "Keys");
                                   clip = ops.insertMidiClip (track, "Riff", 0.0, 1.0);
                                   ops.setTrackVolumeDb (track, -6.0);
                               });

        scripted.setSession (&session);
        REQUIRE (scripted.fabricate());
        suggestions.setSource (&scripted);
    }

    [[nodiscard]] std::string id() const
    {
        REQUIRE (suggestions.cards().size() == 1);
        return suggestions.cards().front().id;
    }

    [[nodiscard]] std::size_t clipCount() const { return session.track (track).clips.size(); }

    TempProject project;
    Session session;
    ScriptedSuggestions scripted;
    Suggestions suggestions;
    TrackRef track = duet::model::noTrack;
    duet::model::ClipRef clip = duet::model::noClip;
};
} // namespace

TEST_CASE ("Audition makes the development Suggestion audible, and leaving it puts the project "
           "back exactly")
{
    FabricatedSuggestion made;
    const auto before = made.session.stateDigest();

    REQUIRE (made.suggestions.cards().front().elements.size() == 3);
    REQUIRE (made.suggestions.audition (made.id()));

    // What is heard is the whole Suggestion: the clip where it would go, the
    // copy it would make, and the level it would set.
    REQUIRE (made.clipCount() == 2);
    REQUIRE_THAT (made.session.track (made.track).volumeDb,
                  WithinAbs (ScriptedSuggestions::proposedLevelDb, 1e-6));

    made.suggestions.stopAudition();

    REQUIRE (made.session.stateDigest() == before);
    REQUIRE_THAT (made.session.track (made.track).volumeDb, WithinAbs (-6.0, 1e-6));
}

TEST_CASE ("the mixer reads the project again when an Audition ends")
{
    FabricatedSuggestion made;

    Mixer mixer;
    mixer.setSession (&made.session);
    mixer.setSuggestions (&made.suggestions);

    REQUIRE_THAT (mixer.strip (made.track).volumeDb, WithinAbs (-6.0, 1e-6));
    REQUIRE (made.suggestions.audition (made.id()));

    // The strip is read while the Audition is on, so what the mixer holds is
    // the suggested level. Leaving has to take it off again: the chip invites
    // the producer to compare, and the numbers beside it have to be the ones
    // they are hearing.
    REQUIRE_THAT (mixer.strip (made.track).volumeDb,
                  WithinAbs (ScriptedSuggestions::proposedLevelDb, 1e-6));

    made.suggestions.stopAudition();

    REQUIRE_THAT (mixer.strip (made.track).volumeDb, WithinAbs (-6.0, 1e-6));
}

TEST_CASE ("an Audition is not the project moving under a Suggestion")
{
    FabricatedSuggestion made;

    REQUIRE_FALSE (made.suggestions.cards().front().stale);
    REQUIRE (made.suggestions.audition (made.id()));

    made.suggestions.stopAudition();
    made.suggestions.refresh();

    // The revert put the project back exactly, so nothing has moved under the
    // Suggestion and it is not stale — hearing a Suggestion is not editing.
    REQUIRE_FALSE (made.suggestions.cards().front().stale);
}

TEST_CASE ("an Audition hears only the Elements the producer has left ticked")
{
    FabricatedSuggestion made;

    // The second Element is the copy the Suggestion would make; unticked, it is
    // not among what is heard.
    made.suggestions.setChecked (made.id(), 1, false);

    REQUIRE (made.suggestions.audition (made.id()));
    REQUIRE (made.clipCount() == 1);
    REQUIRE_THAT (made.session.track (made.track).volumeDb,
                  WithinAbs (ScriptedSuggestions::proposedLevelDb, 1e-6));

    made.suggestions.stopAudition();
}

TEST_CASE ("A/B swaps what is heard while the transport rolls, and never stops it")
{
    FabricatedSuggestion made;
    made.session.useNoAudioDevice();
    made.session.startPlayback();
    REQUIRE (made.session.isPlaying());

    REQUIRE (made.suggestions.audition (made.id()));
    REQUIRE (made.session.isPlaying());
    REQUIRE (made.clipCount() == 2);

    made.suggestions.toggleAB();

    REQUIRE (made.session.isPlaying());
    REQUIRE_FALSE (made.suggestions.hearingProposed());
    REQUIRE (made.clipCount() == 1);

    made.suggestions.toggleAB();

    REQUIRE (made.session.isPlaying());
    REQUIRE (made.suggestions.hearingProposed());
    REQUIRE (made.clipCount() == 2);

    made.suggestions.stopAudition();
    REQUIRE (made.session.isPlaying());
}

TEST_CASE ("accepting a Suggestion lands as exactly one Action, and one undo removes all of it")
{
    FabricatedSuggestion made;
    const auto before = made.session.stateDigest();
    const auto depthBefore = made.session.undoNames().size();

    REQUIRE (made.suggestions.audition (made.id()));
    REQUIRE (made.suggestions.accept (made.id()));

    REQUIRE (made.session.undoNames().size() == depthBefore + 1);
    REQUIRE (made.session.stateDigest() != before);
    REQUIRE (made.clipCount() == 2);
    REQUIRE_THAT (made.session.track (made.track).volumeDb,
                  WithinAbs (ScriptedSuggestions::proposedLevelDb, 1e-6));

    // Accepting everything leaves nothing pending, so the ghosts go with it.
    REQUIRE (made.suggestions.cards().empty());
    REQUIRE_FALSE (made.suggestions.auditioning().has_value());

    REQUIRE (made.session.undo());
    REQUIRE (made.session.stateDigest() == before);
}

TEST_CASE ("accepting the ticked Elements clears their ghosts and leaves the rest of the card")
{
    FabricatedSuggestion made;
    made.suggestions.setChecked (made.id(), 1, false);

    REQUIRE (made.suggestions.accept (made.id()));

    const auto* card = made.suggestions.card (made.id());

    REQUIRE (card != nullptr);
    REQUIRE (card->elements.size() == 1);

    // The copy was never made: only what the producer ticked was applied.
    REQUIRE (made.clipCount() == 1);
    REQUIRE_THAT (made.session.track (made.track).volumeDb,
                  WithinAbs (ScriptedSuggestions::proposedLevelDb, 1e-6));
}

TEST_CASE ("rejecting a Suggestion leaves the project, the undo stack and the redo stack alone")
{
    FabricatedSuggestion made;
    REQUIRE (made.session.undo());
    REQUIRE (made.session.redo());

    const auto before = made.session.stateDigest();
    const auto undoDepth = made.session.undoNames().size();
    const auto redoDepth = made.session.redoNames().size();

    REQUIRE (made.suggestions.audition (made.id()));
    made.suggestions.reject (made.id());

    REQUIRE (made.suggestions.cards().empty());
    REQUIRE (made.session.stateDigest() == before);
    REQUIRE (made.session.undoNames().size() == undoDepth);
    REQUIRE (made.session.redoNames().size() == redoDepth);
}

TEST_CASE ("a Suggestion the producer has edited under is marked stale and stays auditionable")
{
    FabricatedSuggestion made;

    REQUIRE_FALSE (made.suggestions.cards().front().stale);

    made.session.performAction ("Move", [&] (auto& ops) { ops.moveClip (made.clip, 4.0); });
    made.suggestions.refresh();

    REQUIRE (made.suggestions.cards().front().stale);

    // Stale takes away a Suggestion's claim to still fit, and nothing else.
    REQUIRE (made.suggestions.audition (made.id()));
    REQUIRE (made.clipCount() == 2);
    made.suggestions.stopAudition();
}
