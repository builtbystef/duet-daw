#include "ProjectToolsHarness.h"

#include <duet/collab/Analysis.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/** `get_track_analysis`: what a track actually puts out, measured over its
    rendered audio and answered across the socket.

    The seam is the spec's primary one — a real service, a real socket, the
    test-double sidecar as a real child process, and a real project — because
    what this slice adds to the vocabulary is a tool, and a tool is what comes
    back over the wire. What the routines themselves measure is asserted where
    the spec puts that: AnalysisRoutinesTests, over signals built by hand.
*/

using Catch::Matchers::WithinAbs;
using duet::collab::Json;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;
using duet::testing::toolCall;
using duet::testing::ToolRun;
using duet::testing::ToolRunOptions;

namespace
{
/** The tone every fixture here is made of: half of full scale, so a render of
    it peaks at −6.02 dB and measures −9.03 by RMS, and a kilohertz, which sits
    in the middle of the mid band and nowhere near the edge of another.
*/
constexpr double toneFrequencyHz = 1000.0;
constexpr double tonePeakDb = -6.02;
constexpr double toneRmsDb = -9.03;

/** How far a measured level may be from what the signal was written at. A
    render carries the tone through a fader and a bit depth, and neither moves
    it by anything like this.
*/
constexpr double levelToleranceDb = 0.3;

/** A project of one audio track carrying that tone, at a tempo that makes a bar
    two seconds long so a bar range is arithmetic anyone can check.
*/
struct ToneProject
{
    // Four seconds by default, so that a three-second short-term window fits
    // inside the render: a track too short to hold one has no short-term
    // loudness to report.
    explicit ToneProject (double startSeconds = 0.0,
                          double lengthSeconds = 4.0,
                          bool headless = true)
    {
        const auto tone = project.writeTone ("tone.wav", lengthSeconds, toneFrequencyHz);

        // No device and no rebuild unless a case is about playing: a render
        // needs neither, and the engine's one-time rebuild is a stop nothing
        // here should have to survive.
        if (headless)
        {
            session.useNoAudioDevice();
            session.suppressDeviceRebuild();
        }

        session.performAction ("Build the example",
                               [&] (auto& ops)
                               {
                                   ops.setTempo (120.0);
                                   ops.setTimeSignature (4, 4);
                                   track = ops.createTrack (TrackKind::audio, "Tone");
                                   other = ops.createTrack (TrackKind::midi, "Elsewhere");
                                   ops.insertAudioClip (
                                       track, "tone", tone, startSeconds, lengthSeconds);
                               });
    }

    /** The project's own render path, with a tally of how often it was asked:
        what says a call was answered out of the cache rather than by rendering
        again.
    */
    [[nodiscard]] duet::collab::TrackRenderer countingRenderer()
    {
        return [this] (TrackRef track,
                       const std::filesystem::path& destination,
                       const std::function<bool()>& keepGoing)
        {
            ++renders;

            return duet::collab::offlineTrackRenderer (session) (track, destination, keepGoing);
        };
    }

    [[nodiscard]] ToolRunOptions options()
    {
        ToolRunOptions made;
        made.measured = &measured;

        return made;
    }

    TempProject project;
    Session session { project.editFile() };
    TrackRef track = duet::model::noTrack;
    TrackRef other = duet::model::noTrack;
    std::atomic<int> renders { 0 };
    duet::collab::TrackAnalysis measured { session,
                                           duet::testing::messageThreadMarshal(),
                                           countingRenderer(),
                                           project.folder() };
};

/** A call on one track, with a bar range or without one. */
Json analysisCall (TrackRef track) { return toolCall ("get_track_analysis", track); }

Json analysisCall (TrackRef track, int firstBar, int lastBar)
{
    Json arguments = Json::object();
    arguments["trackId"] = duet::collab::toolId::forTrack (track);
    arguments["barRange"] = Json::array ({ firstBar, lastBar });

    return toolCall ("get_track_analysis", arguments);
}

/** Whether anything anywhere in a result is wrapped as an estimate.

    The provenance rule stated as a search rather than as a field list: a
    measured tool may not produce a wrapper anywhere, however deep.
*/
bool holdsAnEstimate (const Json& value)
{
    std::vector<const Json*> remaining { &value };

    while (! remaining.empty())
    {
        const auto* looking = remaining.back();
        remaining.pop_back();

        if (looking->is_object()
            && (looking->contains ("source") || looking->contains ("confidence")))
            return true;

        if (looking->is_object() || looking->is_array())
            for (const auto& inside : *looking)
                remaining.push_back (&inside);
    }

    return false;
}

/** The energy of one named band in a result. */
double bandEnergyDb (const Json& result, std::string_view band)
{
    for (const auto& entry : result.at ("spectralBands"))
        if (entry.at ("band") == band)
            return entry.at ("energyDb").get<double>();

    return 0.0;
}
} // namespace

TEST_CASE ("a track's analysis carries every measurement of the contract, each a bare number",
           "[collab]")
{
    ToneProject fixture;

    const ToolRun run { fixture.session,
                        Json::array ({ analysisCall (fixture.track) }),
                        fixture.options() };

    REQUIRE (run.finished());

    const auto& measured = run.result (0);

    REQUIRE_THAT (measured.at ("peakDb").get<double>(), WithinAbs (tonePeakDb, levelToleranceDb));
    REQUIRE (measured.at ("truePeakDbtp").get<double>() >= measured.at ("peakDb").get<double>());
    REQUIRE_THAT (measured.at ("rmsDb").get<double>(), WithinAbs (toneRmsDb, levelToleranceDb));
    // A tone in both channels measures the level it was written at: BS.1770
    // adds its channels rather than averaging them, and its own offset is what
    // makes a stereo sine's loudness its amplitude in decibels.
    REQUIRE_THAT (measured.at ("lufsIntegrated").get<double>(),
                  WithinAbs (tonePeakDb, levelToleranceDb));
    REQUIRE_THAT (measured.at ("lufsShortTermMax").get<double>(),
                  WithinAbs (tonePeakDb, levelToleranceDb));
    REQUIRE_THAT (measured.at ("crestFactorDb").get<double>(), WithinAbs (3.01, 0.2));

    // The seven bands, in their own order, and the tone in the one it belongs
    // to and nowhere else.
    REQUIRE (measured.at ("spectralBands").size() == 7);
    REQUIRE (measured.at ("spectralBands").at (0).at ("band") == "sub");
    REQUIRE (measured.at ("spectralBands").at (6).at ("band") == "air");
    REQUIRE_THAT (bandEnergyDb (measured, "mid"), WithinAbs (toneRmsDb, levelToleranceDb));
    REQUIRE (bandEnergyDb (measured, "low") < bandEnergyDb (measured, "mid") - 40.0);
    REQUIRE (bandEnergyDb (measured, "high") < bandEnergyDb (measured, "mid") - 40.0);

    REQUIRE_THAT (measured.at ("spectralCentroidHz").get<double>(),
                  WithinAbs (toneFrequencyHz, 50.0));
    REQUIRE (measured.at ("spectralFlatness").get<double>() < 0.05);
    REQUIRE_THAT (measured.at ("stereoCorrelation").get<double>(), WithinAbs (1.0, 0.01));
    REQUIRE_THAT (measured.at ("stereoWidth").get<double>(), WithinAbs (0.0, 0.01));

    // The tone starts where the timeline does, which is beat zero.
    REQUIRE (measured.at ("onsetsBeats").size() == 1);
    REQUIRE_THAT (measured.at ("onsetsBeats").at (0).get<double>(), WithinAbs (0.0, 0.05));

    // Everything measured is a fact, so nothing anywhere in it is wrapped.
    REQUIRE_FALSE (holdsAnEstimate (measured));

    for (const auto& [name, value] : measured.items())
        if (name != "spectralBands" && name != "onsetsBeats")
            REQUIRE (value.is_number());
}

TEST_CASE ("a track id the project does not hold is an error the model can correct against",
           "[collab]")
{
    ToneProject fixture;

    const ToolRun run { fixture.session,
                        Json::array (
                            { toolCall ("get_track_analysis", Json { { "trackId", "track-99" } }),
                              toolCall ("get_track_analysis", Json::object()) }),
                        fixture.options() };

    REQUIRE (run.finished());

    REQUIRE (run.error (0).at ("code") == duet::collab::rpcError::invalidParams);
    REQUIRE (run.error (1).at ("code") == duet::collab::rpcError::invalidParams);
}

TEST_CASE ("a bar range measures that part of the track, and no range measures all of it",
           "[collab]")
{
    // Two seconds to a bar at 120 BPM in 4/4, so bar 5 begins eight seconds in
    // and the four bars after it are the eight seconds of tone.
    ToneProject fixture { 8.0, 8.0 };

    const ToolRun run { fixture.session,
                        Json::array ({ analysisCall (fixture.track, 1, 4),
                                       analysisCall (fixture.track, 5, 8),
                                       analysisCall (fixture.track) }),
                        fixture.options() };

    REQUIRE (run.finished());

    const auto& before = run.result (0);
    const auto& during = run.result (1);
    const auto& whole = run.result (2);

    REQUIRE_THAT (before.at ("peakDb").get<double>(),
                  WithinAbs (duet::collab::analysis::silenceDb, 0.0001));
    REQUIRE_THAT (before.at ("rmsDb").get<double>(),
                  WithinAbs (duet::collab::analysis::silenceDb, 0.0001));
    REQUIRE (before.at ("onsetsBeats").empty());

    REQUIRE_THAT (during.at ("peakDb").get<double>(), WithinAbs (tonePeakDb, levelToleranceDb));
    REQUIRE_THAT (during.at ("rmsDb").get<double>(), WithinAbs (toneRmsDb, levelToleranceDb));
    REQUIRE (during.at ("onsetsBeats").size() == 1);
    REQUIRE_THAT (during.at ("onsetsBeats").at (0).get<double>(), WithinAbs (16.0, 0.05));

    // The whole track is that tone and the silence in front of it, so half of
    // it carries signal and its level is three decibels under the tone's own.
    REQUIRE_THAT (whole.at ("peakDb").get<double>(), WithinAbs (tonePeakDb, levelToleranceDb));
    REQUIRE_THAT (whole.at ("rmsDb").get<double>(), WithinAbs (toneRmsDb - 3.01, levelToleranceDb));
    REQUIRE (whole.at ("onsetsBeats").size() == 1);
}

TEST_CASE ("a track's render is kept, and only an edit to that track throws it away", "[collab]")
{
    ToneProject fixture;

    // Twice in one run: the second call is answered out of what the first one
    // rendered.
    const ToolRun asked { fixture.session,
                          Json::array (
                              { analysisCall (fixture.track), analysisCall (fixture.track) }),
                          fixture.options() };

    REQUIRE (asked.finished());
    REQUIRE (fixture.renders.load() == 1);
    REQUIRE (asked.result (0) == asked.result (1));

    fixture.session.performAction ("Move another track",
                                   [&] (auto& ops) { ops.setTrackVolumeDb (fixture.other, -3.0); });

    const ToolRun elsewhere { fixture.session,
                              Json::array ({ analysisCall (fixture.track) }),
                              fixture.options() };

    REQUIRE (elsewhere.finished());
    REQUIRE (fixture.renders.load() == 1);

    fixture.session.performAction ("Move this track",
                                   [&] (auto& ops) { ops.setTrackVolumeDb (fixture.track, -6.0); });

    const ToolRun moved { fixture.session,
                          Json::array ({ analysisCall (fixture.track) }),
                          fixture.options() };

    REQUIRE (moved.finished());
    REQUIRE (fixture.renders.load() == 2);

    // And the fresh render is of the track as it is now.
    REQUIRE_THAT (moved.result (0).at ("peakDb").get<double>(),
                  WithinAbs (tonePeakDb - 6.0, levelToleranceDb));
}

TEST_CASE ("a producer keeps editing while a first analysis is in flight", "[collab]")
{
    ToneProject fixture;

    std::atomic<bool> rendering { false };
    std::atomic<bool> edited { false };
    std::mutex mutex;
    std::thread::id renderThread;

    duet::collab::TrackAnalysis measured {
        fixture.session,
        duet::testing::messageThreadMarshal(),
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

    ToolRunOptions options;
    options.measured = &measured;
    options.meanwhile = [&]
    {
        if (rendering.load() && ! edited.load())
        {
            fixture.session.performAction (
                "Rename a track", [&] (auto& ops) { ops.renameTrack (fixture.other, "Renamed"); });
            edited = true;
        }
    };

    const ToolRun run { fixture.session, Json::array ({ analysisCall (fixture.track) }), options };

    REQUIRE (run.finished());
    REQUIRE (edited.load());
    REQUIRE (fixture.session.track (fixture.other).name == "Renamed");

    // The render happened on the thread the call arrived on, which is the
    // service's own and never the one the project is written on.
    const std::lock_guard lock (mutex);
    REQUIRE (renderThread == run.serviceThread());
    REQUIRE (renderThread != std::this_thread::get_id());
}

TEST_CASE ("an analysis nobody is waiting for any more is abandoned", "[collab]")
{
    SECTION ("before it has rendered anything")
    {
        ToneProject fixture;
        std::atomic<int> rendered { 0 };

        duet::collab::TrackAnalysis measured {
            fixture.session,
            duet::testing::messageThreadMarshal(),
            [&] (TrackRef, const std::filesystem::path&, const std::function<bool()>&)
            {
                ++rendered;

                return true;
            },
            fixture.project.folder(),
            [] (const std::string&) { return false; }
        };

        ToolRunOptions options;
        options.measured = &measured;

        const ToolRun run { fixture.session,
                            Json::array ({ analysisCall (fixture.track) }),
                            options };

        REQUIRE (run.finished());
        REQUIRE (run.error (0).at ("code") == duet::collab::rpcError::runAbandoned);
        REQUIRE (rendered.load() == 0);
    }

    SECTION ("in the middle of the render")
    {
        ToneProject fixture;
        std::atomic<bool> started { false };

        duet::collab::TrackAnalysis measured {
            fixture.session,
            duet::testing::messageThreadMarshal(),
            [&] (TrackRef track,
                 const std::filesystem::path& destination,
                 const std::function<bool()>& keepGoing)
            {
                started = true;

                return duet::collab::offlineTrackRenderer (fixture.session) (
                    track, destination, keepGoing);
            },
            fixture.project.folder(),
            [&] (const std::string&) { return ! started.load(); }
        };

        ToolRunOptions options;
        options.measured = &measured;

        const ToolRun run { fixture.session,
                            Json::array ({ analysisCall (fixture.track) }),
                            options };

        REQUIRE (run.finished());
        REQUIRE (started.load());
        REQUIRE (run.error (0).at ("code") == duet::collab::rpcError::runAbandoned);
    }
}

TEST_CASE ("the band edges the model is told are the edges the routine measures", "[collab]")
{
    // The tool's description is the sidecar's, because that is what reaches the
    // model, and a band whose edges the model has wrong is worse than no band.
    const std::filesystem::path vocabulary =
        std::filesystem::path { DUET_SIDECAR_DIR } / "src" / "vocabulary.ts";

    std::ifstream file { vocabulary };

    REQUIRE (file.is_open());

    std::ostringstream read;
    read << file.rdbuf();

    // What the description says, and not how its source is laid out: a long one
    // is written as several literals joined together, so the quotes, the pluses
    // and the line breaks between them collapse to the single space they stand
    // for before anything is looked for in it.
    std::string said;

    for (const auto letter : std::move (read).str())
    {
        const auto joins = letter == '"' || letter == '+' || std::isspace (letter) != 0;

        if (! joins)
            said += letter;
        else if (! said.empty() && said.back() != ' ')
            said += ' ';
    }

    for (const auto& band : duet::collab::analysis::spectralBands)
    {
        std::ostringstream stated;
        stated << band.name << " " << static_cast<long long> (band.fromHz) << "-"
               << static_cast<long long> (band.toHz) << " Hz";

        INFO ("the description must state: " << stated.str());
        REQUIRE (said.find (stated.str()) != std::string::npos);
    }
}

TEST_CASE ("a measurement leaves the transport where it found it", "[collab]")
{
    ToneProject fixture { 0.0, 4.0, false };

    if (fixture.session.audioDeviceDescription().empty())
        SKIP ("this machine has no audio device to play through");

    REQUIRE (duet::testing::playUntilRolling (fixture.session));

    // Hazard 6, got out of the way first: the engine rebuilds its device list
    // once and that stops the transport, and a stop it caused must not read as
    // one a tool call caused.
    fixture.session.rebuildDevices();
    REQUIRE (duet::testing::playUntilRolling (fixture.session));

    const auto before = fixture.session.playbackPositionSeconds();

    const ToolRun run { fixture.session,
                        Json::array ({ analysisCall (fixture.track) }),
                        fixture.options() };

    REQUIRE (run.finished());

    // Leaves and not keeps, deliberately. An offline render stops the transport
    // for as long as it runs — the engine's own render status, not anything
    // Duet asks for (engine notes) — so playback does not survive a
    // measurement, it comes back after one, through the model's own play retry.
    // Spec js437t asks for the first and gets the second; that is issue xbh9qk,
    // and what this case pins is only that a measurement does not leave the
    // producer's transport stopped.
    REQUIRE (fixture.session.isPlaying());
    REQUIRE (duet::testing::pumpUntil (
        [&] { return fixture.session.playbackPositionSeconds() > before; }));
}
