#include <duet/collab/SuggestionManager.h>
#include <duet/collab/ToolDispatch.h>
#include <duet/persistence/Project.h>
#include <duet/persistence/ProjectLayout.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using duet::collab::ElementState;
using duet::collab::Json;
using duet::collab::RunStart;
using duet::collab::SuggestionState;
using duet::model::BuiltinPlugin;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** One operation of the edit vocabulary, as a `suggest` call writes it. */
Json operation (const std::string& name, Json fields = Json::object())
{
    Json out = Json::object();
    out["op"] = name;

    for (const auto& field : fields.items())
        out[field.key()] = field.value();

    return out;
}

/** One Element of a `suggest` call. */
Json element (const std::string& description, Json operations)
{
    Json made = Json::object();
    made["description"] = description;
    made["operations"] = std::move (operations);

    return made;
}

/** A mixer Element setting one track's fader. */
Json faderElement (const std::string& description, TrackRef track, double db)
{
    return element (
        description,
        Json::array ({ operation (
            "mixer.set",
            { { "trackId", duet::collab::toolId::forTrack (track) }, { "volumeDb", db } }) }));
}

/** The Duet Loop with a real project under it: the write-tool that makes
    Suggestions against the live project, and the manager that holds them.

    The seam the spec names for this area's mechanics is the manager's own C++
    interface, so the Task Runs are this object's: no socket, no sidecar and no
    language model is involved. What the Suggestions are, though, is the real
    write-tool's work against the real project, because a Suggestion built by
    hand in a test would be one that nothing could have made.
*/
class Loop
{
public:
    explicit Loop (Session& projectSession)
        : writes (projectSession, duet::testing::messageThreadMarshal()),
          manager (projectSession,
                   [this] (const std::string& prompt)
                   {
                       asked.push_back (prompt);

                       return RunStart::accepted ("run-" + std::to_string (asked.size()));
                   })
    {
        writes.addTo (registry);
    }

    duet::collab::SuggestionManager* operator->() { return &manager; }

    /** One whole turn: the producer asks, and the run answers with a
        Suggestion of these Elements. The Suggestion's id comes back.
    */
    std::string turn (const std::string& request, const std::string& summary, Json elements)
    {
        const auto started = manager.ask (request);
        REQUIRE (started.started);

        return answer (started.runId, summary, std::move (elements));
    }

    /** What a run that has already started answers with. */
    std::string answer (const std::string& runId, const std::string& summary, Json elements)
    {
        Json arguments = Json::object();
        arguments["summary"] = summary;
        arguments["elements"] = std::move (elements);

        Json params = Json::object();
        params["runId"] = runId;
        params["tool"] = "suggest";
        params["args"] = std::move (arguments);

        const auto outcome = registry.call (params);
        REQUIRE (outcome.succeeded);

        const auto id = outcome.result.at ("suggestionId").get<std::string>();
        const auto made = writes.suggestion (id);
        REQUIRE (made.has_value());
        REQUIRE (manager.suggested (runId, made.value_or (duet::collab::Suggestion { {}, {} })));

        return id;
    }

    /** The Suggestion an id names. Asking for one that is not there is a
        broken test rather than an assertion about the manager, so it says so
        here instead of at every use.
    */
    [[nodiscard]] duet::collab::SuggestionInfo held (const std::string& id)
    {
        const auto found = manager.suggestion (id);
        REQUIRE (found.has_value());

        return found.value_or (duet::collab::SuggestionInfo {});
    }

    /** Every prompt a run has been started with, in order. */
    [[nodiscard]] const std::vector<std::string>& prompts() const { return asked; }

private:
    duet::collab::ToolRegistry registry;
    duet::collab::SuggestTool writes;
    std::vector<std::string> asked;
    duet::collab::SuggestionManager manager;
};

/** Two tracks with faders to move. */
struct Mix
{
    TrackRef bass = duet::model::noTrack;
    TrackRef pad = duet::model::noTrack;
};

Mix buildMix (Session& session)
{
    Mix mix;

    session.performAction ("Build the mix",
                           [&] (auto& ops)
                           {
                               mix.bass =
                                   ops.createTrack (TrackKind::midi, "Bass", BuiltinPlugin::synth);
                               mix.pad =
                                   ops.createTrack (TrackKind::midi, "Pad", BuiltinPlugin::synth);
                           });

    return mix;
}
} // namespace

TEST_CASE ("two Suggestions are pending at once and resolve independently", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto mix = buildMix (session);
    Loop loop { session };

    const auto first = loop.turn ("make the bass sit back",
                                  "Pull the bass down",
                                  Json::array ({ faderElement ("Bass to -3 dB", mix.bass, -3.0) }));
    const auto second = loop.turn ("the pad is too loud",
                                   "Pull the pad down",
                                   Json::array ({ faderElement ("Pad to -9 dB", mix.pad, -9.0) }));

    REQUIRE (loop->suggestions().size() == 2);
    REQUIRE (loop.held (first).state == SuggestionState::pending);
    REQUIRE (loop.held (second).state == SuggestionState::pending);

    const auto untouched = loop.held (second);

    REQUIRE (loop->accept (first));

    REQUIRE (loop.held (first).state == SuggestionState::accepted);
    REQUIRE (loop.held (first).elements.front() == ElementState::accepted);
    REQUIRE (session.track (mix.bass).volumeDb == -3.0);

    const auto after = loop.held (second);

    REQUIRE (after.state == untouched.state);
    REQUIRE (after.stale == untouched.stale);
    REQUIRE (after.elements == untouched.elements);
    REQUIRE (after.request == untouched.request);
    REQUIRE (after.made.summary == untouched.made.summary);
    REQUIRE (session.track (mix.pad).volumeDb == 0.0);
}

namespace
{
/** Two MIDI clips on one track, each of them something to name. */
struct Clips
{
    TrackRef track = duet::model::noTrack;
    duet::model::ClipRef x = duet::model::noClip;
    duet::model::ClipRef y = duet::model::noClip;
};

Clips buildClips (Session& session)
{
    Clips built;

    session.performAction ("Build the clips",
                           [&] (auto& ops)
                           {
                               built.track =
                                   ops.createTrack (TrackKind::midi, "Keys", BuiltinPlugin::synth);
                               built.x = ops.insertMidiClip (built.track, "X", 0.0, 2.0);
                               built.y = ops.insertMidiClip (built.track, "Y", 4.0, 2.0);
                           });

    return built;
}

/** Where a clip sits now, whatever order the track happens to hold it in. */
double startOf (const Session& session, TrackRef track, duet::model::ClipRef clip)
{
    for (const auto& held : session.track (track).clips)
        if (held.clip == clip)
            return held.startSeconds;

    return -1.0;
}

/** An Element moving one clip to a bar. */
Json moveElement (const std::string& description, duet::model::ClipRef clip, double bar)
{
    return element (description,
                    Json::array ({ operation ("clip.move",
                                              { { "clipId", duet::collab::toolId::forClip (clip) },
                                                { "startBar", bar } }) }));
}
} // namespace

TEST_CASE ("a producer edit makes the Suggestion that names what moved stale, and only it",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto clips = buildClips (session);
    Loop loop { session };

    const auto aboutX = loop.turn ("push X later",
                                   "Move X to bar 3",
                                   Json::array ({ moveElement ("X to bar 3", clips.x, 3.0) }));
    const auto aboutY = loop.turn ("push Y later",
                                   "Move Y to bar 5",
                                   Json::array ({ moveElement ("Y to bar 5", clips.y, 5.0) }));

    REQUIRE_FALSE (loop.held (aboutX).stale);
    REQUIRE_FALSE (loop.held (aboutY).stale);

    session.performAction ("Move X",
                           [&] (auto& ops) { ops.moveClip (clips.x, session.secondsAtBar (7.0)); });

    REQUIRE (loop.held (aboutX).stale);
    REQUIRE_FALSE (loop.held (aboutY).stale);
}

TEST_CASE ("an undo back to what a Suggestion was made against takes its staleness with it",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto clips = buildClips (session);
    Loop loop { session };

    const auto aboutX = loop.turn ("push X later",
                                   "Move X to bar 3",
                                   Json::array ({ moveElement ("X to bar 3", clips.x, 3.0) }));

    session.performAction ("Move X",
                           [&] (auto& ops) { ops.moveClip (clips.x, session.secondsAtBar (7.0)); });

    REQUIRE (loop.held (aboutX).stale);
    REQUIRE (session.undo());
    REQUIRE_FALSE (loop.held (aboutX).stale);
}

TEST_CASE ("a stale Suggestion stays auditionable and applies nothing until it is accepted",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto clips = buildClips (session);
    Loop loop { session };

    const auto aboutX = loop.turn ("push X later",
                                   "Move X to bar 3",
                                   Json::array ({ moveElement ("X to bar 3", clips.x, 3.0) }));

    session.performAction ("Move X",
                           [&] (auto& ops) { ops.moveClip (clips.x, session.secondsAtBar (7.0)); });

    REQUIRE (loop.held (aboutX).stale);

    const auto asTheProducerLeftIt = session.stateDigest();
    const auto atBar7 = session.secondsAtBar (7.0);

    REQUIRE (loop->audition (aboutX));
    REQUIRE (loop->auditioning() == aboutX);
    REQUIRE (startOf (session, clips.track, clips.x) == session.secondsAtBar (3.0));

    loop->stopAudition();

    REQUIRE_FALSE (loop->auditioning().has_value());
    REQUIRE (session.stateDigest() == asTheProducerLeftIt);

    // Nothing else that can happen to the Duet Loop applies it either.
    const auto other = loop.turn ("and lift Y",
                                  "Move Y to bar 9",
                                  Json::array ({ moveElement ("Y to bar 9", clips.y, 9.0) }));
    REQUIRE (loop->reject (other));
    REQUIRE (loop->ask ("what else could I try?").started);

    REQUIRE (session.stateDigest() == asTheProducerLeftIt);
    REQUIRE (startOf (session, clips.track, clips.x) == atBar7);

    REQUIRE (loop->accept (aboutX));

    REQUIRE (startOf (session, clips.track, clips.x) == session.secondsAtBar (3.0));
}

namespace
{
/** Three tracks, so that a Suggestion can carry three Elements that are each
    applicable on their own.
*/
struct Trio
{
    TrackRef bass = duet::model::noTrack;
    TrackRef pad = duet::model::noTrack;
    TrackRef drums = duet::model::noTrack;
};

Trio buildTrio (Session& session)
{
    Trio trio;

    session.performAction ("Build the trio",
                           [&] (auto& ops)
                           {
                               trio.bass =
                                   ops.createTrack (TrackKind::midi, "Bass", BuiltinPlugin::synth);
                               trio.pad =
                                   ops.createTrack (TrackKind::midi, "Pad", BuiltinPlugin::synth);
                               trio.drums = ops.createTrack (TrackKind::audio, "Drums");
                           });

    return trio;
}

/** The three-Element Suggestion the cherry-pick criteria are worked with. */
Json threeFaders (const Trio& trio)
{
    return Json::array ({ faderElement ("Bass to -3 dB", trio.bass, -3.0),
                          faderElement ("Pad to -9 dB", trio.pad, -9.0),
                          faderElement ("Drums to -6 dB", trio.drums, -6.0) });
}
} // namespace

TEST_CASE ("accepting and rejecting Elements leaves the rest of the Suggestion pending", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto trio = buildTrio (session);
    Loop loop { session };

    const auto id = loop.turn ("balance these three", "Balance the three", threeFaders (trio));
    const auto historyBefore = session.undoNames().size();

    REQUIRE (loop->acceptElement (id, 0));

    REQUIRE (session.track (trio.bass).volumeDb == -3.0);
    REQUIRE (session.undoNames().size() == historyBefore + 1);
    REQUIRE (session.undoNames().front() == "Bass to -3 dB");

    REQUIRE (loop->rejectElement (id, 1));

    REQUIRE (session.track (trio.pad).volumeDb == 0.0);
    REQUIRE (session.undoNames().size() == historyBefore + 1);

    const auto midway = loop.held (id);

    REQUIRE (midway.state == SuggestionState::pending);
    REQUIRE (
        midway.elements
        == std::vector { ElementState::accepted, ElementState::rejected, ElementState::pending });

    REQUIRE (loop->acceptElement (id, 2));

    REQUIRE (session.track (trio.drums).volumeDb == -6.0);
    REQUIRE (loop.held (id).state == SuggestionState::accepted);
}

TEST_CASE ("accepting one Element lands as exactly one Action", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto trio = buildTrio (session);
    Loop loop { session };

    const auto id = loop.turn ("balance these three", "Balance the three", threeFaders (trio));
    const auto before = session.stateDigest();
    const auto historyBefore = session.undoNames().size();

    REQUIRE (loop->acceptElement (id, 0));
    REQUIRE (session.undoNames().size() == historyBefore + 1);

    REQUIRE (session.undo());

    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.undoNames().size() == historyBefore);
}

TEST_CASE ("accepting a whole Suggestion is one Action and rejecting one applies nothing",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto trio = buildTrio (session);
    Loop loop { session };

    const auto accepted =
        loop.turn ("balance these three", "Balance the three", threeFaders (trio));
    const auto before = session.stateDigest();
    const auto historyBefore = session.undoNames().size();

    REQUIRE (loop->accept (accepted));

    REQUIRE (session.track (trio.bass).volumeDb == -3.0);
    REQUIRE (session.track (trio.pad).volumeDb == -9.0);
    REQUIRE (session.track (trio.drums).volumeDb == -6.0);
    REQUIRE (session.undoNames().size() == historyBefore + 1);
    REQUIRE (session.undoNames().front() == "Balance the three");
    REQUIRE (loop.held (accepted).elements == std::vector (3, ElementState::accepted));

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == before);

    const auto rejected = loop.turn ("try again", "Balance them differently", threeFaders (trio));

    REQUIRE (loop->reject (rejected));

    REQUIRE (loop.held (rejected).state == SuggestionState::rejected);
    REQUIRE (loop.held (rejected).elements == std::vector (3, ElementState::rejected));
    REQUIRE (session.stateDigest() == before);
}

TEST_CASE ("replying to a pending Suggestion supersedes it and the revision arrives pending",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto mix = buildMix (session);
    Loop loop { session };

    const auto first = loop.turn ("make the bass sit back",
                                  "Pull the bass down",
                                  Json::array ({ faderElement ("Bass to -3 dB", mix.bass, -3.0) }));

    const auto revision = loop->reply (first, "that is too far, and it is the pad I meant");

    REQUIRE (revision.started);
    REQUIRE (loop.held (first).state == SuggestionState::superseded);
    REQUIRE (loop.prompts().size() == 2);
    REQUIRE (loop.prompts().back().find ("make the bass sit back") != std::string::npos);
    REQUIRE (loop.prompts().back().find ("that is too far, and it is the pad I meant")
             != std::string::npos);

    const auto second =
        loop.answer (revision.runId,
                     "Pull the pad down",
                     Json::array ({ faderElement ("Pad to -3 dB", mix.pad, -3.0) }));

    const auto revised = loop.held (second);

    REQUIRE (revised.state == SuggestionState::pending);
    REQUIRE (revised.revises == first);
    REQUIRE (revised.request == "make the bass sit back");
}

TEST_CASE ("replying to a rejected Suggestion leaves it rejected and yields a new pending one",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto mix = buildMix (session);
    Loop loop { session };

    const auto first = loop.turn ("make the bass sit back",
                                  "Pull the bass down",
                                  Json::array ({ faderElement ("Bass to -3 dB", mix.bass, -3.0) }));

    REQUIRE (loop->reject (first));

    const auto revision = loop->reply (first, "duck it under the kick instead");

    REQUIRE (revision.started);
    REQUIRE (loop.held (first).state == SuggestionState::rejected);
    REQUIRE (loop.prompts().back().find ("duck it under the kick instead") != std::string::npos);

    const auto second =
        loop.answer (revision.runId,
                     "Pull the pad down",
                     Json::array ({ faderElement ("Pad to -3 dB", mix.pad, -3.0) }));

    REQUIRE (loop.held (second).state == SuggestionState::pending);
    REQUIRE (loop.held (second).revises == first);
}

TEST_CASE ("redoing a stale Suggestion supersedes it and asks once against current state",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto clips = buildClips (session);
    Loop loop { session };

    const auto first = loop.turn ("push X later",
                                  "Move X to bar 3",
                                  Json::array ({ moveElement ("X to bar 3", clips.x, 3.0) }));

    session.performAction ("Move X",
                           [&] (auto& ops) { ops.moveClip (clips.x, session.secondsAtBar (7.0)); });

    REQUIRE (loop.held (first).stale);

    const auto again = loop->redo (first);

    REQUIRE (again.started);
    REQUIRE (loop.prompts().size() == 2);
    REQUIRE (loop.held (first).state == SuggestionState::superseded);
    REQUIRE (loop.prompts().back().find ("push X later") != std::string::npos);
    REQUIRE (loop.prompts().back().find (duet::collab::toolId::forClip (clips.x))
             != std::string::npos);
    REQUIRE (loop.prompts().back().find ("bar 7.000") != std::string::npos);

    const auto second = loop.answer (
        again.runId, "Move X to bar 9", Json::array ({ moveElement ("X to bar 9", clips.x, 9.0) }));

    REQUIRE (loop.held (second).state == SuggestionState::pending);
    REQUIRE (loop.held (second).revises == first);
    REQUIRE (loop.held (second).request == "push X later");
}

TEST_CASE ("a resolved Suggestion cannot be resolved again", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto trio = buildTrio (session);
    Loop loop { session };

    const auto turnedDown = loop.turn ("balance these", "Balance the three", threeFaders (trio));

    REQUIRE (loop->reject (turnedDown));

    const auto after = session.stateDigest();

    REQUIRE_FALSE (loop->accept (turnedDown));
    REQUIRE_FALSE (loop->acceptElement (turnedDown, 0));
    REQUIRE_FALSE (loop->rejectElement (turnedDown, 0));
    REQUIRE_FALSE (loop->reject (turnedDown));
    REQUIRE_FALSE (loop->audition (turnedDown));
    REQUIRE_FALSE (loop->redo (turnedDown).started);

    REQUIRE (loop.held (turnedDown).state == SuggestionState::rejected);
    REQUIRE (session.stateDigest() == after);

    const auto replaced = loop.turn ("balance these", "Balance them again", threeFaders (trio));

    REQUIRE (loop->reply (replaced, "not like that").started);
    REQUIRE (loop.held (replaced).state == SuggestionState::superseded);

    REQUIRE_FALSE (loop->accept (replaced));
    REQUIRE_FALSE (loop->acceptElement (replaced, 0));
    REQUIRE_FALSE (loop->audition (replaced));
    REQUIRE_FALSE (loop->reply (replaced, "nor like that").started);

    REQUIRE (loop.held (replaced).state == SuggestionState::superseded);
    REQUIRE (session.stateDigest() == after);
}

TEST_CASE ("rejecting leaves the project and both histories as the Suggestion found them",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto trio = buildTrio (session);

    session.performAction ("Name the bus",
                           [&] (auto& ops) { ops.renameTrack (trio.drums, "Kit"); });
    REQUIRE (session.undo());

    Loop loop { session };

    const auto digestBefore = session.stateDigest();
    const auto undoBefore = session.undoNames();
    const auto redoBefore = session.redoNames();

    REQUIRE (redoBefore.size() == 1);

    const auto id = loop.turn ("balance these", "Balance the three", threeFaders (trio));

    REQUIRE (loop->audition (id));
    loop->stopAudition();
    REQUIRE (loop->reject (id));

    REQUIRE (session.stateDigest() == digestBefore);
    REQUIRE (session.undoNames() == undoBefore);
    REQUIRE (session.redoNames() == redoBefore);
}

namespace
{
std::string contentsOf (const std::filesystem::path& file)
{
    const std::ifstream input { file };
    std::ostringstream contents;
    contents << input.rdbuf();

    return std::move (contents).str();
}
} // namespace

TEST_CASE ("Suggestions and the conversation reach no project file and no next launch", "[collab]")
{
    const TempProject temporary;
    const auto folder = temporary.folder() / "Nocturne";

    const std::string request = "the low end is fighting itself";
    const std::string reason = "not the pad, it is the bass that has to move";
    const std::string summary = "Duck the bass under the kick";
    const std::string revised = "Carve the bass instead";
    const std::string description = "Bass to -4 dB";

    std::string saved;

    {
        const auto project = duet::persistence::Project::create (folder);
        REQUIRE (project != nullptr);

        auto& session = project->session();
        const auto mix = buildMix (session);

        REQUIRE (project->save());

        const auto beforeAnythingWasAsked = session.stateDigest();
        Loop loop { session };

        const auto first = loop.turn (
            request, summary, Json::array ({ faderElement (description, mix.bass, -4.0) }));

        const auto again = loop->reply (first, reason);
        REQUIRE (again.started);

        const auto second = loop.answer (
            again.runId, revised, Json::array ({ faderElement ("Bass to -2 dB", mix.bass, -2.0) }));

        REQUIRE (loop.held (second).state == SuggestionState::pending);
        REQUIRE (project->save());
        REQUIRE (session.stateDigest() == beforeAnythingWasAsked);

        saved = contentsOf (duet::persistence::editFile (folder));
    }

    for (const auto& said : { request, reason, summary, revised, description })
        REQUIRE (saved.find (said) == std::string::npos);

    const auto reopened = duet::persistence::Project::open (folder);
    REQUIRE (reopened != nullptr);

    Loop next { reopened->session() };

    REQUIRE (next->suggestions().empty());
}

TEST_CASE ("cherry-picked Elements that each create something keep their creations apart",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    buildMix (session);
    Loop loop { session };

    const auto id = loop.turn (
        "give me two more voices",
        "Add a pad and a lead",
        Json::array ({ element ("Add a warm pad",
                                Json::array ({ operation ("track.create",
                                                          { { "kind", "midi" },
                                                            { "name", "Pad" },
                                                            { "instrument", "synth" },
                                                            { "ref", "#voice" } }),
                                               operation ("track.rename",
                                                          { { "trackId", "#voice" },
                                                            { "name", "Warm Pad" } }) })),
                       element ("Add a lead",
                                Json::array ({ operation ("track.create",
                                                          { { "kind", "midi" },
                                                            { "name", "Lead" },
                                                            { "instrument", "synth" },
                                                            { "ref", "#voice" } }),
                                               operation ("track.rename",
                                                          { { "trackId", "#voice" },
                                                            { "name", "Bright Lead" } }) })) }));

    const auto historyBefore = session.undoNames().size();
    const auto tracksBefore = session.tracks().size();

    REQUIRE (loop->accept (id));
    REQUIRE (session.undoNames().size() == historyBefore + 1);

    // Each Element's placeholder is its own, so the two renames land on the two
    // tracks that made them rather than both on one of them.
    const auto tracks = session.tracks();
    std::vector<std::string> added;

    for (auto at = tracksBefore; at < tracks.size(); ++at)
        added.push_back (tracks.at (at).name);

    REQUIRE (added == std::vector<std::string> { "Warm Pad", "Bright Lead" });
}

TEST_CASE ("accepting a chosen few Elements is one Action and leaves the rest pending", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto trio = buildTrio (session);
    Loop loop { session };

    const auto id = loop.turn ("balance these three", "Balance the three", threeFaders (trio));
    const auto before = session.stateDigest();
    const auto historyBefore = session.undoNames().size();

    // The producer unticked the middle row: what Accept applies is the first
    // and the third, and it is still one thing on the undo history.
    REQUIRE (loop->accept (id, { 0, 2 }));

    REQUIRE (session.track (trio.bass).volumeDb == -3.0);
    REQUIRE (session.track (trio.drums).volumeDb == -6.0);
    REQUIRE (session.track (trio.pad).volumeDb == 0.0);
    REQUIRE (session.undoNames().size() == historyBefore + 1);
    REQUIRE (session.undoNames().front() == "Balance the three");

    const auto midway = loop.held (id);

    REQUIRE (midway.state == SuggestionState::pending);
    REQUIRE (
        midway.elements
        == std::vector { ElementState::accepted, ElementState::pending, ElementState::accepted });

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == before);
}

TEST_CASE ("auditioning a chosen few Elements hears those and nothing else", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto trio = buildTrio (session);
    Loop loop { session };

    const auto id = loop.turn ("balance these three", "Balance the three", threeFaders (trio));
    const auto before = session.stateDigest();

    REQUIRE (loop->audition (id, { 0, 2 }));

    REQUIRE (session.track (trio.bass).volumeDb == -3.0);
    REQUIRE (session.track (trio.drums).volumeDb == -6.0);
    REQUIRE (session.track (trio.pad).volumeDb == 0.0);

    loop->stopAudition();

    REQUIRE (session.stateDigest() == before);
}
