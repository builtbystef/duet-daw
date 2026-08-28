#include "ProjectToolsHarness.h"

#include <duet/app/Collaborator.h>
#include <duet/collab/SuggestTool.h>
#include <duet/collab/SuggestionManager.h>
#include <duet/model/Session.h>
#include <duet/persistence/Project.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using duet::app::Collaborator;
using duet::collab::Estimate;
using duet::collab::Json;
using duet::collab::OpeningContext;
using duet::collab::RpcOutcome;
using duet::collab::RunStart;
using duet::collab::SelectionKind;
using duet::collab::ToolCall;
using duet::gui::CollaboratorPanel;
using duet::gui::EntryKind;
using duet::model::BuiltinPlugin;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::Harness;
using duet::testing::pumpMessages;
using duet::testing::pumpUntil;
using duet::testing::TempProject;

namespace
{
/** How long a run driven through a real socket and a real child process is
    given: long enough to spawn the double on a loaded machine.
*/
constexpr int runTimeoutMs = 20000;

/** How much message loop is enough for something that was going to arrive to
    have arrived, when the assertion is that nothing did.
*/
constexpr int settleMs = 300;

/** The panel on the real service: a real socket, the test-double sidecar as a
    real child process, and the Collaborator between them.

    Nothing here stands in for anything the shipping path has. What the panel
    shows is what came back over the socket, and the only thing missing is the
    model that would be deciding what to send.
*/
struct PanelOnService
{
    explicit PanelOnService (const std::string& script,
                             const std::vector<std::string>& scriptArguments = {},
                             const std::filesystem::path& executable = DUET_SIDECAR_DOUBLE)
        : harness (script, scriptArguments, executable),
          bridge (
              *harness,
              panel,
              [this] { return opening; },
              Collaborator::MessageThread { duet::testing::messageThreadMarshal(),
                                            duet::testing::messageThreadPost() })
    {
        // The suite asserts about both kinds of build, and only one of them is
        // ever compiled here, so each test says which it is talking about.
        panel.setToolTraceEnabled (true);
        harness->start();
    }

    /** Stops the service before the Collaborator it reports to goes away, which
        is what anything holding the two owes them: the service has a thread and
        the Collaborator is what that thread calls into.
    */
    ~PanelOnService() { harness->stop(); }

    PanelOnService (const PanelOnService&) = delete;
    PanelOnService& operator= (const PanelOnService&) = delete;

    /** Types a message and presses Send, which is the whole of the producer's
        part in starting a Task Run.
    */
    void ask (const std::string& message)
    {
        panel.setComposerText (message);
        panel.send();
    }

    /** Waits for the run to reach its ending, whatever the ending is. */
    bool settled()
    {
        return pumpUntil ([this] { return ! panel.taskRunning(); }, runTimeoutMs);
    }

    /** The last thing in the conversation. */
    [[nodiscard]] const duet::gui::ConversationEntry& last() const
    {
        return panel.conversation().back();
    }

    /** The payloads the double reported under one tag, in order. */
    [[nodiscard]] std::vector<Json> reported (const std::string& tag) const
    {
        std::vector<Json> chosen;

        for (const auto& report : harness.reports())
            if (report.at ("tag") == tag)
                chosen.push_back (report.at ("payload"));

        return chosen;
    }

    /** What the producer had when they pressed Send. */
    OpeningContext opening;

    Harness harness;
    CollaboratorPanel panel;
    Collaborator bridge;
};

/** A producer with two clips selected, at bar 9 beat 1, with the transport
    rolling: the spec's own worked example.
*/
OpeningContext twoClipsAtBarNine()
{
    OpeningContext context;
    context.selection = SelectionKind::clips;
    context.selectionIds = { "clip-3", "clip-4" };
    context.playheadBar = 9;
    context.playheadBeat = 1.0;
    context.transportPlaying = true;

    return context;
}

/** One instrument track with a clip on it: enough project for a tool to read
    and for the transport to have something to roll through.
*/
TrackRef buildTrack (Session& session)
{
    TrackRef made = duet::model::noTrack;

    session.performAction ("Build the project",
                           [&] (auto& ops)
                           {
                               made =
                                   ops.createTrack (TrackKind::midi, "Bass", BuiltinPlugin::synth);
                               ops.insertMidiClip (made, "riff", 0.0, 8.0);
                           });

    return made;
}

/** A real project on disk, opened through the persistence facade, so that a
    save is the save the producer's Save is.
*/
std::unique_ptr<duet::persistence::Project> openProject (const TempProject& folder)
{
    auto project = duet::persistence::Project::create (folder.folder() / "Project");
    REQUIRE (project != nullptr);

    return project;
}
} // namespace

TEST_CASE ("sending a message starts one Task Run, and the composer is held until it ends",
           "[collab]")
{
    PanelOnService fixture { "run-stream" };

    fixture.ask ("something's off in the drop");

    REQUIRE (fixture.panel.taskRunning());
    REQUIRE_FALSE (fixture.panel.composerEnabled());

    // One run at a time: a second Send while one is on starts nothing and says
    // nothing.
    fixture.ask ("and again");

    REQUIRE (fixture.panel.conversation().size() == 1);

    REQUIRE (fixture.settled());
    REQUIRE (fixture.panel.composerEnabled());

    // Exactly one run crossed the seam, and what it said is in the conversation
    // under the message that asked for it.
    REQUIRE (fixture.reported ("run.start").size() == 1);
    REQUIRE (fixture.panel.conversation().size() == 2);
    REQUIRE (fixture.last().kind == EntryKind::commentary);
    REQUIRE (fixture.last().text == "Something is off in the drop.");
}

TEST_CASE ("commentary arrives while the run is still going, not when it finishes", "[collab]")
{
    PanelOnService fixture { "run-stream-hang" };

    fixture.ask ("something's off in the drop");

    REQUIRE (pumpUntil (
        [&]
        {
            return fixture.panel.conversation().size() > 1
                   && fixture.last().text == "Something is off in the drop.";
        },
        runTimeoutMs));

    // The whole of it is readable and the run has not ended: this is what the
    // producer is reading while the Collaborator is still working.
    REQUIRE (fixture.panel.taskRunning());
    REQUIRE (fixture.last().kind == EntryKind::commentary);

    fixture.panel.requestCancel();
}

TEST_CASE ("a run carries what the producer had at the moment they sent it", "[collab]")
{
    PanelOnService fixture { "run-hang-once" };

    fixture.opening = twoClipsAtBarNine();
    fixture.ask ("give me a turnaround into bar 9");

    REQUIRE (fixture.harness.waitForReports (1));

    const auto started = fixture.reported ("run.start");

    REQUIRE (started.size() == 1);

    const auto& opening = started.at (0).at ("openingContext");

    REQUIRE (opening.at ("selection").at ("kind") == "clips");
    REQUIRE (opening.at ("selection").at ("ids") == Json::array ({ "clip-3", "clip-4" }));
    REQUIRE (opening.at ("playhead").at ("bar") == 9);
    REQUIRE (opening.at ("playhead").at ("beat") == 1.0);
    REQUIRE (opening.at ("transportPlaying") == true);

    // The selection moving on is a fact about a moment that has already passed:
    // the run carries what was true when it began and nothing else.
    fixture.opening = OpeningContext {};
    pumpMessages (settleMs);

    REQUIRE (fixture.reported ("run.start").size() == 1);
    REQUIRE (fixture.reported ("run.start").at (0) == started.at (0));

    fixture.panel.requestCancel();
}

TEST_CASE ("the producer keeps playing and editing while a run is in flight", "[collab]")
{
    const TempProject folder;
    const auto project = openProject (folder);
    auto& session = project->session();

    const auto bass = buildTrack (session);

    PanelOnService fixture { "run-hang-once" };
    fixture.bridge.setSession (&session, folder.folder());

    REQUIRE (duet::testing::playUntilRolling (session));

    fixture.ask ("what is in this project?");

    REQUIRE (fixture.panel.taskRunning());

    // The transport is the producer's, not the run's.
    REQUIRE (session.isPlaying());

    session.performAction ("Rename the track", [&] (auto& ops) { ops.renameTrack (bass, "Sub"); });

    REQUIRE (session.track (bass).name == "Sub");

    REQUIRE (project->save());
    REQUIRE (fixture.panel.taskRunning());

    fixture.panel.requestCancel();
    session.stopPlayback();
}

TEST_CASE ("canceling ends the run where it stands, and nothing arrives after it", "[collab]")
{
    PanelOnService fixture { "run-stream-hang" };

    fixture.ask ("something's off in the drop");

    REQUIRE (pumpUntil ([&] { return fixture.panel.conversation().size() > 1; }, runTimeoutMs));

    fixture.panel.requestCancel();

    REQUIRE_FALSE (fixture.panel.taskRunning());
    REQUIRE (fixture.panel.composerEnabled());
    REQUIRE (fixture.last().kind == EntryKind::notice);
    REQUIRE (fixture.last().text == CollaboratorPanel::cancelNotice);

    const auto afterCancel = fixture.panel.conversation().size();

    pumpMessages (settleMs);

    REQUIRE (fixture.panel.conversation().size() == afterCancel);
    REQUIRE (fixture.last().kind == EntryKind::notice);
}

TEST_CASE ("a failed run leaves one plain line and the DAW keeps working", "[collab]")
{
    const TempProject folder;
    const auto project = openProject (folder);
    auto& session = project->session();

    const auto bass = buildTrack (session);

    PanelOnService fixture { "run-fail" };
    fixture.bridge.setSession (&session, folder.folder());

    fixture.ask ("something's off in the drop");

    REQUIRE (fixture.settled());
    REQUIRE (fixture.panel.conversation().size() == 2);
    REQUIRE (fixture.last().kind == EntryKind::failure);
    REQUIRE (fixture.last().text == "the provider refused the request");

    // Nothing queues and nothing retries: the failure is one line, and the next
    // run is the producer's to ask for.
    pumpMessages (settleMs);

    REQUIRE (fixture.panel.conversation().size() == 2);
    REQUIRE (fixture.reported ("run.start").size() == 1);
    REQUIRE (fixture.panel.composerEnabled());

    // Saving, playing and editing are all exactly as they were.
    session.performAction ("Rename the track", [&] (auto& ops) { ops.renameTrack (bass, "Sub"); });

    REQUIRE (session.track (bass).name == "Sub");
    REQUIRE (project->save());
    REQUIRE (duet::testing::playUntilRolling (session));

    session.stopPlayback();
}

TEST_CASE ("a Collaborator with nothing behind it fails the run and nothing else", "[collab]")
{
    // A sidecar that is not there is every kind of backend trouble at once, and
    // the panel has one answer for all of them: one line, and no offline state
    // to be in.
    PanelOnService fixture { "obey", {}, "/no/such/sidecar" };

    fixture.ask ("something's off in the drop");

    REQUIRE (fixture.settled());
    REQUIRE (fixture.panel.conversation().size() == 2);
    REQUIRE (fixture.last().kind == EntryKind::failure);
    REQUIRE (fixture.panel.composerEnabled());

    // And the next message is asked and answered the same way, there being no
    // queue and no state to come out of.
    fixture.ask ("try again");

    REQUIRE (fixture.settled());
    REQUIRE (fixture.panel.conversation().size() == 4);
    REQUIRE (fixture.last().kind == EntryKind::failure);
}

TEST_CASE ("what a run said after it was handed a guess is marked, and the mark is the ledger",
           "[collab]")
{
    const Json calls = Json::array (
        { duet::testing::toolCommentary ("It reads as "),
          duet::testing::toolCall ("estimate_audio_content", Json { { "trackId", "track-3" } }),
          duet::testing::toolCommentary ("C major.") });

    PanelOnService fixture { "call-tools", { calls.dump() } };

    // A stand-in for the estimating tool, and the one thing about it that
    // matters here: wrapping a guess and writing it into the run's ledger are
    // one act, so nothing can hand one over unrecorded. What the real routine
    // answers is issue 2z0y5u's.
    fixture.bridge.tools().add (
        "estimate_audio_content",
        [&fixture] (const ToolCall& call)
        {
            return RpcOutcome::success (
                Json { { "key",
                         fixture.bridge.estimateLedger().record (
                             call.runId,
                             "estimate_audio_content",
                             "key",
                             Estimate { "C major", "krumhansl-schmuckler", 0.72 }) } });
        });

    fixture.ask ("what key is this in?");

    REQUIRE (fixture.settled());
    REQUIRE (fixture.last().kind == EntryKind::commentary);
    REQUIRE (fixture.last().text == "It reads as C major.");

    // The mark is the ledger: opening it is what says which guess, made by what,
    // and how far the routine trusted itself.
    REQUIRE (fixture.last().estimates.size() == 1);
    REQUIRE (fixture.last().estimates.front().field == "key");
    REQUIRE (fixture.last().estimates.front().value == "C major");
    REQUIRE (fixture.last().estimates.front().method == "krumhansl-schmuckler");
    REQUIRE (fixture.last().estimates.front().confidence == 0.72);
}

TEST_CASE ("a run that was handed no guess carries no mark, and its tools read the real project",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bass = buildTrack (session);

    const Json calls = Json::array ({ duet::testing::toolCall ("list_tracks"),
                                      duet::testing::toolCommentary ("One track, called Bass.") });

    PanelOnService fixture { "call-tools", { calls.dump() } };
    fixture.bridge.setSession (&session, project.folder());

    fixture.ask ("what is in this project?");

    REQUIRE (fixture.settled());
    REQUIRE (fixture.last().kind == EntryKind::commentary);
    REQUIRE (fixture.last().estimates.empty());

    // The development trace is the only place the arguments and the results of
    // a call exist, and it is what a developer debugging a run reads.
    REQUIRE (pumpUntil ([&] { return ! fixture.panel.toolTrace().empty(); }, runTimeoutMs));
    REQUIRE (fixture.panel.toolTrace().size() == 1);

    const auto& traced = fixture.panel.toolTrace().front();

    REQUIRE (traced.tool == "list_tracks");
    REQUIRE (traced.arguments == "{}");

    const auto answered = Json::parse (traced.result, nullptr, false);

    REQUIRE_FALSE (answered.is_discarded());
    REQUIRE (duet::testing::trackEntry (answered, bass).at ("name") == "Bass");
}

TEST_CASE ("an ordinary build keeps no trace of a run at all", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    static_cast<void> (buildTrack (session));

    const Json calls = Json::array ({ duet::testing::toolCall ("list_tracks") });

    PanelOnService fixture { "call-tools", { calls.dump() } };
    fixture.bridge.setSession (&session, project.folder());

    // What the Target Producer's build does: the call is answered, and nothing
    // about it is kept for anyone to read.
    fixture.panel.setToolTraceEnabled (false);
    fixture.ask ("what is in this project?");

    REQUIRE (fixture.settled());
    pumpMessages (settleMs);

    REQUIRE (fixture.panel.toolTrace().empty());
    REQUIRE (fixture.reported ("tool").size() == 1);
    REQUIRE (fixture.reported ("tool").at (0).at ("response").contains ("result"));
}

TEST_CASE ("the History section is the Suggestions the producer has finished with", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bass = buildTrack (session);

    PanelOnService fixture { "run-echo" };

    duet::collab::ToolRegistry writes;
    duet::collab::SuggestTool suggest { session, duet::testing::messageThreadMarshal() };
    suggest.addTo (writes);

    // A run makes at most one Suggestion, so each ask is its own run.
    int runsAsked = 0;
    duet::collab::SuggestionManager manager { session, [&runsAsked] (const std::string&) {
                                                 return RunStart::accepted (
                                                     "run-" + std::to_string (++runsAsked));
                                             } };

    const auto made = [&] (const std::string& summary, double db)
    {
        const auto started = manager.ask (summary);
        REQUIRE (started.started);

        Json arguments = Json::object();
        arguments["summary"] = summary;
        arguments["elements"] = Json::array (
            { Json { { "description", "Set the fader" },
                     { "operations",
                       Json::array ({ Json { { "op", "mixer.set" },
                                             { "trackId", duet::collab::toolId::forTrack (bass) },
                                             { "volumeDb", db } } }) } } });

        const auto outcome = writes.call (Json {
            { "runId", started.runId }, { "tool", "suggest" }, { "args", std::move (arguments) } });
        REQUIRE (outcome.succeeded);

        const auto id = outcome.result.at ("suggestionId").get<std::string>();
        const auto held = suggest.suggestion (id);
        REQUIRE (held.has_value());
        REQUIRE (
            manager.suggested (started.runId, held.value_or (duet::collab::Suggestion { {}, {} })));

        return id;
    };

    const auto accepted = made ("Bring the bass down", -6.0);
    const auto rejected = made ("Push the bass up", 3.0);

    fixture.bridge.setSuggestions (&manager);

    // Nothing is finished with yet, so there is no History to show.
    REQUIRE (fixture.panel.history().empty());

    REQUIRE (manager.accept (accepted));
    REQUIRE (manager.reject (rejected));

    fixture.bridge.refreshHistory();

    REQUIRE (fixture.panel.history().size() == 2);
    REQUIRE (fixture.panel.history().front().summary == "Bring the bass down");
    REQUIRE (fixture.panel.history().front().outcome == "accepted");
    REQUIRE (fixture.panel.history().back().summary == "Push the bass up");
    REQUIRE (fixture.panel.history().back().outcome == "rejected");
}
