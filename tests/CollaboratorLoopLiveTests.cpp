// The Duet Loop, all the way through, with a real backend behind it: a real
// project, the real Tool Vocabulary, the real sidecar, and a real model
// provider deciding what to suggest.
//
// The one case here is hidden — Catch2 runs a `[.live]` case only when it is
// asked for by name or tag — because it costs money, needs credentials and
// reaches the network, and none of those belong in a suite the push gate runs.
// Everything about the loop's mechanics is asserted without it in
// `SuggestionManagerTests.cpp`, `SuggestionSurfacesTests.cpp` and
// `CollaboratorTests.cpp`; what this adds is the half no double can stand in
// for, which is a model that actually decides what to suggest.
//
// Run it, with a model whichever provider your environment configures:
//
//     DUET_LIVE_MODEL=openai:gpt-5.6 ./build/tests/Debug/duet_tests "[live]"
//
// It prints every turn of the loop — the request, the Suggestion, the Audition,
// the cherry-pick, the revision, the staleness and the redo — which is what
// issue 2suzzi's closing criterion asks to be recorded as a note.
//
// `DUET_LIVE_MODEL=duet-offline:scripted` puts a scripted model where the
// provider goes, over a script this case writes against its own fixture. That
// is not the criterion — it is how this harness itself is kept honest without
// spending anything.

#include "CollaboratorHarness.h"

#include <duet/app/Collaborator.h>
#include <duet/collab/ProjectTools.h>
#include <duet/gui/CollaboratorPanel.h>
#include <duet/gui/Suggestions.h>
#include <duet/model/Session.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using duet::app::Collaborator;
using duet::collab::Json;
using duet::gui::CollaboratorPanel;
using duet::gui::EntryKind;
using duet::gui::Suggestions;
using duet::model::BuiltinPlugin;
using duet::model::Session;
using duet::model::TrackKind;
using duet::testing::Harness;
using duet::testing::pumpUntil;
using duet::testing::TempProject;

namespace
{
#ifdef DUET_SIDECAR_BINARY
constexpr std::string_view liveSidecar = DUET_SIDECAR_BINARY;
#else
constexpr std::string_view liveSidecar;
#endif

/** The model id that means "no provider": a scripted model inside the real
    sidecar, which is how this case is exercised without spending anything.
*/
constexpr std::string_view scriptedModel = "duet-offline:scripted";

/** How long one turn of the loop is given. A provider takes seconds per turn
    and a run that reads a project takes several.
*/
constexpr int liveTurnTimeoutMs = 180000;

std::string fromEnvironment (const char* name)
{
    const auto* const value = std::getenv (name); // NOLINT(concurrency-mt-unsafe)

    return value == nullptr ? std::string {} : std::string { value };
}

/** A small project with something worth suggesting a change to: one MIDI track
    carrying one four-bar idea, under a fader that has been left low.

    Small on purpose — every Suggestion about it must name this track or this
    clip, which is what lets the case edit the project under one and know that
    it has gone stale.
*/
struct Fixture
{
    Fixture() : session (project.editFile())
    {
        session.performAction ("build the live fixture",
                               [this] (duet::model::EditOps& ops)
                               {
                                   ops.setTempo (120.0);
                                   keys = ops.createTrack (
                                       TrackKind::midi, "Keys", BuiltinPlugin::synth);
                                   verse = ops.insertMidiClip (keys, "Verse", 0.0, 8.0);

                                   for (const auto pitch : { 60, 64, 67 })
                                       ops.addNote (verse, pitch, 0.0, 4.0, 90);

                                   ops.setTrackVolumeDb (keys, -12.0);
                               });
    }

    TempProject project;
    Session session;
    duet::model::TrackRef keys = duet::model::noTrack;
    duet::model::ClipRef verse = duet::model::noClip;
};

/** One turn of the scripted model: it suggests something and then says a word
    about it, which is two provider turns.
*/
Json suggestsThen (const std::string& said, const Json& arguments)
{
    return Json::array (
        { Json { { "text", said },
                 { "toolCall", Json { { "name", "suggest" }, { "arguments", arguments } } } },
          Json { { "text", "That is what I would try." } } });
}

/** What the scripted model suggests each turn, written against the fixture's
    own ids: three Elements the first time, so that the cherry-pick has
    something to pick from, and one each time after.
*/
Json scriptFor (const Fixture& fixture)
{
    const auto track = duet::collab::toolId::forTrack (fixture.keys);
    const auto clip = duet::collab::toolId::forClip (fixture.verse);

    const auto element = [] (const std::string& description, Json operation)
    {
        return Json { { "description", description },
                      { "operations", Json::array ({ std::move (operation) }) } };
    };

    Json steps = Json::array();

    const auto add = [&steps] (const Json& turns)
    {
        for (const auto& turn : turns)
            steps.push_back (turn);
    };

    const Json opening = {
        { "summary", "Lift the keys and move the idea" },
        { "elements",
          Json::array (
              { element (
                    "Bring the keys up to -6 dB",
                    Json { { "op", "mixer.set" }, { "trackId", track }, { "volumeDb", -6.0 } }),
                element ("Move the verse to bar 3",
                         Json { { "op", "clip.move" }, { "clipId", clip }, { "startBar", 3.0 } }),
                element ("Pan the keys a little left",
                         Json { { "op", "mixer.set" }, { "trackId", track }, { "pan", -0.2 } }) }) }
    };

    const Json revised = {
        { "summary", "Lift the keys a little further" },
        { "elements",
          Json::array ({ element (
              "Bring the keys up to -4 dB",
              Json { { "op", "mixer.set" }, { "trackId", track }, { "volumeDb", -4.0 } }) }) }
    };

    const Json afresh = {
        { "summary", "Sit the keys where they now are" },
        { "elements",
          Json::array ({ element (
              "Bring the keys up to -5 dB",
              Json { { "op", "mixer.set" }, { "trackId", track }, { "volumeDb", -5.0 } }) }) }
    };

    add (suggestsThen ("The keys are buried. Here is what I would do.", opening));
    add (suggestsThen ("Then let us leave the verse where it is and only lift it.", revised));
    add (suggestsThen ("Against the project as it now stands, this is what I would do.", afresh));

    return Json { { "steps", steps } };
}
} // namespace

TEST_CASE ("the whole Duet Loop runs against a real backend on a fixture project", "[.live]")
{
    if (liveSidecar.empty() || ! std::filesystem::exists (liveSidecar))
        SKIP ("the sidecar was not built — no bun on this machine");

    const auto model = fromEnvironment ("DUET_LIVE_MODEL");

    if (model.empty())
        SKIP (
            "set DUET_LIVE_MODEL=provider:id, with that provider's credentials in the environment");

    Fixture fixture;

    // The scripted model reads its turns from a file beside the project, which
    // is what makes this case runnable with nothing configured.
    std::vector<std::string> arguments;

    if (model == scriptedModel)
    {
        const auto path = fixture.project.folder() / "offline-script.json";
        std::ofstream written { path };
        written << scriptFor (fixture).dump();
        written.close();

        arguments = { "--offline-script", path.string() };
    }

    CollaboratorPanel panel;
    Harness harness { arguments, std::filesystem::path { liveSidecar } };

    Collaborator collaborator { *harness,
                                panel,
                                [] { return duet::collab::OpeningContext {}; },
                                Collaborator::MessageThread {
                                    duet::testing::messageThreadMarshal(),
                                    duet::testing::messageThreadPost() } };

    collaborator.setSession (&fixture.session, fixture.project.folder());

    Suggestions suggestions;
    suggestions.setSource (&collaborator.suggestionSurfaces());

    harness->start();

    const auto configured = harness->configure (model, Json { { "project", "Live fixture" } });
    INFO ("configure: " << configured.error.message);
    REQUIRE (configured.succeeded);

    const auto say = [] (const std::string& what) { std::cout << "\n=== " << what << " ===\n"; };

    const auto report = [&]
    {
        for (const auto& entry : panel.conversation())
            std::cout << "  " << (entry.kind == EntryKind::suggestion ? "[card] " : "       ")
                      << entry.text << "\n";
    };

    /** Waits for the run on now to end and for its card to have arrived.

        A run that ended without one is what a model that would not suggest
        anything looks like, so what it said is printed: with a live provider
        that is the only record of why.
    */
    const auto answered = [&]
    {
        const auto ended = pumpUntil ([&] { return ! panel.taskRunning(); }, liveTurnTimeoutMs)
                           && pumpUntil (
                               [&]
                               {
                                   suggestions.refresh();

                                   return ! suggestions.cards().empty();
                               },
                               liveTurnTimeoutMs);

        if (! ended)
        {
            std::cout << "  the run left no Suggestion:\n";
            report();
        }

        return ended;
    };

    //==============================================================================
    say ("the request");
    const std::string request = "The keys feel buried. What would you do?";
    panel.setComposerText (request);
    panel.send();

    REQUIRE (answered());
    report();

    say ("the Suggestion");
    const auto first = suggestions.cards().front().id;
    const auto* card = suggestions.card (first);

    REQUIRE (card != nullptr);
    std::cout << "  " << card->summary << "\n";

    for (const auto& element : card->elements)
        std::cout << "   - " << element.description << " (" << element.clips.size()
                  << " ghost clips, " << element.faders.size() << " ghost faders)\n";

    // Nothing has moved: a Suggestion is data until the producer accepts it.
    const auto beforeAnything = fixture.session.stateDigest();
    const auto undoAtStart = fixture.session.undoNames().size();

    say ("the Audition");
    REQUIRE (suggestions.audition (first));
    std::cout << "  heard: " << fixture.session.stateDigest() << "\n";
    REQUIRE (fixture.session.stateDigest() != beforeAnything);

    suggestions.toggleAB();
    REQUIRE_FALSE (suggestions.hearingProposed());
    REQUIRE (fixture.session.stateDigest() == beforeAnything);

    suggestions.stopAudition();
    REQUIRE (fixture.session.stateDigest() == beforeAnything);
    REQUIRE (fixture.session.undoNames().size() == undoAtStart);

    say ("the cherry-pick");
    const auto rows = suggestions.card (first)->elements.size();

    if (rows > 1)
        suggestions.setChecked (first, rows - 1, false);

    std::cout << "  applying " << suggestions.checkedElements (first).size() << " of " << rows
              << " Elements\n";

    REQUIRE (suggestions.accept (first));

    // Exactly one Action, whatever the model suggested and however much of it
    // the producer took.
    REQUIRE (fixture.session.undoNames().size() == undoAtStart + 1);
    std::cout << "  one Action: " << fixture.session.undoNames().front() << "\n";

    say ("the revision");

    /** How many cards the conversation is carrying. */
    const auto cardsShown = [&panel]
    {
        std::size_t cards = 0;

        for (const auto& entry : panel.conversation())
            if (entry.kind == EntryKind::suggestion)
                ++cards;

        return cards;
    };

    REQUIRE (cardsShown() == 1);

    suggestions.refresh();

    // What is left of the first Suggestion is rejected with a reason, which is
    // what asks for a better one.
    REQUIRE (suggestions.card (first) != nullptr);
    suggestions.reject (first, "the last row was not what I meant");

    REQUIRE (answered());
    report();

    const auto second = suggestions.cards().front().id;

    REQUIRE (second != first);

    // The revision stands where the one it revises stood, so the conversation
    // does not accumulate cards.
    REQUIRE (suggestions.card (first) == nullptr);
    REQUIRE (cardsShown() == 1);

    say ("the staleness");
    REQUIRE_FALSE (suggestions.cards().front().stale);

    fixture.session.performAction ("the producer edits under it",
                                   [&fixture] (duet::model::EditOps& ops)
                                   {
                                       ops.moveClip (fixture.verse, 6.0);
                                       ops.setTrackVolumeDb (fixture.keys, -9.0);
                                       ops.setTempo (124.0);
                                   });
    suggestions.refresh();

    REQUIRE (suggestions.card (second) != nullptr);
    REQUIRE (suggestions.card (second)->stale);
    std::cout << "  " << suggestions.card (second)->summary
              << " is stale, and still auditionable\n";

    REQUIRE (suggestions.audition (second));
    suggestions.stopAudition();

    say ("the redo");
    const auto undoBeforeRedo = fixture.session.undoNames().size();

    REQUIRE (suggestions.redo (second));
    REQUIRE (answered());
    report();

    // The stale one is gone and the fresh one is in its place, and asking again
    // applied nothing.
    REQUIRE (suggestions.card (second) == nullptr);
    REQUIRE (suggestions.cards().size() == 1);
    REQUIRE (fixture.session.undoNames().size() == undoBeforeRedo);

    say ("the History");

    for (const auto& resolved : panel.history())
        std::cout << "  " << resolved.summary << " — " << resolved.outcome << "\n";

    REQUIRE_FALSE (panel.history().empty());

    // The service has a thread and the Collaborator is what it calls into, so
    // it stops before either of them goes.
    harness->stop();
}
