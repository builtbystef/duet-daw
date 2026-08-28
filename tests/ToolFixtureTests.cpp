#include "ProjectToolsHarness.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <system_error>
#include <vector>

/** The fod077 corpus, rebuilt as real projects and served through the seam.

    Seven situations a producer would recognise, each one a project this suite
    builds through the edit vocabulary and then asks the five project-read tools
    about over a socket. A fixture file is both the recipe and the expectation:
    everything it states is asserted, and nothing else is.

    `tests/fixtures/collaborator/README.md` says what a fixture file holds and
    what the rebuild changed.
*/

using Catch::Matchers::WithinAbs;
using duet::collab::Json;
using duet::model::AutomationPoint;
using duet::model::AutomationTarget;
using duet::model::BuiltinPlugin;
using duet::model::ClipRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;
using duet::testing::toolCall;
using duet::testing::ToolRun;

namespace
{
constexpr double decibelTolerance = 0.001;
constexpr double barTolerance = 0.000001;

/** A plugin parameter is stored as a float, so a value goes out and comes back
    within a part in a thousand of itself.
*/
double toleranceFor (double value) { return std::max (0.001, std::abs (value) * 0.001); }

/** What Duet calls the built-in a fixture names. */
BuiltinPlugin builtinCalled (const std::string& name)
{
    if (name == "eq")
        return BuiltinPlugin::eq;
    if (name == "compressor")
        return BuiltinPlugin::compressor;
    if (name == "reverb")
        return BuiltinPlugin::reverb;
    if (name == "sampler")
        return BuiltinPlugin::sampler;

    return BuiltinPlugin::synth;
}

/** What the project calls it back, which is what a tool result carries. */
std::string nameOfBuiltin (const std::string& name)
{
    if (name == "eq")
        return "4-Band Equaliser";
    if (name == "compressor")
        return "Compressor";
    if (name == "reverb")
        return "Reverb";
    if (name == "sampler")
        return "Sampler";

    return "4OSC";
}

TrackKind kindCalled (const std::string& name)
{
    if (name == "midi")
        return TrackKind::midi;
    if (name == "group")
        return TrackKind::group;

    return TrackKind::audio;
}

Json readFixture (const std::string& name)
{
    const auto path =
        std::filesystem::path { DUET_TESTS_DIR } / "fixtures" / "collaborator" / (name + ".json");
    std::ifstream file { path };

    return Json::parse (file);
}

int numeratorOf (const Json& fixture)
{
    const std::string signature = fixture.at ("arrangement").at ("timeSignature");

    return std::stoi (signature.substr (0, signature.find ('/')));
}

/** The project a fixture describes, and the name-to-ref map that reads it. */
class BuiltFixture
{
public:
    BuiltFixture (Session& session, const TempProject& folder, const Json& fixture)
    {
        const auto& arrangement = fixture.at ("arrangement");
        const auto beats = numeratorOf (fixture);

        session.performAction (
            "State the arrangement",
            [&] (auto& ops)
            {
                ops.setTempo (arrangement.at ("tempoBpm").get<double>());
                ops.setTimeSignature (
                    beats,
                    std::stoi (std::string { arrangement.at ("timeSignature") }.substr (
                        std::string { arrangement.at ("timeSignature") }.find ('/') + 1)));
                ops.setKey (arrangement.at ("key").get<std::string>());

                std::vector<duet::model::SectionInfo> sections;

                for (const auto& section : arrangement.at ("sections"))
                    sections.push_back (
                        { section.at ("name"), section.at ("startBar"), section.at ("endBar") });

                ops.setSections (sections);
            });

        // The track a new project opens with is not one of the fixture's, and
        // the fixture is meant to be the whole of what a tool sees.
        session.performAction ("Clear the project",
                               [&] (auto& ops)
                               {
                                   for (const auto& track : session.tracks())
                                       ops.removeTrack (track.track);
                               });

        session.performAction ("Add the tracks",
                               [&] (auto& ops)
                               {
                                   for (const auto& track : fixture.at ("tracks"))
                                   {
                                       const std::string kind = track.at ("kind");

                                       if (kind == "master")
                                       {
                                           refs[track.at ("id")] = duet::model::masterChannel;
                                           continue;
                                       }

                                       std::optional<BuiltinPlugin> instrument;

                                       if (track.contains ("instrument"))
                                           instrument = builtinCalled (track.at ("instrument"));

                                       refs[track.at ("id")] =
                                           ops.createTrack (kindCalled (kind),
                                                            std::string { track.at ("name") },
                                                            instrument);
                                   }
                               });

        session.performAction (
            "Route and mix",
            [&] (auto& ops)
            {
                for (const auto& track : fixture.at ("tracks"))
                {
                    const auto ref = refs.at (track.at ("id"));
                    const auto& mixer = track.at ("mixer");

                    ops.setTrackVolumeDb (ref, mixer.at ("volumeDb").get<double>());
                    ops.setTrackPan (ref, mixer.at ("pan").get<double>());
                    ops.setTrackMute (ref, mixer.at ("mute").get<bool>());

                    // The master is where the signal ends: it goes nowhere, it
                    // sends nowhere, and there is nothing for it to be soloed
                    // against.
                    if (ref == duet::model::masterChannel)
                        continue;

                    ops.setTrackSolo (ref, mixer.at ("solo").get<bool>());

                    if (track.contains ("output"))
                        ops.setTrackOutput (ref, refs.at (track.at ("output")));

                    for (const auto& send : mixer.at ("sends"))
                        ops.setSend (
                            ref, refs.at (send.at ("busId")), send.at ("levelDb").get<double>());
                }
            });

        session.performAction (
            "Add the plugins",
            [&] (auto& ops)
            {
                for (const auto& track : fixture.at ("tracks"))
                {
                    const auto ref = refs.at (track.at ("id"));

                    // The instrument is already at the head of the chain, so the
                    // effects go in after it.
                    int position = track.contains ("instrument") ? 1 : 0;

                    for (const auto& plugin : track.value ("plugins", Json::array()))
                    {
                        const auto added =
                            ops.addPlugin (ref, builtinCalled (plugin.at ("builtin")), position++);

                        plugins[track.at ("id")].push_back (added);

                        for (const auto& parameter : plugin.at ("parameters").items())
                            ops.setPluginParameter (
                                added, parameter.key(), parameter.value().get<double>());
                    }
                }
            });

        // Bars become seconds through the project's own tempo map, which is set
        // by now, and a read cannot happen inside an Action anyway.
        placeTheClips (session, folder, fixture, beats);

        const auto secondsPerBeat = 60.0 / arrangement.at ("tempoBpm").get<double>();

        session.performAction (
            "Draw the curves",
            [&] (auto& ops)
            {
                for (const auto& track : fixture.at ("tracks"))
                {
                    for (const auto& lane : track.value ("automation", Json::array()))
                    {
                        std::vector<AutomationPoint> points;

                        for (const auto& point : lane.at ("points"))
                            points.push_back (
                                { point.at ("timeBeats").get<double>() * secondsPerBeat,
                                  point.at ("value").get<double>(),
                                  0.0 });

                        ops.setAutomationPoints (targetOf (track, lane), points);
                    }
                }
            });
    }

    [[nodiscard]] TrackRef ref (const std::string& id) const { return refs.at (id); }

    [[nodiscard]] duet::model::PluginRef plugin (const std::string& track, std::size_t index) const
    {
        return plugins.at (track).at (index);
    }

    [[nodiscard]] const std::vector<ClipRef>& clipsOf (const std::string& track) const
    {
        return clips.at (track);
    }

private:
    [[nodiscard]] AutomationTarget targetOf (const Json& track, const Json& lane) const
    {
        const auto& target = lane.at ("target");

        if (target.is_string())
            return target == "volume" ? AutomationTarget::trackVolumeOf (refs.at (track.at ("id")))
                                      : AutomationTarget::trackPanOf (refs.at (track.at ("id")));

        return AutomationTarget::parameterOf (
            plugins.at (track.at ("id")).at (target.at ("pluginIndex").get<std::size_t>()),
            std::string { target.at ("paramId") });
    }

    void placeTheClips (Session& session,
                        const TempProject& folder,
                        const Json& fixture,
                        int beatsPerBar)
    {
        std::filesystem::path tone;

        for (const auto& track : fixture.at ("tracks"))
        {
            if (track.at ("kind") != "audio")
                continue;

            double longest = 0.0;

            for (const auto& clip : track.value ("clips", Json::array()))
                longest = std::max (
                    longest, session.barStartSeconds (clip.at ("lengthBars").get<int>() + 1));

            if (longest > 0.0)
                tone = folder.writeTone ("fixture-tone.wav", longest + 1.0, 220.0);
        }

        session.performAction (
            "Place the clips",
            [&] (auto& ops)
            {
                for (const auto& track : fixture.at ("tracks"))
                {
                    const auto id = track.at ("id").get<std::string>();
                    const auto ref = refs.at (id);
                    const auto& patterns = track.value ("patterns", Json::object());

                    for (const auto& clip : track.value ("clips", Json::array()))
                    {
                        const auto start = session.barStartSeconds (clip.at ("startBar"));
                        const auto end = session.barStartSeconds (
                            clip.at ("startBar").get<int>() + clip.at ("lengthBars").get<int>());

                        if (! clip.contains ("pattern"))
                        {
                            clips[id].push_back (ops.insertAudioClip (
                                ref, std::string { clip.at ("name") }, tone, start, end - start));
                            continue;
                        }

                        const auto made = ops.insertMidiClip (
                            ref, std::string { clip.at ("name") }, start, end - start);
                        clips[id].push_back (made);

                        const auto& pattern = patterns.at (clip.at ("pattern").get<std::string>());

                        for (const auto& note : pattern.at ("notes"))
                            ops.addNote (made,
                                         note.at ("pitch"),
                                         note.at ("startBeats").get<double>(),
                                         note.at ("lengthBeats").get<double>(),
                                         note.at ("velocity"));

                        if (clip.at ("lengthBars").get<int>()
                            > pattern.at ("lengthBars").get<int>())
                            ops.setClipLoop (
                                made, true, pattern.at ("lengthBars").get<double>() * beatsPerBar);
                    }
                }
            });
    }

    std::map<std::string, TrackRef> refs;
    std::map<std::string, std::vector<duet::model::PluginRef>> plugins;
    std::map<std::string, std::vector<ClipRef>> clips;
};

/** What the track list should say a track's plugins are called. */
Json expectedPluginNames (const Json& track)
{
    Json names = Json::array();

    if (track.contains ("instrument"))
        names.push_back (nameOfBuiltin (track.at ("instrument")));

    for (const auto& plugin : track.value ("plugins", Json::array()))
        names.push_back (nameOfBuiltin (plugin.at ("builtin")));

    return names;
}

bool isLooped (const Json& track, const Json& clip)
{
    if (! clip.contains ("pattern"))
        return false;

    const auto& pattern = track.at ("patterns").at (clip.at ("pattern").get<std::string>());

    return clip.at ("lengthBars").get<int>() > pattern.at ("lengthBars").get<int>();
}
} // namespace

TEST_CASE ("the fod077 corpus is served through the seam, and every fixture reads back as itself",
           "[collab]")
{
    const auto* const name = GENERATE (
        "fixture-a", "fixture-b", "fixture-c", "fixture-d", "fixture-e", "fixture-f", "fixture-g");

    INFO (name);

    const auto fixture = readFixture (name);

    const TempProject project;
    Session session { project.editFile() };

    const BuiltFixture built { session, project, fixture };

    // Every tool, over the whole project: the list and the arrangement once, and
    // the three that take a track id once per track.
    auto calls = Json::array ({ toolCall ("list_tracks"), toolCall ("get_arrangement") });

    for (const auto& track : fixture.at ("tracks"))
    {
        const auto ref = built.ref (track.at ("id"));
        calls.push_back (toolCall ("get_midi", ref));
        calls.push_back (toolCall ("get_automation", ref));
        calls.push_back (toolCall ("get_plugin_chain", ref));
    }

    const ToolRun run { session, calls };

    REQUIRE (run.finished());
    REQUIRE (run.responses().size() == calls.size());

    //==========================================================================
    const auto& listed = run.result (0).at ("tracks");

    REQUIRE (listed.size() == fixture.at ("tracks").size());

    for (std::size_t index = 0; index < listed.size(); ++index)
    {
        const auto& expected = fixture.at ("tracks").at (index);
        const auto& actual = listed.at (index);

        INFO ("track " << expected.at ("id"));

        REQUIRE (actual.at ("id")
                 == duet::collab::toolId::forTrack (built.ref (expected.at ("id"))));
        REQUIRE (actual.at ("name") == expected.at ("name"));
        REQUIRE (actual.at ("kind") == expected.at ("kind"));

        if (expected.contains ("instrument"))
            REQUIRE (actual.at ("instrument") == nameOfBuiltin (expected.at ("instrument")));
        else
            REQUIRE_FALSE (actual.contains ("instrument"));

        if (expected.at ("kind") == "master")
            REQUIRE_FALSE (actual.contains ("output"));
        else
            REQUIRE (actual.at ("output")
                     == duet::collab::toolId::forTrack (expected.contains ("output")
                                                            ? built.ref (expected.at ("output"))
                                                            : duet::model::masterChannel));

        REQUIRE (actual.at ("clipCount") == expected.value ("clips", Json::array()).size());
        REQUIRE (actual.at ("hasMidi")
                 == (expected.at ("kind") == "midi"
                     && ! expected.value ("clips", Json::array()).empty()));
        REQUIRE (actual.at ("pluginNames") == expectedPluginNames (expected));
        REQUIRE (actual.at ("automatedParameters").size()
                 == expected.value ("automation", Json::array()).size());

        const auto& mixer = expected.at ("mixer");

        REQUIRE_THAT (actual.at ("mixer").at ("volumeDb").get<double>(),
                      WithinAbs (mixer.at ("volumeDb").get<double>(), decibelTolerance));
        REQUIRE_THAT (actual.at ("mixer").at ("pan").get<double>(),
                      WithinAbs (mixer.at ("pan").get<double>(), 0.00001));
        REQUIRE (actual.at ("mixer").at ("mute") == mixer.at ("mute"));
        REQUIRE (actual.at ("mixer").at ("solo") == mixer.at ("solo"));
        REQUIRE (actual.at ("mixer").at ("sends").size() == mixer.at ("sends").size());

        for (std::size_t send = 0; send < mixer.at ("sends").size(); ++send)
        {
            REQUIRE (actual.at ("mixer").at ("sends").at (send).at ("busId")
                     == duet::collab::toolId::forTrack (
                         built.ref (mixer.at ("sends").at (send).at ("busId"))));
            REQUIRE_THAT (actual.at ("mixer").at ("sends").at (send).at ("levelDb").get<double>(),
                          WithinAbs (mixer.at ("sends").at (send).at ("levelDb").get<double>(),
                                     decibelTolerance));
        }
    }

    //==========================================================================
    const auto& arrangement = run.result (1);
    const auto& expectedArrangement = fixture.at ("arrangement");

    REQUIRE (arrangement.at ("key") == expectedArrangement.at ("key"));
    REQUIRE_THAT (arrangement.at ("tempoBpm").get<double>(),
                  WithinAbs (expectedArrangement.at ("tempoBpm").get<double>(), 0.000001));
    REQUIRE (arrangement.at ("timeSignature") == expectedArrangement.at ("timeSignature"));
    REQUIRE (arrangement.at ("sections") == expectedArrangement.at ("sections"));

    for (const auto& track : fixture.at ("tracks"))
    {
        const auto placed = std::find_if (arrangement.at ("placements").begin(),
                                          arrangement.at ("placements").end(),
                                          [&] (const Json& entry) {
                                              return entry.at ("trackId")
                                                     == duet::collab::toolId::forTrack (
                                                         built.ref (track.at ("id")));
                                          });

        const auto& expectedClips = track.value ("clips", Json::array());

        INFO ("placement of " << track.at ("id"));

        if (expectedClips.empty())
        {
            REQUIRE (placed == arrangement.at ("placements").end());
            continue;
        }

        REQUIRE (placed != arrangement.at ("placements").end());
        REQUIRE (placed->at ("clips").size() == expectedClips.size());

        for (std::size_t index = 0; index < expectedClips.size(); ++index)
        {
            const auto& expected = expectedClips.at (index);
            const auto& actual = placed->at ("clips").at (index);

            REQUIRE (actual.at ("name") == expected.at ("name"));
            REQUIRE_THAT (actual.at ("startBar").get<double>(),
                          WithinAbs (expected.at ("startBar").get<double>(), barTolerance));
            REQUIRE_THAT (actual.at ("lengthBars").get<double>(),
                          WithinAbs (expected.at ("lengthBars").get<double>(), barTolerance));
            REQUIRE (actual.at ("looped") == isLooped (track, expected));
        }
    }

    //==========================================================================
    std::size_t call = 2;

    for (const auto& track : fixture.at ("tracks"))
    {
        INFO ("track " << track.at ("id"));

        const auto& midi = run.result (call++);
        const auto& automation = run.result (call++);
        const auto& chain = run.result (call++);

        const auto& expectedClips = track.value ("clips", Json::array());
        const auto midiClips = track.at ("kind") == "midi" ? expectedClips.size() : 0;

        REQUIRE (midi.at ("clips").size() == midiClips);

        for (std::size_t index = 0; index < midiClips; ++index)
        {
            const auto& pattern =
                track.at ("patterns")
                    .at (expectedClips.at (index).at ("pattern").get<std::string>());
            const auto& notes = midi.at ("clips").at (index).at ("notes");

            REQUIRE (midi.at ("clips").at (index).at ("clipId")
                     == duet::collab::toolId::forClip (built.clipsOf (track.at ("id")).at (index)));
            REQUIRE (notes.size() == pattern.at ("notes").size());

            for (std::size_t note = 0; note < notes.size(); ++note)
            {
                // The model answers in time order, and a pattern is written in
                // it, so the two lists line up note for note.
                const auto& expected = pattern.at ("notes").at (note);

                INFO ("note " << note);
                REQUIRE (notes.at (note).at ("pitch") == expected.at ("pitch"));
                REQUIRE_THAT (notes.at (note).at ("startBeats").get<double>(),
                              WithinAbs (expected.at ("startBeats").get<double>(), 0.000001));
                REQUIRE_THAT (notes.at (note).at ("lengthBeats").get<double>(),
                              WithinAbs (expected.at ("lengthBeats").get<double>(), 0.000001));
                REQUIRE (notes.at (note).at ("velocity") == expected.at ("velocity"));
                REQUIRE_FALSE (notes.at (note).at ("noteId").get<std::string>().empty());
            }
        }

        const auto& expectedLanes = track.value ("automation", Json::array());

        REQUIRE (automation.at ("lanes").size() == expectedLanes.size());

        for (std::size_t lane = 0; lane < expectedLanes.size(); ++lane)
        {
            const auto& expected = expectedLanes.at (lane);
            const auto& actual = automation.at ("lanes").at (lane);

            INFO ("lane " << lane);

            if (expected.at ("target").is_string())
            {
                REQUIRE (actual.at ("target").at ("kind") == expected.at ("target"));
            }
            else
            {
                REQUIRE (actual.at ("target").at ("kind") == "pluginParam");
                REQUIRE (actual.at ("target").at ("pluginId")
                         == duet::collab::toolId::forPlugin (built.plugin (
                             track.at ("id"),
                             expected.at ("target").at ("pluginIndex").get<std::size_t>())));
                REQUIRE (actual.at ("target").at ("paramId")
                         == expected.at ("target").at ("paramId"));
            }

            REQUIRE (actual.at ("points").size() == expected.at ("points").size());

            for (std::size_t point = 0; point < expected.at ("points").size(); ++point)
            {
                const auto value = expected.at ("points").at (point).at ("value").get<double>();

                REQUIRE_THAT (
                    actual.at ("points").at (point).at ("timeBeats").get<double>(),
                    WithinAbs (expected.at ("points").at (point).at ("timeBeats").get<double>(),
                               0.0001));
                REQUIRE_THAT (actual.at ("points").at (point).at ("value").get<double>(),
                              WithinAbs (value, toleranceFor (value)));
            }
        }

        REQUIRE (chain.at ("plugins").size() == expectedPluginNames (track).size());

        for (std::size_t index = 0; index < chain.at ("plugins").size(); ++index)
        {
            const auto& plugin = chain.at ("plugins").at (index);

            INFO ("plugin " << index);
            REQUIRE (plugin.at ("name") == expectedPluginNames (track).at (index));
            REQUIRE (plugin.at ("format") == "builtin");
            REQUIRE (plugin.at ("enabled") == true);
            REQUIRE (plugin.at ("latencySamples") == 0);
        }

        const auto& effects = track.value ("plugins", Json::array());
        const auto firstEffect = track.contains ("instrument") ? 1U : 0U;

        for (std::size_t index = 0; index < effects.size(); ++index)
        {
            const auto& parameters =
                chain.at ("plugins").at (index + firstEffect).at ("parameters");

            for (const auto& expected : effects.at (index).at ("parameters").items())
            {
                INFO ("parameter " << expected.key());

                const auto found =
                    std::find_if (parameters.begin(),
                                  parameters.end(),
                                  [&] (const Json& parameter)
                                  { return parameter.at ("paramId") == expected.key(); });

                REQUIRE (found != parameters.end());
                REQUIRE (found->at ("value").is_number());
                REQUIRE_THAT (found->at ("value").get<double>(),
                              WithinAbs (expected.value().get<double>(),
                                         toleranceFor (expected.value().get<double>())));
            }
        }
    }
}

namespace
{
/** A render that stands in for the project's own.

    What the provenance rule is about is the tools and not the audio — a bare
    value is a fact whatever the waveform was, and a wrapped one is a guess — so
    the corpus goes through both analysis tools over a signal this suite wrote,
    and asserts the same thing for a fraction of the cost. Measured on the dev
    machine on 2026-08-28: a real render of one of these ninety-six-bar projects
    costs about fifty seconds in an unoptimised Debug build, which is longer
    than a Task Run waits, and would say nothing more about provenance for it.
    What a real render measures and estimates is asserted over real projects in
    TrackAnalysisTests and ContentEstimateTests.
*/
duet::collab::TrackRenderer standInRenderer (const std::filesystem::path& audio)
{
    return
        [audio] (TrackRef, const std::filesystem::path& destination, const std::function<bool()>&)
    {
        std::error_code failed;
        std::filesystem::copy_file (
            audio, destination, std::filesystem::copy_options::overwrite_existing, failed);

        return ! failed;
    };
}
} // namespace

TEST_CASE ("across the corpus, a fact crosses bare and a guess crosses wrapped", "[collab]")
{
    const auto* const name = GENERATE (
        "fixture-a", "fixture-b", "fixture-c", "fixture-d", "fixture-e", "fixture-f", "fixture-g");

    INFO (name);

    const auto fixture = readFixture (name);

    const TempProject project;
    Session session { project.editFile() };

    const BuiltFixture built { session, project, fixture };

    // The first track of the fixture, which every one of them has, put through
    // both analysis tools as well as the read ones: the asymmetry is a property
    // of the tools, so what it takes to assert it is one real project of each
    // kind that exists and one track of each to ask about.
    const auto track = built.ref (fixture.at ("tracks").at (0).at ("id"));
    const auto standIn =
        project.writeChords ("stand-in.wav", 2.0, { { 60, 64, 67 }, { 67, 71, 74 } });

    duet::collab::EstimateLedger ledger;
    duet::collab::TrackRenders renders { standInRenderer (standIn), project.folder() };
    duet::collab::TrackAnalysis measured { session,
                                           duet::testing::messageThreadMarshal(),
                                           renders };
    duet::collab::ContentEstimates estimated {
        session, duet::testing::messageThreadMarshal(), renders, ledger
    };

    duet::testing::ToolRunOptions options;
    options.measured = &measured;
    options.estimated = &estimated;
    options.ledger = &ledger;

    const auto calls = Json::array ({ toolCall ("list_tracks"),
                                      toolCall ("get_arrangement"),
                                      toolCall ("get_midi", track),
                                      toolCall ("get_automation", track),
                                      toolCall ("get_plugin_chain", track),
                                      toolCall ("get_track_analysis", track),
                                      toolCall ("estimate_audio_content", track) });

    const ToolRun run { session, calls, options };

    REQUIRE (run.finished());

    // Nothing read from the project model, and nothing measured off the render,
    // is wrapped anywhere in what it answered with.
    for (std::size_t call = 0; call < 6; ++call)
    {
        INFO ("call " << call);
        REQUIRE_FALSE (duet::testing::holdsAnEstimate (run.result (call)));
    }

    // And nothing the estimating tool answers with is bare: every member of its
    // result is a wrapper, the ledger holds the same values, and the run is
    // marked because it holds any.
    const auto& estimates = run.result (6);

    REQUIRE_FALSE (estimates.empty());

    for (const auto& [aspect, value] : estimates.items())
    {
        INFO ("aspect " << aspect);
        REQUIRE (duet::testing::isAnEstimate (value));
    }

    REQUIRE (ledger.entries (run.id()).size() == estimates.size());
    REQUIRE (ledger.basedOnEstimates (run.id()));
}
