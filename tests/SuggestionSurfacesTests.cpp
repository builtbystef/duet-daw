#include <duet/app/SuggestionSurfaces.h>

#include <duet/collab/SuggestTool.h>
#include <duet/collab/ToolDispatch.h>
#include <duet/gui/Mixer.h>
#include <duet/gui/Suggestions.h>
#include <duet/model/Session.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::app::SuggestionSurfaces;
using duet::collab::Json;
using duet::collab::RunStart;
using duet::gui::Mixer;
using duet::gui::Suggestions;
using duet::model::BuiltinPlugin;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** The proposed level the worked examples move a fader to. */
constexpr double proposedLevelDb = -3.0;

Json operation (const std::string& name, Json fields)
{
    Json out = Json::object();
    out["op"] = name;

    for (const auto& field : fields.items())
        out[field.key()] = field.value();

    return out;
}

Json element (const std::string& description, Json operations)
{
    return Json { { "description", description }, { "operations", std::move (operations) } };
}

/** A project with a clip to move and a fader to lift, and the whole Duet Loop
    over it: the real write-tool, the real manager, and the surfaces the ghosts
    are read off.

    The Task Runs are this object's — no socket and no sidecar — because what is
    asserted here is what the interface reads off the manager. What the manager
    holds is the real `suggest` tool's work against the real project.
*/
class Surfaces
{
public:
    Surfaces()
        : session (project.editFile()), writes (session, duet::testing::messageThreadMarshal()),
          manager (session,
                   [this] (const std::string& prompt)
                   {
                       asked.push_back (prompt);

                       return RunStart::accepted ("run-" + std::to_string (asked.size()));
                   })
    {
        session.performAction (
            "Keys",
            [&] (auto& ops)
            {
                keys = ops.createTrack (TrackKind::midi, "Keys", BuiltinPlugin::synth);
                riff = ops.insertMidiClip (keys, "Riff", 0.0, 1.0);
                ops.setTrackVolumeDb (keys, -6.0);
                pad = ops.createTrack (TrackKind::midi, "Pad", BuiltinPlugin::synth);
            });

        writes.addTo (registry);
        surfaces.setManager (&manager, &session);
        suggestions.setSource (&surfaces);
    }

    /** One whole turn: the producer asks, and the run answers with a Suggestion
        of these Elements. The Suggestion's id comes back.
    */
    std::string turn (const std::string& request, const std::string& summary, Json elements)
    {
        const auto started = manager.ask (request);
        REQUIRE (started.started);

        Json arguments = Json::object();
        arguments["summary"] = summary;
        arguments["elements"] = std::move (elements);

        const auto outcome = registry.call (Json {
            { "runId", started.runId }, { "tool", "suggest" }, { "args", std::move (arguments) } });
        REQUIRE (outcome.succeeded);

        const auto id = outcome.result.at ("suggestionId").get<std::string>();
        const auto made = writes.suggestion (id);
        REQUIRE (made.has_value());
        REQUIRE (
            manager.suggested (started.runId, made.value_or (duet::collab::Suggestion { {}, {} })));

        suggestions.refresh();

        return id;
    }

    /** The three-Element Suggestion the worked examples are made of: the riff
        moved, the riff copied, and the fader lifted.
    */
    std::string threeElements (const std::string& request = "rework the intro",
                               const std::string& summary = "Rework the intro")
    {
        const auto clip = duet::collab::toolId::forClip (riff);
        const auto track = duet::collab::toolId::forTrack (keys);

        return turn (
            request,
            summary,
            Json::array (
                { element ("Move the riff to bar 2",
                           Json::array ({ operation (
                               "clip.move", { { "clipId", clip }, { "startBar", 2.0 } }) })),
                  element ("Double the riff at bar 3",
                           Json::array ({ operation ("clip.duplicate",
                                                     { { "clipId", clip }, { "atBar", 3.0 } }) })),
                  element ("Bring the keys up",
                           Json::array ({ operation (
                               "mixer.set",
                               { { "trackId", track }, { "volumeDb", proposedLevelDb } }) })) }));
    }

    [[nodiscard]] std::size_t clipCount() const { return session.track (keys).clips.size(); }

    /** Every prompt a run has been started with, in order. */
    [[nodiscard]] const std::vector<std::string>& prompts() const { return asked; }

    TempProject project;
    Session session;
    TrackRef keys = duet::model::noTrack;
    TrackRef pad = duet::model::noTrack;
    duet::model::ClipRef riff = duet::model::noClip;

    duet::collab::ToolRegistry registry;
    duet::collab::SuggestTool writes;
    std::vector<std::string> asked;
    duet::collab::SuggestionManager manager;
    SuggestionSurfaces surfaces;
    Suggestions suggestions;
};
} // namespace

TEST_CASE ("a Suggestion's ghosts stand where its operations say, and the project is untouched")
{
    Surfaces made;
    const auto before = made.session.stateDigest();
    const auto undoDepth = made.session.undoNames().size();

    const auto id = made.threeElements();

    REQUIRE (made.suggestions.cards().size() == 1);

    const auto& card = made.suggestions.cards().front();

    REQUIRE (card.id == id);
    REQUIRE (card.summary == "Rework the intro");
    REQUIRE (card.elements.size() == 3);

    // Bar 2 and bar 3, as the operations wrote them, in the seconds the surfaces
    // draw in — and the riff's own name and length, which the operations never
    // said and the project did.
    REQUIRE (card.elements[0].clips.size() == 1);
    REQUIRE (card.elements[0].clips.front().track == made.keys);
    REQUIRE (card.elements[0].clips.front().name == "Riff");
    REQUIRE_THAT (card.elements[0].clips.front().startSeconds,
                  WithinAbs (made.session.secondsAtBar (2.0), 1e-9));
    REQUIRE_THAT (card.elements[0].clips.front().lengthSeconds, WithinAbs (1.0, 1e-9));
    REQUIRE (card.elements[0].clips.front().holdsMidi);

    REQUIRE (card.elements[1].clips.size() == 1);
    REQUIRE_THAT (card.elements[1].clips.front().startSeconds,
                  WithinAbs (made.session.secondsAtBar (3.0), 1e-9));

    REQUIRE (card.elements[2].faders.size() == 1);
    REQUIRE (card.elements[2].faders.front().channel == made.keys);
    REQUIRE_THAT (card.elements[2].faders.front().db, WithinAbs (proposedLevelDb, 1e-9));

    // A Suggestion is data until it is accepted: nothing moved and no undo step
    // was made.
    REQUIRE (made.session.stateDigest() == before);
    REQUIRE (made.session.undoNames().size() == undoDepth);
}

TEST_CASE ("an Audition of a Suggestion is heard in place and leaves the project digest-exact")
{
    Surfaces made;
    const auto id = made.threeElements();
    const auto before = made.session.stateDigest();

    REQUIRE (made.suggestions.audition (id));

    REQUIRE (made.clipCount() == 2);
    REQUIRE_THAT (made.session.track (made.keys).volumeDb, WithinAbs (proposedLevelDb, 1e-6));

    made.suggestions.stopAudition();

    REQUIRE (made.session.stateDigest() == before);
    REQUIRE_THAT (made.session.track (made.keys).volumeDb, WithinAbs (-6.0, 1e-6));
}

TEST_CASE ("A/B swaps what is heard while the transport rolls, and never stops it")
{
    Surfaces made;
    const auto id = made.threeElements();

    made.session.useNoAudioDevice();
    made.session.startPlayback();
    REQUIRE (made.session.isPlaying());

    REQUIRE (made.suggestions.audition (id));

    REQUIRE (made.session.isPlaying());
    REQUIRE_THAT (made.session.track (made.keys).volumeDb, WithinAbs (proposedLevelDb, 1e-6));

    made.suggestions.toggleAB();

    REQUIRE (made.session.isPlaying());
    REQUIRE_FALSE (made.suggestions.hearingProposed());
    REQUIRE_THAT (made.session.track (made.keys).volumeDb, WithinAbs (-6.0, 1e-6));

    made.suggestions.toggleAB();

    REQUIRE (made.session.isPlaying());
    REQUIRE_THAT (made.session.track (made.keys).volumeDb, WithinAbs (proposedLevelDb, 1e-6));

    made.suggestions.stopAudition();
    REQUIRE (made.session.isPlaying());
}

TEST_CASE ("cherry-pick, worked: the unticked Element is neither heard nor applied, and what is "
           "left is one Action")
{
    Surfaces made;
    const auto id = made.threeElements();
    const auto before = made.session.stateDigest();
    const auto undoDepth = made.session.undoNames().size();

    made.suggestions.setChecked (id, 1, false);

    // The copy is the second row, and unticked it is not among what is heard.
    REQUIRE (made.suggestions.audition (id));
    REQUIRE (made.clipCount() == 1);
    REQUIRE_THAT (made.session.track (made.keys).volumeDb, WithinAbs (proposedLevelDb, 1e-6));

    REQUIRE (made.suggestions.accept (id));

    REQUIRE (made.clipCount() == 1);
    REQUIRE_THAT (made.session.track (made.keys).volumeDb, WithinAbs (proposedLevelDb, 1e-6));
    REQUIRE (made.session.undoNames().size() == undoDepth + 1);

    // What the producer said no to is still on the card, and what they said yes
    // to is one thing a single undo takes back.
    const auto* card = made.suggestions.card (id);

    REQUIRE (card != nullptr);
    REQUIRE (card->elements.size() == 1);
    REQUIRE (card->elements.front().description == "Double the riff at bar 3");

    REQUIRE (made.session.undo());
    REQUIRE (made.session.stateDigest() == before);
}

TEST_CASE ("rejecting with a reason asks once more, carrying the reason and the original request")
{
    Surfaces made;
    const auto id = made.threeElements();
    const auto before = made.session.stateDigest();
    const auto undoDepth = made.session.undoNames().size();
    const auto redoDepth = made.session.redoNames().size();
    const auto asked = made.prompts().size();

    made.suggestions.reject (id, "the riff belongs where it was");

    REQUIRE (made.suggestions.cards().empty());
    REQUIRE (made.prompts().size() == asked + 1);
    REQUIRE (made.prompts().back().find ("the riff belongs where it was") != std::string::npos);
    REQUIRE (made.prompts().back().find ("rework the intro") != std::string::npos);

    // A rejection applies nothing and undoes nothing.
    REQUIRE (made.session.stateDigest() == before);
    REQUIRE (made.session.undoNames().size() == undoDepth);
    REQUIRE (made.session.redoNames().size() == redoDepth);
}

TEST_CASE ("a producer edit that touches a Suggestion's clip marks it stale, and it stays "
           "auditionable")
{
    Surfaces made;
    const auto id = made.threeElements();

    REQUIRE_FALSE (made.suggestions.cards().front().stale);

    made.session.performAction ("Move the riff",
                                [&] (auto& ops) { ops.moveClip (made.riff, 4.0); });
    made.suggestions.refresh();

    REQUIRE (made.suggestions.cards().front().stale);

    // Stale takes away a Suggestion's claim to still fit, and nothing else: it
    // is still heard, and nothing has merged by itself.
    REQUIRE (made.suggestions.audition (id));
    REQUIRE (made.clipCount() == 2);

    made.suggestions.stopAudition();

    REQUIRE (made.suggestions.card (id) != nullptr);
}

TEST_CASE ("the redo control asks the same question against the project as it now stands")
{
    Surfaces made;
    const auto id = made.threeElements();

    made.session.performAction ("Move the riff",
                                [&] (auto& ops) { ops.moveClip (made.riff, 4.0); });
    made.suggestions.refresh();
    REQUIRE (made.suggestions.cards().front().stale);

    const auto asked = made.prompts().size();

    REQUIRE (made.suggestions.redo (id));

    REQUIRE (made.prompts().size() == asked + 1);
    REQUIRE (made.prompts().back().find ("rework the intro") != std::string::npos);
    REQUIRE (made.prompts().back().find ("is now") != std::string::npos);

    // The Suggestion the producer asked again about is gone from the surfaces:
    // it was answered by another.
    REQUIRE (made.suggestions.cards().empty());
}

TEST_CASE ("two Suggestions render their ghosts apart and resolve independently")
{
    Surfaces made;
    const auto first = made.threeElements ("rework the intro", "Rework the intro");

    const auto second = made.turn (
        "sit the pad back",
        "Sit the pad back",
        Json::array ({ element (
            "Pad down to -12 dB",
            Json::array ({ operation ("mixer.set",
                                      { { "trackId", duet::collab::toolId::forTrack (made.pad) },
                                        { "volumeDb", -12.0 } }) })) }));

    REQUIRE (made.suggestions.cards().size() == 2);
    REQUIRE (made.suggestions.card (first)->elements.size() == 3);
    REQUIRE (made.suggestions.card (second)->elements.size() == 1);

    Mixer mixer;
    mixer.setSession (&made.session);
    mixer.setSuggestions (&made.suggestions);

    // Each Suggestion's mark stands on the strip it is about, so the two are
    // told apart by where they are drawn.
    REQUIRE (mixer.ghostFader (made.keys).has_value());
    REQUIRE_THAT (mixer.ghostFader (made.keys)->db, WithinAbs (proposedLevelDb, 1e-9));
    REQUIRE (mixer.ghostFader (made.pad).has_value());
    REQUIRE_THAT (mixer.ghostFader (made.pad)->db, WithinAbs (-12.0, 1e-9));

    // Resolving one says nothing about the other, in every respect.
    REQUIRE (made.suggestions.accept (second));
    made.suggestions.refresh();

    REQUIRE (made.suggestions.card (second) == nullptr);
    REQUIRE_FALSE (mixer.ghostFader (made.pad).has_value());

    REQUIRE (made.suggestions.card (first) != nullptr);
    REQUIRE (made.suggestions.card (first)->elements.size() == 3);
    REQUIRE (mixer.ghostFader (made.keys).has_value());
}
