#include "ProjectToolsHarness.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::collab::Json;
using duet::model::AutomationPoint;
using duet::model::AutomationTarget;
using duet::model::BuiltinPlugin;
using duet::model::PluginRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;
using duet::testing::toolCall;
using duet::testing::ToolRun;
using duet::testing::trackEntry;

namespace
{
/** The fader stores a position and not a level, in single precision, so a level
    goes out and comes back within a small fraction of a decibel.
*/
constexpr double decibelTolerance = 0.001;

/** The project the spec's worked example describes: a MIDI bass at −6 dB with
    two clips, an EQ, a volume curve, and a bus it both feeds and sends to.
*/
struct BassProject
{
    TrackRef bass = duet::model::noTrack;
    TrackRef bus = duet::model::noTrack;
    PluginRef eq = duet::model::noPlugin;
};

BassProject buildBassProject (Session& session)
{
    BassProject built;

    session.performAction (
        "Build the example",
        [&] (auto& ops)
        {
            built.bass = ops.createTrack (TrackKind::midi, "Bass", BuiltinPlugin::synth);
            built.bus = ops.createTrack (TrackKind::group, "Low Bus");

            ops.setTrackOutput (built.bass, built.bus);
            ops.setTrackVolumeDb (built.bass, -6.0);
            ops.setTrackPan (built.bass, 0.0);
            ops.setSend (built.bass, built.bus, -12.0);

            ops.insertMidiClip (built.bass, "riff", 0.0, 4.0);
            ops.insertMidiClip (built.bass, "riff again", 4.0, 4.0);

            built.eq = ops.addPlugin (built.bass, BuiltinPlugin::eq, 1);

            ops.setAutomationPoints (
                AutomationTarget::trackVolumeOf (built.bass),
                { AutomationPoint { 0.0, -6.0, 0.0 }, AutomationPoint { 2.0, -3.0, 0.0 } });
        });

    return built;
}
} // namespace

TEST_CASE ("the track list carries a track's mixer, routing, clips, plugins and curves", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildBassProject (session);

    const ToolRun run { session, Json::array ({ toolCall ("list_tracks") }) };

    REQUIRE (run.finished());

    const auto bass = trackEntry (run.result (0), built.bass);

    REQUIRE (bass.at ("name") == "Bass");
    REQUIRE (bass.at ("kind") == "midi");
    REQUIRE (bass.at ("instrument") == "4OSC");
    REQUIRE (bass.at ("output") == duet::collab::toolId::forTrack (built.bus));
    REQUIRE (bass.at ("clipCount") == 2);
    REQUIRE (bass.at ("hasMidi") == true);
    REQUIRE (bass.at ("pluginNames") == Json::array ({ "4OSC", "4-Band Equaliser" }));
    REQUIRE (bass.at ("automatedParameters") == Json::array ({ "volume" }));

    const auto& mixer = bass.at ("mixer");

    REQUIRE_THAT (mixer.at ("volumeDb").get<double>(), WithinAbs (-6.0, decibelTolerance));
    REQUIRE_THAT (mixer.at ("pan").get<double>(), WithinAbs (0.0, 0.000001));
    REQUIRE (mixer.at ("mute") == false);
    REQUIRE (mixer.at ("solo") == false);

    REQUIRE (mixer.at ("sends").size() == 1);
    REQUIRE (mixer.at ("sends").at (0).at ("busId") == duet::collab::toolId::forTrack (built.bus));
    REQUIRE_THAT (mixer.at ("sends").at (0).at ("levelDb").get<double>(),
                  WithinAbs (-12.0, decibelTolerance));
}

TEST_CASE ("the master and every group are tracks, and every tool takes their ids", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildBassProject (session);
    const auto master = duet::model::masterChannel;

    const ToolRun run { session,
                        Json::array ({ toolCall ("list_tracks"),
                                       toolCall ("get_midi", master),
                                       toolCall ("get_automation", master),
                                       toolCall ("get_plugin_chain", master),
                                       toolCall ("get_midi", built.bus),
                                       toolCall ("get_automation", built.bus),
                                       toolCall ("get_plugin_chain", built.bus) }) };

    REQUIRE (run.finished());

    REQUIRE (trackEntry (run.result (0), master).at ("kind") == "master");
    REQUIRE (trackEntry (run.result (0), master).at ("name") == "Master");
    REQUIRE (trackEntry (run.result (0), built.bus).at ("kind") == "group");

    // The master is the one track with nowhere to go, and every other track
    // names where it goes — the bus by name, and the bus itself the master.
    REQUIRE_FALSE (trackEntry (run.result (0), master).contains ("output"));
    REQUIRE (trackEntry (run.result (0), built.bus).at ("output")
             == duet::collab::toolId::forTrack (master));

    for (std::size_t call = 1; call < 7; ++call)
    {
        INFO ("call " << call);
        REQUIRE (run.error (call).empty());
    }

    REQUIRE (run.result (1).at ("clips").empty());
    REQUIRE (run.result (2).at ("lanes").empty());
    REQUIRE (run.result (3).at ("plugins").empty());
}

TEST_CASE ("the arrangement carries the metre, the sections and where the clips are", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;

    session.performAction ("Build the arrangement",
                           [&] (auto& ops)
                           {
                               ops.setTempo (128.0);
                               ops.setTimeSignature (4, 4);
                               ops.setSections ({ { "Intro", 1, 8 } });
                               keys =
                                   ops.createTrack (TrackKind::midi, "Keys", BuiltinPlugin::synth);
                           });

    // Bar 5 and four bars of it, said in the seconds the vocabulary writes in.
    const auto fifthBar = session.barStartSeconds (5);
    const auto fourBars = session.barStartSeconds (9) - fifthBar;

    session.performAction ("Place a clip",
                           [&] (auto& ops)
                           {
                               const auto clip =
                                   ops.insertMidiClip (keys, "chords", fifthBar, fourBars);
                               ops.setClipLoop (clip, true, 4.0);
                           });

    {
        const ToolRun run { session, Json::array ({ toolCall ("get_arrangement") }) };

        REQUIRE (run.finished());

        const auto& arrangement = run.result (0);

        REQUIRE_THAT (arrangement.at ("tempoBpm").get<double>(), WithinAbs (128.0, 0.000001));
        REQUIRE (arrangement.at ("timeSignature") == "4/4");
        REQUIRE (arrangement.at ("barCount") == 8);

        REQUIRE (arrangement.at ("sections").size() == 1);
        REQUIRE (arrangement.at ("sections").at (0).at ("name") == "Intro");
        REQUIRE (arrangement.at ("sections").at (0).at ("startBar") == 1);
        REQUIRE (arrangement.at ("sections").at (0).at ("endBar") == 8);

        REQUIRE (arrangement.at ("placements").size() == 1);

        const auto& placed = arrangement.at ("placements").at (0);

        REQUIRE (placed.at ("trackId") == duet::collab::toolId::forTrack (keys));
        REQUIRE (placed.at ("clips").size() == 1);

        const auto& clip = placed.at ("clips").at (0);

        REQUIRE (clip.at ("name") == "chords");
        REQUIRE_THAT (clip.at ("startBar").get<double>(), WithinAbs (5.0, 0.000001));
        REQUIRE_THAT (clip.at ("lengthBars").get<double>(), WithinAbs (4.0, 0.000001));
        REQUIRE (clip.at ("looped") == true);

        // A project that declares no key has no key field at all, which is what
        // sends the Collaborator to the analysis layer for one.
        REQUIRE_FALSE (arrangement.contains ("key"));
    }

    session.performAction ("Name the key", [&] (auto& ops) { ops.setKey ("F minor"); });

    {
        const ToolRun run { session, Json::array ({ toolCall ("get_arrangement") }) };

        REQUIRE (run.finished());

        // A key the project declares is read from the project model, so it
        // crosses the seam bare: a fact, and not an estimate wrapper.
        REQUIRE (run.result (0).at ("key").is_string());
        REQUIRE (run.result (0).at ("key") == "F minor");
    }
}

TEST_CASE ("the MIDI of a clip, and of every clip on a track", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;
    duet::model::ClipRef phrase = duet::model::noClip;

    session.performAction ("Write a phrase",
                           [&] (auto& ops)
                           {
                               keys =
                                   ops.createTrack (TrackKind::midi, "Keys", BuiltinPlugin::synth);
                               phrase = ops.insertMidiClip (keys, "phrase", 0.0, 2.0);
                               ops.insertMidiClip (keys, "answer", 2.0, 2.0);
                               ops.addNote (phrase, 60, 1.0, 0.5, 100);
                           });

    Json oneClip = Json::object();
    oneClip["trackId"] = duet::collab::toolId::forTrack (keys);
    oneClip["clipId"] = duet::collab::toolId::forClip (phrase);

    const ToolRun run { session,
                        Json::array ({ toolCall ("get_midi", oneClip),
                                       toolCall ("get_midi", keys),
                                       toolCall ("get_midi", oneClip) }) };

    REQUIRE (run.finished());

    REQUIRE (run.result (0).at ("clips").size() == 1);
    REQUIRE (run.result (0).at ("clips").at (0).at ("clipId")
             == duet::collab::toolId::forClip (phrase));

    const auto& notes = run.result (0).at ("clips").at (0).at ("notes");

    REQUIRE (notes.size() == 1);
    REQUIRE (notes.at (0).at ("pitch") == 60);
    REQUIRE_THAT (notes.at (0).at ("startBeats").get<double>(), WithinAbs (1.0, 0.000001));
    REQUIRE_THAT (notes.at (0).at ("lengthBeats").get<double>(), WithinAbs (0.5, 0.000001));
    REQUIRE (notes.at (0).at ("velocity") == 100);
    REQUIRE_FALSE (notes.at (0).at ("noteId").get<std::string>().empty());

    // Asking for the track and not the clip is asking about every MIDI clip on
    // it, which is how a model that has not read the arrangement yet begins.
    REQUIRE (run.result (1).at ("clips").size() == 2);

    // A note's id is the same note's id the next time it is asked about.
    REQUIRE (run.result (2).at ("clips").at (0).at ("notes").at (0).at ("noteId")
             == notes.at (0).at ("noteId"));
}

TEST_CASE ("automation lanes carry their points in time order, and name what they drive",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef bass = duet::model::noTrack;
    PluginRef compressor = duet::model::noPlugin;

    session.performAction (
        "Draw the curves",
        [&] (auto& ops)
        {
            bass = ops.createTrack (TrackKind::midi, "Bass", BuiltinPlugin::synth);
            compressor = ops.addPlugin (bass, BuiltinPlugin::compressor, 1);

            // The project's own tempo is 120, so four beats is two seconds.
            ops.setAutomationPoints (
                AutomationTarget::trackVolumeOf (bass),
                { AutomationPoint { 2.0, -3.0, 0.0 }, AutomationPoint { 0.0, -12.0, 0.0 } });

            ops.setAutomationPoints (AutomationTarget::parameterOf (compressor, "ratio"),
                                     { AutomationPoint { 0.0, 0.25, 0.0 } });
        });

    const ToolRun run { session, Json::array ({ toolCall ("get_automation", bass) }) };

    REQUIRE (run.finished());

    const auto& lanes = run.result (0).at ("lanes");

    REQUIRE (lanes.size() == 2);

    const auto& volume = lanes.at (0);

    REQUIRE (volume.at ("target").at ("kind") == "volume");
    REQUIRE (volume.at ("points").size() == 2);
    REQUIRE_THAT (volume.at ("points").at (0).at ("timeBeats").get<double>(),
                  WithinAbs (0.0, 0.000001));
    REQUIRE_THAT (volume.at ("points").at (0).at ("value").get<double>(),
                  WithinAbs (-12.0, decibelTolerance));
    REQUIRE_THAT (volume.at ("points").at (1).at ("timeBeats").get<double>(),
                  WithinAbs (4.0, 0.000001));
    REQUIRE_THAT (volume.at ("points").at (1).at ("value").get<double>(),
                  WithinAbs (-3.0, decibelTolerance));

    const auto& ratio = lanes.at (1);

    REQUIRE (ratio.at ("target").at ("kind") == "pluginParam");
    REQUIRE (ratio.at ("target").at ("pluginId") == duet::collab::toolId::forPlugin (compressor));
    REQUIRE (ratio.at ("target").at ("paramId") == "ratio");
    REQUIRE (ratio.at ("points").size() == 1);
}

TEST_CASE ("a plugin chain carries its order, its enabled state, its latency and its parameters",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef bass = duet::model::noTrack;
    PluginRef eq = duet::model::noPlugin;
    PluginRef compressor = duet::model::noPlugin;

    session.performAction ("Build a chain",
                           [&] (auto& ops)
                           {
                               bass = ops.createTrack (TrackKind::audio, "Bass");

                               eq = ops.addPlugin (bass, BuiltinPlugin::eq, 0);
                               compressor = ops.addPlugin (bass, BuiltinPlugin::compressor, 1);

                               ops.setPluginParameter (eq, "Low-pass freq", 80.0);
                               ops.setPluginParameter (eq, "Low-pass gain", -3.0);
                               ops.setPluginBypassed (compressor, true);
                           });

    const ToolRun run { session, Json::array ({ toolCall ("get_plugin_chain", bass) }) };

    REQUIRE (run.finished());

    const auto& plugins = run.result (0).at ("plugins");

    REQUIRE (plugins.size() == 2);
    REQUIRE (plugins.at (0).at ("pluginId") == duet::collab::toolId::forPlugin (eq));
    REQUIRE (plugins.at (1).at ("pluginId") == duet::collab::toolId::forPlugin (compressor));
    REQUIRE (plugins.at (0).at ("name") == "4-Band Equaliser");
    REQUIRE (plugins.at (0).at ("format") == "builtin");
    REQUIRE (plugins.at (0).at ("enabled") == true);
    REQUIRE (plugins.at (0).at ("latencySamples") == 0);

    REQUIRE (plugins.at (1).at ("name") == "Compressor");
    REQUIRE (plugins.at (1).at ("enabled") == false);

    const auto parameterCalled = [&] (const std::string& id)
    {
        for (const auto& parameter : plugins.at (0).at ("parameters"))
            if (parameter.at ("paramId") == id)
                return parameter;

        return Json::object();
    };

    const auto frequency = parameterCalled ("Low-pass freq");
    const auto gain = parameterCalled ("Low-pass gain");

    // Duet owns what its built-ins mean, so their values cross bare: a number
    // with a name and a unit, and no estimate wrapper anywhere.
    REQUIRE (frequency.at ("name") == "Low-shelf freq");
    REQUIRE (frequency.at ("value").is_number());
    REQUIRE_THAT (frequency.at ("value").get<double>(), WithinAbs (80.0, 0.01));
    REQUIRE (frequency.at ("unit") == "Hz");

    REQUIRE (gain.at ("value").is_number());
    REQUIRE_THAT (gain.at ("value").get<double>(), WithinAbs (-3.0, 0.01));
    REQUIRE (gain.at ("unit") == "dB");

    const auto compressorParameterCalled = [&] (const std::string& id)
    {
        for (const auto& parameter : plugins.at (1).at ("parameters"))
            if (parameter.at ("paramId") == id)
                return parameter;

        return Json::object();
    };

    // A unit is only a unit when the plugin's own text is about this value. The
    // compressor's attack is milliseconds and says so; its ratio is held as
    // 0.05 and displayed as "20.00 : 1", and calling ": 1" the unit of 0.05
    // would tell the Collaborator the ratio is one to twenty.
    REQUIRE (compressorParameterCalled ("attack").at ("unit") == "ms");
    REQUIRE (compressorParameterCalled ("ratio").at ("unit").get<std::string>().empty());
    REQUIRE (compressorParameterCalled ("threshold").at ("unit").get<std::string>().empty());
}

TEST_CASE ("every tool in the vocabulary answers, and a name outside it is an error the run "
           "survives",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildBassProject (session);

    const ToolRun run { session,
                        Json::array ({ toolCall ("list_tracks"),
                                       toolCall ("get_arrangement"),
                                       toolCall ("get_midi", built.bass),
                                       toolCall ("get_automation", built.bass),
                                       toolCall ("get_plugin_chain", built.bass),
                                       toolCall ("hear_the_drop"),
                                       toolCall ("list_tracks") }) };

    REQUIRE (run.finished());

    REQUIRE (run.result (0).contains ("tracks"));

    for (const auto& field : { "tempoBpm", "timeSignature", "barCount", "sections", "placements" })
    {
        INFO (field);
        REQUIRE (run.result (1).contains (field));
    }

    REQUIRE (run.result (2).contains ("clips"));
    REQUIRE (run.result (3).contains ("lanes"));
    REQUIRE (run.result (4).contains ("plugins"));

    REQUIRE (run.error (5).at ("code") == duet::collab::rpcError::unknownTool);

    // The run survives it: the next call is answered as if nothing had happened.
    REQUIRE (run.result (6).contains ("tracks"));
}

TEST_CASE ("an id the project does not hold is an error, never a crash and never an empty success",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildBassProject (session);

    Json strangerClip = Json::object();
    strangerClip["trackId"] = duet::collab::toolId::forTrack (built.bass);
    strangerClip["clipId"] = "clip-999999";

    // A number where an id belongs: not a track this project has, and not a
    // reason to stop answering either.
    Json numberedTrack = Json::object();
    numberedTrack["trackId"] = 7;

    const ToolRun run { session,
                        Json::array (
                            { toolCall ("get_midi", Json { { "trackId", "track-999999" } }),
                              toolCall ("get_plugin_chain", Json { { "trackId", "the low one" } }),
                              toolCall ("get_automation", Json::object()),
                              toolCall ("get_automation", numberedTrack),
                              toolCall ("get_midi", strangerClip),
                              toolCall ("list_tracks") }) };

    REQUIRE (run.finished());

    for (std::size_t call = 0; call < 5; ++call)
    {
        INFO ("call " << call);
        REQUIRE (run.error (call).at ("code") == duet::collab::rpcError::invalidParams);
        REQUIRE_FALSE (run.error (call).at ("message").get<std::string>().empty());
        REQUIRE (run.result (call).empty());
    }

    REQUIRE (run.result (5).contains ("tracks"));
}

TEST_CASE ("every tool result is read on the message thread, and not on the service's", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildBassProject (session);
    const auto messageThread = std::this_thread::get_id();

    std::mutex mutex;
    std::vector<std::thread::id> readThreads;

    auto onward = duet::testing::messageThreadMarshal();

    const ToolRun run { session,
                        Json::array ({ toolCall ("list_tracks"),
                                       toolCall ("get_arrangement"),
                                       toolCall ("get_midi", built.bass),
                                       toolCall ("get_automation", built.bass),
                                       toolCall ("get_plugin_chain", built.bass) }),
                        [&] (const std::function<void()>& work)
                        {
                            onward (
                                [&]
                                {
                                    {
                                        const std::lock_guard lock (mutex);
                                        readThreads.push_back (std::this_thread::get_id());
                                    }

                                    work();
                                });
                        } };

    REQUIRE (run.finished());

    // One read per tool, each of them on the thread that owns the project model
    // and none of them on the thread the call arrived on.
    const std::lock_guard lock (mutex);

    REQUIRE (readThreads.size() == 5);

    for (const auto& thread : readThreads)
    {
        REQUIRE (thread == messageThread);
        REQUIRE (thread != run.serviceThread());
    }
}

TEST_CASE ("the transport keeps rolling while the Collaborator reads the project", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildBassProject (session);

    session.performAction ("Write a phrase",
                           [&] (auto& ops)
                           {
                               const auto clip =
                                   ops.insertMidiClip (built.bass, "phrase", 0.0, 8.0);

                               for (int note = 0; note < 8; ++note)
                                   ops.addNote (clip, 48 + note, note * 1.0, 0.9, 90);
                           });

    if (session.audioDeviceDescription().empty())
        SKIP ("this machine has no audio device to play through");

    REQUIRE (duet::testing::playUntilRolling (session));

    // Hazard 6, got out of the way first: the engine rebuilds its device list
    // once and that stops the transport, and a stop it caused must not read as
    // one a tool call caused.
    session.rebuildDevices();
    REQUIRE (duet::testing::playUntilRolling (session));

    const auto before = session.playbackPositionSeconds();

    const ToolRun run { session,
                        Json::array ({ toolCall ("list_tracks"),
                                       toolCall ("get_arrangement"),
                                       toolCall ("get_midi", built.bass),
                                       toolCall ("get_automation", built.bass),
                                       toolCall ("get_plugin_chain", built.bass) }) };

    REQUIRE (run.finished());

    // Reading the project is not something a producer waits through: the
    // transport is still rolling and the playhead has moved on.
    REQUIRE (session.isPlaying());
    REQUIRE (session.playbackPositionSeconds() > before);
}

TEST_CASE ("a tool result is the same bytes for the same project state, whenever it is asked",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildBassProject (session);

    const auto calls = Json::array ({ toolCall ("list_tracks"),
                                      toolCall ("get_arrangement"),
                                      toolCall ("get_midi", built.bass),
                                      toolCall ("get_automation", built.bass),
                                      toolCall ("get_plugin_chain", built.bass) });

    const ToolRun first { session, calls };

    REQUIRE (first.finished());

    // More than a second of wall clock between the two, so that a timestamp
    // anywhere in a result — at any resolution a clock has — would show up as
    // different bytes.
    duet::testing::pumpMessages (1100);

    const ToolRun second { session, calls };

    REQUIRE (second.finished());

    for (std::size_t call = 0; call < 5; ++call)
    {
        INFO ("call " << call);
        REQUIRE (first.result (call).dump() == second.result (call).dump());
    }

    const auto keysOf = [] (const Json& object)
    {
        std::vector<std::string> keys;

        for (const auto& member : object.items())
            keys.push_back (member.key());

        return keys;
    };

    // Stable content first and the content an edit moves last, which is what
    // makes a fader change invalidate the tail of a cached result and not its
    // middle.
    const auto track = keysOf (trackEntry (first.result (0), built.bass));

    REQUIRE (track.front() == "id");
    REQUIRE (track.back() == "mixer");

    const auto arrangement = keysOf (first.result (1));

    REQUIRE (arrangement.front() == "tempoBpm");
    REQUIRE (arrangement.back() == "placements");
}

TEST_CASE ("a scanned plugin's own words about its value cross the seam wrapped", "[collab]")
{
    const TempProject project;
    const auto pluginDirectory = project.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);
    std::filesystem::copy (DUET_GOOD_VST3_FIXTURE,
                           pluginDirectory
                               / std::filesystem::path { DUET_GOOD_VST3_FIXTURE }.filename(),
                           std::filesystem::copy_options::recursive
                               | std::filesystem::copy_options::overwrite_existing);

    Session session { project.editFile() };

    if (! session.canHostVst3() || ! session.scanVst3Plugins (pluginDirectory).completed)
        SKIP ("this build cannot scan VST3s");

    const auto known = session.knownVst3Plugins();
    const auto scanned = std::find_if (known.begin(),
                                       known.end(),
                                       [] (const duet::model::KnownPluginInfo& plugin)
                                       { return plugin.name == "Duet Good VST3 Fixture"; });

    REQUIRE (scanned != known.end());

    TrackRef track = duet::model::noTrack;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::audio, "Tone");
                               ops.addPlugin (track, scanned->identifier, 0);
                           });

    const ToolRun run { session, Json::array ({ toolCall ("get_plugin_chain", track) }) };

    REQUIRE (run.finished());

    const auto& plugins = run.result (0).at ("plugins");

    REQUIRE (plugins.size() == 1);
    REQUIRE (plugins.at (0).at ("format") == "vst3");

    const auto& parameters = plugins.at (0).at ("parameters");

    REQUIRE_FALSE (parameters.empty());

    // Duet owns what its own devices mean and hands their values over bare. It
    // does not own this one's, so the text that says what the number means is
    // wrapped, and the Collaborator can tell a fact from a guess by its shape.
    const auto& parameter = parameters.at (0);

    REQUIRE (parameter.at ("normalizedValue").is_number());
    REQUIRE (parameter.at ("vendorName").is_string());
    REQUIRE (parameter.at ("displayString").at ("source") == "estimated");
    REQUIRE (parameter.at ("displayString").at ("value").is_string());
    REQUIRE_FALSE (parameter.at ("displayString").at ("method").get<std::string>().empty());
    REQUIRE (parameter.at ("displayString").at ("confidence").is_number());
    REQUIRE_FALSE (parameter.contains ("unit"));
}
