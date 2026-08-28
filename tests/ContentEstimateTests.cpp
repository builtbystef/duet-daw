#include "ProjectToolsHarness.h"

#include <duet/collab/Analysis.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/** `estimate_audio_content`, the Estimate wrapper, and the estimate ledger,
    asserted across the socket.

    The seam is the spec's primary one — a real service, a real socket, the
    test-double sidecar as a real child process, and a real project — because
    what this slice adds is a tool and the mark that follows from calling it,
    and both of those are things that cross the wire. What the routines
    themselves read out of a waveform is asserted where the spec puts that:
    HarmonyRoutinesTests, over signals built by hand.
*/

using duet::collab::Json;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::isAnEstimate;
using duet::testing::TempProject;
using duet::testing::toolCall;
using duet::testing::toolCommentary;
using duet::testing::ToolRun;
using duet::testing::ToolRunOptions;

namespace
{
/** A project of one audio track carrying a chord progression, at a tempo that
    makes a bar two seconds long, so that a chord and a bar are the same thing.
*/
struct ProgressionProject
{
    /** Each entry is one bar's chord, as the MIDI pitches it is made of. */
    explicit ProgressionProject (const std::vector<std::vector<int>>& chords)
    {
        const auto progression = project.writeChords ("progression.wav", secondsPerBar, chords);

        session.useNoAudioDevice();
        session.suppressDeviceRebuild();

        session.performAction ("Build the example",
                               [&] (auto& ops)
                               {
                                   ops.setTempo (120.0);
                                   ops.setTimeSignature (4, 4);
                                   track = ops.createTrack (TrackKind::audio, "Keys");
                                   other = ops.createTrack (TrackKind::midi, "Elsewhere");
                                   ops.insertAudioClip (track,
                                                        "progression",
                                                        progression,
                                                        0.0,
                                                        secondsPerBar
                                                            * static_cast<double> (chords.size()));
                               });
    }

    /** The project's own render path, with a tally of how often it was asked. */
    [[nodiscard]] duet::collab::TrackRenderer countingRenderer()
    {
        return [this] (TrackRef renderedTrack,
                       const std::filesystem::path& destination,
                       const std::function<bool()>& keepGoing)
        {
            ++renders;

            return duet::collab::offlineTrackRenderer (session) (
                renderedTrack, destination, keepGoing);
        };
    }

    [[nodiscard]] ToolRunOptions options()
    {
        ToolRunOptions made;
        made.measured = &measured;
        made.estimated = &estimates;
        made.ledger = &ledger;

        return made;
    }

    static constexpr double secondsPerBar = 2.0;

    TempProject project;
    Session session { project.editFile() };
    TrackRef track = duet::model::noTrack;
    TrackRef other = duet::model::noTrack;
    std::atomic<int> renders { 0 };
    duet::collab::EstimateLedger ledger;
    duet::collab::TrackRenders rendered { countingRenderer(), project.folder() };
    duet::collab::TrackAnalysis measured { session,
                                           duet::testing::messageThreadMarshal(),
                                           rendered };
    duet::collab::ContentEstimates estimates { session,
                                               duet::testing::messageThreadMarshal(),
                                               rendered,
                                               ledger };
};

/** The chords of the progression the criteria are worked on: C major, then G
    major, one to a bar.
*/
std::vector<std::vector<int>> cThenG() { return { { 60, 64, 67 }, { 67, 71, 74 } }; }

/** A whole cadence, I–IV–V–I in C major, which is what it takes to name a key:
    two chords are the notes of several keys and the routine says so, where four
    of them leave one.
*/
std::vector<std::vector<int>> cadence()
{
    return { { 60, 64, 67 }, { 65, 69, 72 }, { 67, 71, 74 }, { 60, 64, 67 } };
}

Json estimateCall (TrackRef track) { return toolCall ("estimate_audio_content", track); }

Json estimateCall (TrackRef track, const Json& aspects)
{
    Json arguments = Json::object();
    arguments["trackId"] = duet::collab::toolId::forTrack (track);
    arguments["aspects"] = aspects;

    return toolCall ("estimate_audio_content", arguments);
}

/** The chord this result names at that bar, and empty when it names none. */
std::string chordAtBar (const Json& result, int bar)
{
    for (const auto& entry : result.at ("chords").at ("value"))
        if (entry.at ("bar") == bar)
            return entry.at ("chord").get<std::string>();

    return {};
}
} // namespace

TEST_CASE ("a rendered progression's key crosses wrapped, and never bare", "[collab]")
{
    ProgressionProject fixture { cadence() };

    const ToolRun run { fixture.session,
                        Json::array ({ estimateCall (fixture.track, Json::array ({ "key" })) }),
                        fixture.options() };

    REQUIRE (run.finished());

    const auto& key = run.result (0).at ("key");

    REQUIRE (isAnEstimate (key));
    REQUIRE (key.at ("value") == "C major");
    REQUIRE (key.at ("method") == duet::collab::analysis::keyMethod);
    REQUIRE (key.at ("confidence").get<double>() > 0.0);
}

TEST_CASE ("a rendered two-bar progression names a chord at each bar, wrapped", "[collab]")
{
    ProgressionProject fixture { cThenG() };

    const ToolRun run { fixture.session,
                        Json::array ({ estimateCall (fixture.track, Json::array ({ "chords" })) }),
                        fixture.options() };

    REQUIRE (run.finished());

    const auto& chords = run.result (0).at ("chords");

    REQUIRE (isAnEstimate (chords));
    REQUIRE (chords.at ("value").size() == 2);
    REQUIRE (chordAtBar (run.result (0), 1) == "C major");
    REQUIRE (chordAtBar (run.result (0), 2) == "G major");
}

TEST_CASE ("a bar range names the chords of those bars alone", "[collab]")
{
    ProgressionProject fixture { { { 60, 64, 67 }, { 67, 71, 74 }, { 57, 60, 64 } } };

    Json arguments = Json::object();
    arguments["trackId"] = duet::collab::toolId::forTrack (fixture.track);
    arguments["barRange"] = Json::array ({ 2, 3 });
    arguments["aspects"] = Json::array ({ "chords" });

    const ToolRun run { fixture.session,
                        Json::array ({ toolCall ("estimate_audio_content", arguments) }),
                        fixture.options() };

    REQUIRE (run.finished());

    const auto& chords = run.result (0).at ("chords").at ("value");

    REQUIRE (chords.size() == 2);
    REQUIRE (chordAtBar (run.result (0), 2) == "G major");
    REQUIRE (chordAtBar (run.result (0), 3) == "A minor");
}

TEST_CASE ("the aspects argument is what is worked out, and nothing else", "[collab]")
{
    ProgressionProject fixture { cThenG() };

    const ToolRun run { fixture.session,
                        Json::array (
                            { estimateCall (fixture.track, Json::array ({ "key" })),
                              estimateCall (fixture.track, Json::array ({ "chords" })),
                              estimateCall (fixture.track),
                              estimateCall (fixture.track, Json::array ({ "instrument" })) }),
                        fixture.options() };

    REQUIRE (run.finished());

    // An aspect that was not asked for is absent, rather than present and empty.
    REQUIRE (run.result (0).contains ("key"));
    REQUIRE_FALSE (run.result (0).contains ("chords"));

    REQUIRE (run.result (1).contains ("chords"));
    REQUIRE_FALSE (run.result (1).contains ("key"));

    // Asking for nothing in particular asks for everything this build knows.
    REQUIRE (run.result (2).contains ("key"));
    REQUIRE (run.result (2).contains ("chords"));

    // And an aspect this build cannot estimate at all is said so, rather than
    // answered with an empty result the model has to interpret.
    REQUIRE (run.error (3).at ("code") == duet::collab::rpcError::invalidParams);
}

TEST_CASE ("a track with nothing to read is answered with nothing, not with a guess", "[collab]")
{
    ProgressionProject fixture { cThenG() };

    const ToolRun run { fixture.session,
                        Json::array ({ estimateCall (fixture.other) }),
                        fixture.options() };

    REQUIRE (run.finished());
    REQUIRE (run.result (0).empty());
    REQUIRE (fixture.ledger.entries (run.id()).empty());
}

TEST_CASE ("an estimate of a track the project does not hold is an error the model can correct "
           "against",
           "[collab]")
{
    ProgressionProject fixture { cThenG() };

    const ToolRun run {
        fixture.session,
        Json::array ({ toolCall ("estimate_audio_content", Json { { "trackId", "track-99" } }),
                       toolCall ("estimate_audio_content", Json::object()) }),
        fixture.options()
    };

    REQUIRE (run.finished());
    REQUIRE (run.error (0).at ("code") == duet::collab::rpcError::invalidParams);
    REQUIRE (run.error (1).at ("code") == duet::collab::rpcError::invalidParams);
}

TEST_CASE ("a run handed an estimate has everything it says afterwards marked", "[collab]")
{
    ProgressionProject fixture { cThenG() };

    const ToolRun run { fixture.session,
                        Json::array ({ toolCommentary ("before. "),
                                       estimateCall (fixture.track, Json::array ({ "key" })),
                                       toolCommentary ("after. "),
                                       toolCall ("get_track_analysis", fixture.track),
                                       toolCommentary ("later still.") }),
                        fixture.options() };

    REQUIRE (run.finished());
    REQUIRE (run.commentary() == "before. after. later still.");

    // The mark lands on what was said after the estimate, and stays on
    // everything after that — including what followed a measured call that
    // needed no guess at all.
    REQUIRE (run.markedCommentary() == "after. later still.");
}

TEST_CASE ("a Suggestion carries the mark of the run that made it", "[collab]")
{
    ProgressionProject fixture { cThenG() };

    const auto suggestion = duet::testing::suggestCall (
        "Lift the keys a little",
        Json::array ({ duet::testing::suggestElement (
            "Bring the keys up two decibels",
            Json::array ({ Json { { "op", "mixer.set" },
                                  { "trackId", duet::collab::toolId::forTrack (fixture.track) },
                                  { "volumeDb", 2.0 } } })) }));

    const ToolRun guessed { fixture.session,
                            Json::array ({ estimateCall (fixture.track, Json::array ({ "key" })),
                                           suggestion }),
                            fixture.options() };

    REQUIRE (guessed.finished());
    REQUIRE (guessed.suggestion (1).basedOnEstimates);

    const ToolRun measured { fixture.session,
                             Json::array (
                                 { toolCall ("get_track_analysis", fixture.track), suggestion }),
                             fixture.options() };

    REQUIRE (measured.finished());
    REQUIRE_FALSE (measured.suggestion (1).basedOnEstimates);
}

TEST_CASE ("a run told nothing but facts says nothing marked", "[collab]")
{
    ProgressionProject fixture { cThenG() };

    const ToolRun run { fixture.session,
                        Json::array ({ toolCall ("list_tracks"),
                                       toolCall ("get_track_analysis", fixture.track),
                                       toolCommentary ("the low end is fine.") }),
                        fixture.options() };

    REQUIRE (run.finished());
    REQUIRE (run.commentary() == "the low end is fine.");
    REQUIRE (run.markedCommentary().empty());
    REQUIRE_FALSE (fixture.ledger.basedOnEstimates (run.id()));
}

TEST_CASE ("a run's ledger names each estimated value, its method and its confidence", "[collab]")
{
    ProgressionProject fixture { cadence() };

    const ToolRun run { fixture.session,
                        Json::array ({ estimateCall (fixture.track) }),
                        fixture.options() };

    REQUIRE (run.finished());

    const auto entries = fixture.ledger.entries (run.id());

    REQUIRE (entries.size() == 2);
    REQUIRE (entries.at (0).tool == "estimate_audio_content");
    REQUIRE (entries.at (0).field == "key");
    REQUIRE (entries.at (0).estimate.value == "C major");
    REQUIRE (entries.at (0).estimate.method == duet::collab::analysis::keyMethod);
    REQUIRE (entries.at (0).estimate.confidence > 0.0);
    REQUIRE (entries.at (0).estimate.confidence <= 1.0);

    REQUIRE (entries.at (1).field == "chords");
    REQUIRE (entries.at (1).estimate.method == duet::collab::analysis::chordMethod);

    // What the ledger holds is what crossed the seam, so the two say the same
    // thing about the same value.
    REQUIRE (duet::collab::wrapped (entries.at (0).estimate) == run.result (0).at ("key"));
}

TEST_CASE ("a run begins with an empty ledger, whatever the run before it was handed", "[collab]")
{
    ProgressionProject fixture { cThenG() };

    const ToolRun tainted { fixture.session,
                            Json::array ({ estimateCall (fixture.track, Json::array ({ "key" })),
                                           toolCommentary ("it is in C.") }),
                            fixture.options() };

    REQUIRE (tainted.finished());
    REQUIRE (fixture.ledger.basedOnEstimates (tainted.id()));

    // A second run of the same service, on the same ledger, and with the same
    // name a run of a fresh service is given: nothing of the first reaches it.
    const ToolRun after { fixture.session,
                          Json::array ({ toolCall ("list_tracks"),
                                         toolCommentary ("four tracks, and nothing odd.") }),
                          fixture.options() };

    REQUIRE (after.finished());
    REQUIRE (after.markedCommentary().empty());
    REQUIRE_FALSE (fixture.ledger.basedOnEstimates (after.id()));
    REQUIRE (fixture.ledger.entries (after.id()).empty());
}

TEST_CASE ("a run that was canceled or that failed leaves no taint for the next one", "[collab]")
{
    duet::collab::EstimateLedger ledger;
    std::string ended;

    // A run that was handed a guess and then did not finish. The service is its
    // own after this block, so the run that follows is one a fresh service
    // names — and a fresh service starts its numbering again, which is the
    // hardest case there is: the next run is called what this one was called.
    SECTION ("canceled")
    {
        duet::testing::RecordingListener listener;
        const duet::testing::Harness harness { "run-hang-once" };
        harness->setEstimateLedger (&ledger);
        harness->setTaskRunListener (&listener);
        harness->start();

        const auto run = harness->startRun ("what key is this in?", {});

        REQUIRE (run.started);

        ledger.record (run.runId,
                       "estimate_audio_content",
                       "key",
                       duet::collab::Estimate { "C major", "a routine", 0.9 });

        REQUIRE (harness->cancelRun (run.runId));
        REQUIRE (ledger.basedOnEstimates (run.runId));

        ended = run.runId;
    }

    SECTION ("failed")
    {
        duet::testing::RecordingListener listener;
        const duet::testing::Harness harness { "run-fail" };
        harness->setEstimateLedger (&ledger);
        harness->setTaskRunListener (&listener);
        harness->start();

        const auto run = harness->startRun ("what key is this in?", {});

        REQUIRE (run.started);

        ledger.record (run.runId,
                       "estimate_audio_content",
                       "key",
                       duet::collab::Estimate { "C major", "a routine", 0.9 });

        REQUIRE (listener.waitForTerminals (1));
        REQUIRE (listener.of (duet::testing::RecordingListener::Kind::terminal).at (0).status
                 == duet::collab::RunStatus::failed);

        ended = run.runId;
    }

    duet::testing::RecordingListener listener;
    const duet::testing::Harness harness { "run-stream" };
    harness->setEstimateLedger (&ledger);
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto next = harness->startRun ("and now?", {});

    REQUIRE (next.runId == ended);
    REQUIRE_FALSE (ledger.basedOnEstimates (next.runId));
    REQUIRE (ledger.entries (next.runId).empty());

    // And what it says carries no mark.
    REQUIRE (listener.waitForTerminals (1));

    const auto said = listener.of (duet::testing::RecordingListener::Kind::commentary);

    REQUIRE_FALSE (said.empty());

    for (const auto& delta : said)
        REQUIRE_FALSE (delta.basedOnEstimates);
}

TEST_CASE ("estimating and measuring a track cost one render between them", "[collab]")
{
    ProgressionProject fixture { cThenG() };

    const ToolRun run { fixture.session,
                        Json::array ({ toolCall ("get_track_analysis", fixture.track),
                                       estimateCall (fixture.track),
                                       estimateCall (fixture.track, Json::array ({ "key" })) }),
                        fixture.options() };

    REQUIRE (run.finished());
    REQUIRE (fixture.renders.load() == 1);

    // And an edit to the track is what throws that render away.
    fixture.session.performAction ("Move this track",
                                   [&] (auto& ops) { ops.setTrackVolumeDb (fixture.track, -6.0); });

    const ToolRun moved { fixture.session,
                          Json::array ({ estimateCall (fixture.track, Json::array ({ "key" })) }),
                          fixture.options() };

    REQUIRE (moved.finished());
    REQUIRE (fixture.renders.load() == 2);
    REQUIRE (moved.result (0).contains ("key"));
}

TEST_CASE ("a producer keeps editing while an estimate is in flight", "[collab]")
{
    ProgressionProject fixture { cThenG() };

    std::atomic<bool> rendering { false };
    std::atomic<bool> edited { false };
    std::mutex mutex;
    std::thread::id renderThread;

    duet::collab::TrackRenders rendered {
        [&] (TrackRef track,
             const std::filesystem::path& destination,
             const std::function<bool()>& keepGoing)
        {
            {
                const std::lock_guard lock (mutex);
                renderThread = std::this_thread::get_id();
            }

            rendering = true;

            // The producer's edit can only land if the message thread is free
            // while this runs, so waiting for it here is the assertion.
            for (int waited = 0; waited < 2000 && ! edited.load(); ++waited)
                std::this_thread::sleep_for (std::chrono::milliseconds (1));

            return duet::collab::offlineTrackRenderer (fixture.session) (
                track, destination, keepGoing);
        },
        fixture.project.folder()
    };

    duet::collab::ContentEstimates estimates {
        fixture.session, duet::testing::messageThreadMarshal(), rendered, fixture.ledger
    };

    ToolRunOptions options;
    options.estimated = &estimates;
    options.ledger = &fixture.ledger;
    options.meanwhile = [&]
    {
        if (rendering.load() && ! edited.load())
        {
            fixture.session.performAction (
                "Rename a track", [&] (auto& ops) { ops.renameTrack (fixture.other, "Renamed"); });
            edited = true;
        }
    };

    const ToolRun run { fixture.session, Json::array ({ estimateCall (fixture.track) }), options };

    REQUIRE (run.finished());
    REQUIRE (edited.load());
    REQUIRE (fixture.session.track (fixture.other).name == "Renamed");

    // The estimate was made on the thread the call arrived on, which is the
    // service's own and never the one the project is written on.
    const std::lock_guard lock (mutex);
    REQUIRE (renderThread == run.serviceThread());
    REQUIRE (renderThread != std::this_thread::get_id());
}

TEST_CASE ("an estimate nobody is waiting for any more is abandoned", "[collab]")
{
    ProgressionProject fixture { cThenG() };
    std::atomic<int> rendered { 0 };

    duet::collab::TrackRenders renders {
        [&] (TrackRef, const std::filesystem::path&, const std::function<bool()>&)
        {
            ++rendered;

            return true;
        },
        fixture.project.folder(),
        [] (const std::string&) { return false; }
    };

    duet::collab::ContentEstimates estimates {
        fixture.session, duet::testing::messageThreadMarshal(), renders, fixture.ledger
    };

    ToolRunOptions options;
    options.estimated = &estimates;
    options.ledger = &fixture.ledger;

    const ToolRun run { fixture.session, Json::array ({ estimateCall (fixture.track) }), options };

    REQUIRE (run.finished());
    REQUIRE (run.error (0).at ("code") == duet::collab::rpcError::runAbandoned);
    REQUIRE (rendered.load() == 0);
    REQUIRE (fixture.ledger.entries (run.id()).empty());
}

TEST_CASE ("the aspects the model may ask for are the aspects this build estimates", "[collab]")
{
    // The tool's parameters are the sidecar's, because that is what reaches the
    // model, and an aspect the model may ask for and never gets an answer to is
    // worse than one it was never offered. The other two the contract names —
    // the notes of a polyphonic part and its instrument — are offered again when
    // the transcription model can answer them (issue bmrxnw).
    const std::filesystem::path vocabulary =
        std::filesystem::path { DUET_SIDECAR_DIR } / "src" / "vocabulary.ts";

    std::ifstream file { vocabulary };

    REQUIRE (file.is_open());

    std::ostringstream read;
    read << file.rdbuf();

    // What the schema says rather than how its source is laid out.
    std::string said;

    for (const auto letter : std::move (read).str())
        if (std::isspace (static_cast<unsigned char> (letter)) == 0)
            said += letter;

    REQUIRE (said.find (R"(Type.Literal("key"))") != std::string::npos);
    REQUIRE (said.find (R"(Type.Literal("chords"))") != std::string::npos);
    REQUIRE (said.find (R"(Type.Literal("notes"))") == std::string::npos);
    REQUIRE (said.find (R"(Type.Literal("instrument"))") == std::string::npos);
}
