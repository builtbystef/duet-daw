#include "ProjectToolsHarness.h"

#include <duet/app/Collaborator.h>
#include <duet/collab/SuggestTool.h>
#include <duet/collab/SuggestionManager.h>
#include <duet/model/Session.h>
#include <duet/persistence/Project.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using duet::app::Collaborator;
using duet::collab::Estimate;
using duet::collab::Json;
using duet::collab::OpeningContext;
using duet::collab::RpcOutcome;
using duet::collab::SelectionKind;
using duet::collab::ToolCall;
using duet::gui::CollaboratorPanel;
using duet::gui::EntryKind;
using duet::gui::Suggestions;
using duet::model::BuiltinPlugin;
using duet::model::InputKind;
using duet::model::InputRef;
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

/** How much of a take is run while a run is in flight, and in what steps.

    There is no audio device to push the blocks a take is made of, so the case
    pushes them itself, and the message loop the Task Run reports through gets a
    turn between every one of them: a take with nothing going through it is not
    a take a run could have cut.
*/
constexpr int blocksDuringTheRun = 8;
constexpr double secondsPerBlock = 0.25;
constexpr int msPerBlock = 5;

/** How many of those blocks it may take before the transport reports a playhead
    that has moved: the blocks and the message loop do not run at the same time
    without a device, and the position a transport reports is the one that loop
    last fetched from the graph.
*/
constexpr int playheadAttempts = 10;

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
                             const std::filesystem::path& executable = DUET_SIDECAR_DOUBLE,
                             Collaborator::TrackRendererFor trackRenderer = {})
        : harness (script, scriptArguments, executable),
          bridge (
              *harness,
              panel,
              [this] { return opening; },
              Collaborator::MessageThread { duet::testing::messageThreadMarshal(),
                                            duet::testing::messageThreadPost() },
              std::move (trackRenderer))
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

/** The input of a kind that a session running without audio hardware offers. */
InputRef inputOfKind (const Session& session, InputKind kind)
{
    for (const auto& input : session.availableInputs())
        if (input.kind == kind)
            return input.input;

    return duet::model::noInput;
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
    fixture.bridge.setSession (duet::testing::lent (session), folder.folder());

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

TEST_CASE ("the producer keeps recording while a run is in flight", "[collab]")
{
    const TempProject folder;
    const auto project = openProject (folder);
    auto& session = project->session();

    // A machine with no audio hardware offers no inputs, and a track cannot be
    // armed to record from an input that is not there, so the take is taken
    // without a device deliberately — which is what puts recording in CI
    // (ADR 0006) — and the engine's one-time device rebuild, which would stop
    // the transport itself, is kept out of the way of a stop this case would
    // have to read as the run's doing.
    session.useNoAudioDevice();
    session.suppressDeviceRebuild();

    buildTrack (session);

    TrackRef keys = duet::model::noTrack;

    session.performAction ("Add a track to record onto",
                           [&] (auto& ops) {
                               keys =
                                   ops.createTrack (TrackKind::midi, "Keys", BuiltinPlugin::synth);
                           });

    const auto midiInput = inputOfKind (session, InputKind::midi);
    REQUIRE (midiInput != duet::model::noInput);

    session.setTrackInput (keys, midiInput);
    session.setTrackRecordArmed (keys, true);

    PanelOnService fixture { "run-hang-once" };
    fixture.bridge.setSession (duet::testing::lent (session), folder.folder());

    // Record on settled devices starts the take at once and on devices that have
    // only just gone quiet waits out the rest of the pre-roll, so the case waits
    // for the take rather than assuming either.
    session.startRecording();
    REQUIRE (pumpUntil ([&] { return session.isRecording(); }));

    // Enough of the take run for the transport to have said where its playhead
    // has got to, since a playhead that has never moved cannot show that one
    // kept moving.
    for (int attempt = 0; attempt < playheadAttempts && session.playbackPositionSeconds() <= 0.0;
         ++attempt)
    {
        session.runWithoutAudioDevice (secondsPerBlock);
        pumpMessages (msPerBlock);
    }

    // Something played in before the run, so that what the run must not cut is a
    // take with something in it already.
    session.runWithoutAudioDevice (secondsPerBlock, { { { 0.05, 0.1, 60, 100 } } });
    pumpMessages (msPerBlock);

    const auto reachedBeforeTheRun = session.playbackPositionSeconds();
    REQUIRE (reachedBeforeTheRun > 0.0);

    fixture.ask ("what is in this project?");

    REQUIRE (fixture.panel.taskRunning());

    int sampled = 0;
    int recording = 0;

    for (int block = 0; block < blocksDuringTheRun; ++block)
    {
        session.runWithoutAudioDevice (secondsPerBlock, { { { 0.05, 0.1, 64, 100 } } });
        pumpMessages (msPerBlock);

        ++sampled;

        if (session.isRecording())
            ++recording;
    }

    // Asked between the blocks and not only at the end: a take a run stopped
    // partway and one it never touched both read as stopped once it is over.
    REQUIRE (sampled == blocksDuringTheRun);
    REQUIRE (recording == sampled);

    // The transport is the producer's, not the run's, and it was rolling
    // throughout — the playhead is further on than the run found it.
    REQUIRE (session.playbackPositionSeconds() > reachedBeforeTheRun);
    REQUIRE (fixture.panel.taskRunning());

    session.stopRecording();
    REQUIRE_FALSE (session.isRecording());

    // The take landed whole: what was played in before the run and what was
    // played in while it was in flight are one clip on the armed track.
    const auto clips = session.track (keys).clips;

    REQUIRE (clips.size() == 1);
    REQUIRE (clips.front().holdsMidi);
    REQUIRE (session.notes (clips.front().clip).size()
             == static_cast<std::size_t> (1 + blocksDuringTheRun));

    // And landing it was never the run's business either: the take's own Action
    // left the run where it was.
    REQUIRE (fixture.panel.taskRunning());

    fixture.panel.requestCancel();
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
    fixture.bridge.setSession (duet::testing::lent (session), folder.folder());

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
    fixture.bridge.setSession (duet::testing::lent (session), project.folder());

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

namespace
{
/** A measurement's render, stopped where it stands until the case lets it
    through.

    What `get_track_analysis` does on the Collaborator service's own thread, and
    the only part of answering a tool call that is not the message thread's: the
    real one is inside `Session::renderDetachedTrackToFile` for seconds, holding
    the project. This is that moment held open, so that a project swap can be
    driven into the middle of it — and the project is read on the far side of
    the wait, which is where a project destroyed meanwhile would be read after
    it had gone.
*/
class HeldRender
{
public:
    /** The renderer the Collaborator measures one project with.

        The closure is the project's, and goes when the project does, so what it
        carries records where that happened: a project must be put down on the
        message thread, and the thread that let the last call out of one is not
        that thread.
    */
    [[nodiscard]] duet::collab::TrackRenderer over (Session& session)
    {
        return
            [this, &session, farewell = std::make_shared<Farewell> (&putDownThread)] (
                duet::model::TrackRef, const std::filesystem::path&, const std::function<bool()>&)
        {
            {
                std::unique_lock<std::mutex> lock { mutex };
                arrived = true;
                letThrough.wait (lock, [this] { return released; });
            }

            // The project, on the far side of the wait.
            readAfterwards = session.revision();

            const std::lock_guard<std::mutex> lock { mutex };
            arrived = false;

            // Nothing was rendered: the run that asked has been canceled, which
            // is what a render abandoned between blocks answers.
            return false;
        };
    }

    /** Whether the service thread is inside the render right now. */
    [[nodiscard]] bool isInside() const
    {
        const std::lock_guard<std::mutex> lock { mutex };

        return arrived;
    }

    void release()
    {
        {
            const std::lock_guard<std::mutex> lock { mutex };
            released = true;
        }

        letThrough.notify_all();
    }

    /** What the project said when the render read it on the far side of the
        wait, which is after the producer closed it.
    */
    [[nodiscard]] std::uint64_t readWhenReleased() const
    {
        const std::lock_guard<std::mutex> lock { mutex };

        return readAfterwards;
    }

    /** The thread the project this render belonged to was put down on. */
    [[nodiscard]] std::thread::id putDownOn() const { return putDownThread; }

private:
    /** Writes down where it was destroyed, which is where the project holding
        it was destroyed.
    */
    struct Farewell
    {
        explicit Farewell (std::thread::id* where) : recordIn (where) {}

        ~Farewell() { *recordIn = std::this_thread::get_id(); }

        Farewell (const Farewell&) = delete;
        Farewell (Farewell&&) = delete;
        Farewell& operator= (const Farewell&) = delete;
        Farewell& operator= (Farewell&&) = delete;

        std::thread::id* recordIn;
    };

    mutable std::mutex mutex;
    std::condition_variable letThrough;
    bool arrived = false;
    bool released = false;
    std::uint64_t readAfterwards = 0;
    std::thread::id putDownThread;
};

/** How many endings the conversation holds: a canceled run writes one notice,
    and a failed one writes one failure line.
*/
std::size_t endingsIn (const CollaboratorPanel& panel)
{
    std::size_t endings = 0;

    for (const auto& entry : panel.conversation())
        if (entry.kind == EntryKind::notice || entry.kind == EntryKind::failure)
            ++endings;

    return endings;
}
} // namespace

TEST_CASE ("a project closed while a measurement renders is not read once it has gone", "[collab]")
{
    // The project is put down from inside the message loop here, and it is the
    // last one this case holds: without this the loop would go with it.
    const duet::testing::MessageLoop loop;

    const TempProject folder;
    auto project = duet::persistence::Project::create (folder.folder() / "Project");

    REQUIRE (project != nullptr);

    auto held = project->sessionHandle();
    const std::weak_ptr<Session> watching = held;
    const auto bass = buildTrack (*held);

    HeldRender render;
    const Json calls = Json::array ({ duet::testing::toolCall ("get_track_analysis", bass) });

    PanelOnService fixture { "call-tools",
                             { calls.dump() },
                             DUET_SIDECAR_DOUBLE,
                             [&render] (Session& session) { return render.over (session); } };

    fixture.bridge.setSession (held, folder.folder());
    fixture.ask ("how loud is the bass?");

    // The service thread is inside the render, which is where a measurement
    // spends its seconds.
    REQUIRE (pumpUntil ([&] { return render.isInside(); }, runTimeoutMs));

    // New, Open and Save As all do this, and it returns to the producer at
    // once: nothing here waits for the render, which could not be waited for
    // anyway — taking a render's copy down needs the message loop running, so a
    // message thread waiting on one would be waiting on itself.
    fixture.bridge.setSession (nullptr, {});

    REQUIRE (render.isInside());

    // The producer's own hold on the project goes with the project the swap
    // closed. The render still holds it, so what it is about to read is still
    // there rather than freed underneath it.
    const auto revisionWhenClosed = held->revision();

    project.reset();
    held.reset();

    REQUIRE_FALSE (watching.expired());

    render.release();

    // The run about the project that has gone reaches its ending, and the panel
    // shows one — the cancel the swap asked for, and nothing after it.
    REQUIRE (fixture.settled());
    pumpMessages (settleMs);

    REQUIRE (endingsIn (fixture.panel) == 1);

    // What the render read after the producer had closed the project is the
    // project it entered, still saying what it said.
    REQUIRE (render.readWhenReleased() == revisionWhenClosed);

    // And the project the render was holding is put down once the last call is
    // out of it — on the message thread, which is the only thread that may put
    // one down, and not on the service thread that let the call out.
    REQUIRE (pumpUntil ([&] { return watching.expired(); }, runTimeoutMs));
    REQUIRE (render.putDownOn() == std::this_thread::get_id());
}

TEST_CASE ("a Collaborator that has gone answers no tool call either", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    static_cast<void> (buildTrack (session));

    const Json calls = Json::array ({ duet::testing::toolCall ("list_tracks") });

    duet::testing::RunEnding ending;
    CollaboratorPanel panel;
    const Harness harness { "call-tools", std::vector<std::string> { calls.dump() } };

    {
        // Both hosts stop the service before the Collaborator goes, which is
        // what makes this safe in the shipping path. The service is owned
        // separately in both of them, though, so what a service that was not
        // stopped would dispatch is what this asks: the handler is the
        // Collaborator's own, and it goes with it.
        Collaborator bridge { *harness,
                              panel,
                              [] { return OpeningContext {}; },
                              Collaborator::MessageThread { duet::testing::messageThreadMarshal(),
                                                            duet::testing::messageThreadPost() } };

        bridge.setSession (duet::testing::lent (session), project.folder());
    }

    // The run is the service's own here, there being no Collaborator left to
    // ask for one.
    harness->setTaskRunListener (&ending);
    harness->start();

    REQUIRE (harness->startRun ("what is in this project?", {}).started);
    REQUIRE (pumpUntil ([&] { return ending.hasEnded(); }, runTimeoutMs));

    std::vector<Json> answered;

    for (const auto& report : harness.reports())
        if (report.at ("tag") == "tool")
            answered.push_back (report.at ("payload").at ("response"));

    REQUIRE (answered.size() == 1);
    REQUIRE (answered.at (0).contains ("error"));
    REQUIRE (answered.at (0).at ("error").at ("code") == duet::collab::rpcError::methodNotFound);
}

TEST_CASE ("a project that has gone answers no tool call at all", "[collab]")
{
    const TempProject folder;
    const auto project = openProject (folder);
    auto& session = project->session();

    buildTrack (session);

    PanelOnService fixture { "run-commentary", { "the project is open" } };
    fixture.bridge.setSession (duet::testing::lent (session), folder.folder());

    // The vocabulary is the open project's, and it answers while one is open.
    REQUIRE (fixture.bridge.tools().call (Json { { "tool", "list_tracks" } }).succeeded);

    // Detaching is what New, Open and Save As all do, and what the app rests in
    // when opening a project failed. The tools are destroyed by it, so a
    // registry that still held their handlers would answer this out of freed
    // memory rather than refuse it.
    fixture.bridge.setSession (nullptr, {});

    const auto afterwards = fixture.bridge.tools().call (Json { { "tool", "list_tracks" } });

    REQUIRE_FALSE (afterwards.succeeded);
    REQUIRE (afterwards.error.code == duet::collab::rpcError::unknownTool);
}

TEST_CASE ("an ordinary build keeps no trace of a run at all", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    static_cast<void> (buildTrack (session));

    const Json calls = Json::array ({ duet::testing::toolCall ("list_tracks") });

    PanelOnService fixture { "call-tools", { calls.dump() } };
    fixture.bridge.setSession (duet::testing::lent (session), project.folder());

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

namespace
{
/** One `suggest` call over the fixture project: one Element that moves the
    track's clip, and one that lifts its fader.
*/
Json turnaroundOn (TrackRef track, duet::model::ClipRef clip, double startBar, double db)
{
    return duet::testing::suggestCall (
        "A turnaround at bar " + std::to_string (static_cast<int> (startBar)),
        Json::array (
            { duet::testing::suggestElement (
                  "Move the riff",
                  Json::array ({ Json { { "op", "clip.move" },
                                        { "clipId", duet::collab::toolId::forClip (clip) },
                                        { "startBar", startBar } } })),
              duet::testing::suggestElement (
                  "Lift the bass",
                  Json::array ({ Json { { "op", "mixer.set" },
                                        { "trackId", duet::collab::toolId::forTrack (track) },
                                        { "volumeDb", db } } })) }));
}

/** The clip `buildTrack` put on its track. */
duet::model::ClipRef onlyClip (const Session& session, TrackRef track)
{
    const auto clips = session.track (track).clips;
    REQUIRE (clips.size() == 1);

    return clips.front().clip;
}
} // namespace

TEST_CASE ("a real run's Suggestion arrives as a card and as ghosts, and changes nothing",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bass = buildTrack (session);
    const auto riff = onlyClip (session, bass);

    const Json calls = Json::array (
        { duet::testing::toolCommentary ("Try this."), turnaroundOn (bass, riff, 3.0, -4.0) });

    PanelOnService fixture { "call-tools", { calls.dump() } };
    fixture.bridge.setSession (duet::testing::lent (session), project.folder());

    Suggestions suggestions;
    suggestions.setSource (&fixture.bridge.suggestionSurfaces());

    const auto before = session.stateDigest();
    const auto undoDepth = session.undoNames().size();

    fixture.ask ("give me a turnaround into bar 3");

    REQUIRE (fixture.settled());
    REQUIRE (
        pumpUntil ([&] { return fixture.last().kind == EntryKind::suggestion; }, runTimeoutMs));

    // The card sits in the conversation the producer asked in, and the ghosts
    // stand where the operations put them.
    const auto id = fixture.last().suggestion;

    REQUIRE_FALSE (id.empty());
    REQUIRE (fixture.last().text == "A turnaround at bar 3");

    suggestions.refresh();

    const auto* card = suggestions.card (id);

    REQUIRE (card != nullptr);
    REQUIRE (card->elements.size() == 2);
    REQUIRE (card->elements.front().clips.size() == 1);
    REQUIRE (card->elements.front().clips.front().track == bass);
    REQUIRE (card->elements.back().faders.size() == 1);
    REQUIRE (card->elements.back().faders.front().channel == bass);

    // Nothing about the project moved, and the producer's undo history is where
    // they left it: a Suggestion is data until it is accepted.
    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.undoNames().size() == undoDepth);
}

TEST_CASE ("a Suggestion that arrives while the transport rolls does not interrupt playback",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bass = buildTrack (session);
    const auto riff = onlyClip (session, bass);

    const Json calls = Json::array ({ turnaroundOn (bass, riff, 3.0, -4.0) });

    PanelOnService fixture { "call-tools", { calls.dump() } };
    fixture.bridge.setSession (duet::testing::lent (session), project.folder());

    session.useNoAudioDevice();
    session.startPlayback();
    REQUIRE (session.isPlaying());

    fixture.ask ("give me a turnaround into bar 3");

    REQUIRE (fixture.settled());
    REQUIRE (
        pumpUntil ([&] { return fixture.last().kind == EntryKind::suggestion; }, runTimeoutMs));

    REQUIRE (session.isPlaying());
}

TEST_CASE ("a rejection with a reason is answered in the card the rejected one stood in",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bass = buildTrack (session);
    const auto riff = onlyClip (session, bass);

    // One call list per run: the first Suggestion, then the revision the
    // rejection asks for.
    const Json calls = Json::array ({ Json::array ({ turnaroundOn (bass, riff, 3.0, -4.0) }),
                                      Json::array ({ turnaroundOn (bass, riff, 5.0, -2.0) }) });

    PanelOnService fixture { "call-tools", { calls.dump() } };
    fixture.bridge.setSession (duet::testing::lent (session), project.folder());

    Suggestions suggestions;
    suggestions.setSource (&fixture.bridge.suggestionSurfaces());

    fixture.ask ("give me a turnaround into bar 3");

    REQUIRE (fixture.settled());
    REQUIRE (
        pumpUntil ([&] { return fixture.last().kind == EntryKind::suggestion; }, runTimeoutMs));

    const auto rejected = fixture.last().suggestion;
    const auto entries = fixture.panel.conversation().size();

    suggestions.refresh();
    suggestions.reject (rejected, "bar 3 is too early");

    // Exactly one more run, carrying what the producer said and what they asked
    // in the first place.
    REQUIRE (fixture.settled());
    REQUIRE (
        pumpUntil ([&] { return fixture.last().kind == EntryKind::suggestion; }, runTimeoutMs));

    REQUIRE (fixture.reported ("run.start").size() == 2);

    const auto asked = fixture.reported ("run.start").back().at ("prompt").get<std::string>();

    REQUIRE (asked.find ("bar 3 is too early") != std::string::npos);
    REQUIRE (asked.find ("give me a turnaround into bar 3") != std::string::npos);

    // The revision stands where the rejected card stood: the conversation gains
    // no second card.
    REQUIRE (fixture.panel.conversation().size() == entries);
    REQUIRE (fixture.last().suggestion != rejected);
    REQUIRE (fixture.last().text == "A turnaround at bar 5");

    suggestions.refresh();

    REQUIRE (suggestions.cards().size() == 1);
    REQUIRE (suggestions.card (rejected) == nullptr);

    // And where the rejected one went is the History section's to say: it was
    // not merely dropped, it was answered by another.
    REQUIRE (fixture.panel.history().size() == 1);
    REQUIRE (fixture.panel.history().front().outcome == "asked again");
}

TEST_CASE ("the redo control on a stale Suggestion asks once more against current state",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bass = buildTrack (session);
    const auto riff = onlyClip (session, bass);

    const Json calls = Json::array ({ Json::array ({ turnaroundOn (bass, riff, 3.0, -4.0) }),
                                      Json::array ({ turnaroundOn (bass, riff, 5.0, -2.0) }) });

    PanelOnService fixture { "call-tools", { calls.dump() } };
    fixture.bridge.setSession (duet::testing::lent (session), project.folder());

    Suggestions suggestions;
    suggestions.setSource (&fixture.bridge.suggestionSurfaces());

    fixture.ask ("give me a turnaround into bar 3");

    REQUIRE (fixture.settled());
    REQUIRE (
        pumpUntil ([&] { return fixture.last().kind == EntryKind::suggestion; }, runTimeoutMs));

    const auto stale = fixture.last().suggestion;

    // The producer moves the very clip the Suggestion is about, which is what
    // makes the card and its ghosts stale.
    session.performAction ("Move the riff", [&] (auto& ops) { ops.moveClip (riff, 6.0); });
    suggestions.refresh();

    REQUIRE (suggestions.card (stale) != nullptr);
    REQUIRE (suggestions.card (stale)->stale);

    REQUIRE (suggestions.redo (stale));

    REQUIRE (fixture.settled());
    REQUIRE (pumpUntil ([&] { return fixture.last().suggestion != stale; }, runTimeoutMs));

    REQUIRE (fixture.reported ("run.start").size() == 2);

    const auto asked = fixture.reported ("run.start").back().at ("prompt").get<std::string>();

    REQUIRE (asked.find ("give me a turnaround into bar 3") != std::string::npos);
    REQUIRE (asked.find ("is now") != std::string::npos);

    suggestions.refresh();

    REQUIRE (suggestions.card (stale) == nullptr);
    REQUIRE (suggestions.cards().size() == 1);
    REQUIRE (fixture.panel.history().size() == 1);
    REQUIRE (fixture.panel.history().front().outcome == "asked again");
}

TEST_CASE ("accepting from the card lands as one Action, and the History says where it went",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bass = buildTrack (session);
    const auto riff = onlyClip (session, bass);

    const Json calls = Json::array ({ turnaroundOn (bass, riff, 3.0, -4.0) });

    PanelOnService fixture { "call-tools", { calls.dump() } };
    fixture.bridge.setSession (duet::testing::lent (session), project.folder());

    Suggestions suggestions;
    suggestions.setSource (&fixture.bridge.suggestionSurfaces());

    const auto before = session.stateDigest();
    const auto undoDepth = session.undoNames().size();

    fixture.ask ("give me a turnaround into bar 3");

    REQUIRE (fixture.settled());
    REQUIRE (
        pumpUntil ([&] { return fixture.last().kind == EntryKind::suggestion; }, runTimeoutMs));

    const auto id = fixture.last().suggestion;

    suggestions.refresh();

    REQUIRE (suggestions.accept (id));

    REQUIRE (session.undoNames().size() == undoDepth + 1);
    REQUIRE (session.track (bass).volumeDb == -4.0);
    REQUIRE (suggestions.cards().empty());

    REQUIRE (fixture.panel.history().size() == 1);
    REQUIRE (fixture.panel.history().front().summary == "A turnaround at bar 3");
    REQUIRE (fixture.panel.history().front().outcome == "accepted");

    // One Action: a single undo takes the whole of it back.
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == before);
}
