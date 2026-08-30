#include "ProjectToolsHarness.h"
#include "Vst3FixtureHarness.h"

#include <duet/persistence/Project.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
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

/** The parameters of one chain entry that carry the vendor's own meaning.

    The engine gives every plugin Duet hosts two parameters of its own, so a
    test about what the vendor said looks for the vendor's shape rather than
    taking the first parameter the list holds.
*/
std::vector<Json> vendorParameters (const Json& entry)
{
    std::vector<Json> theirs;

    for (const auto& parameter : entry.at ("parameters"))
        if (parameter.contains ("vendorName"))
            theirs.push_back (parameter);

    return theirs;
}

/** The same list, read off the model rather than off the seam. */
std::vector<duet::model::PluginParameterInfo> vendorParameters (const Session& session,
                                                                PluginRef plugin)
{
    std::vector<duet::model::PluginParameterInfo> theirs;

    for (const auto& parameter : session.pluginParameters (plugin))
        if (! parameter.duetOwnsMeaning)
            theirs.push_back (parameter);

    return theirs;
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
                                     { AutomationPoint { 0.0, 4.0, 0.0 } });
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

    // One scale per parameter, wherever it is read: a lane on the ratio carries
    // the four get_plugin_chain reports, not the quarter the engine holds.
    REQUIRE_THAT (ratio.at ("points").at (0).at ("value").get<double>(), WithinAbs (4.0, 0.001));
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
                               ops.setPluginParameter (compressor, "ratio", 4.0);
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

    // Duet ships the compressor, so it says what each of its numbers is: the
    // attack is milliseconds, the ratio is a ratio, and the threshold is the
    // decibels the producer reads, not the gain the engine keeps underneath.
    REQUIRE (compressorParameterCalled ("attack").at ("unit") == "ms");
    REQUIRE (compressorParameterCalled ("ratio").at ("unit") == ":1");
    REQUIRE (compressorParameterCalled ("threshold").at ("unit") == "dB");

    // Four to one, read as the number that would be written to set it, with the
    // two ends of what may be written beside it: everything the Collaborator
    // needs to change a parameter it has just read, and nothing it has to
    // derive.
    const auto ratio = compressorParameterCalled ("ratio");

    REQUIRE_THAT (ratio.at ("value").get<double>(), WithinAbs (4.0, 0.001));
    REQUIRE_THAT (ratio.at ("min").get<double>(), WithinAbs (1.0526, 0.001));
    REQUIRE_THAT (ratio.at ("max").get<double>(), WithinAbs (1000.0, 0.001));

    const auto frequencyRange = parameterCalled ("Low-pass freq");

    REQUIRE_THAT (frequencyRange.at ("min").get<double>(), WithinAbs (20.0, 0.001));
    REQUIRE_THAT (frequencyRange.at ("max").get<double>(), WithinAbs (20000.0, 0.001));
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
    // transport is still rolling. That is asked before anything here pumps the
    // message loop, so a stop a tool call caused cannot be asked away by the
    // model's own retry first.
    REQUIRE (session.isPlaying());

    // The position the transport publishes moves in steps — the message thread
    // publishes it, and a step here is one audio block (engine notes, further
    // facts) — and five reads can cost less than one step. So the playhead
    // moving on is waited for, not assumed from the run having been long
    // enough. A tool call that stopped the transport or froze the playhead
    // never gets past this, and the wait runs out instead.
    REQUIRE (duet::testing::pumpUntil ([&] { return session.playbackPositionSeconds() > before; }));
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
    Session session { project.editFile() };

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, project.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        session, bundle.parent_path(), duet::testing::goodVst3FixtureName);

    TrackRef track = duet::model::noTrack;
    PluginRef hosted = duet::model::noPlugin;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::audio, "Tone");
                               hosted = ops.addPlugin (track, fixture.identifier, 0);
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
    const auto theirs = vendorParameters (plugins.at (0));

    REQUIRE_FALSE (theirs.empty());

    const auto& parameter = theirs.front();

    REQUIRE (parameter.at ("normalizedValue").is_number());
    REQUIRE (parameter.at ("vendorName").is_string());
    REQUIRE (duet::testing::isAnEstimate (parameter.at ("displayString")));
    REQUIRE_FALSE (parameter.contains ("unit"));

    // The plugin's own text, unaltered, and a method that says exactly that:
    // what the model reads is what the plugin said, and what it means is the
    // plugin's to say.
    const auto held = vendorParameters (session, hosted);

    REQUIRE (session.pluginParameters (hosted).size() == parameters.size());
    REQUIRE (held.size() == theirs.size());
    REQUIRE (parameter.at ("displayString").at ("value") == held.front().displayValue);
    REQUIRE (parameter.at ("displayString").at ("method") == duet::collab::displayStringMethod);
}

TEST_CASE ("the two parameters the engine adds to a hosted plugin cross as Duet's own", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, project.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        session, bundle.parent_path(), duet::testing::goodVst3FixtureName);

    TrackRef track = duet::model::noTrack;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::audio, "Tone");
                               ops.addPlugin (track, fixture.identifier, 0);
                           });

    const ToolRun run { session, Json::array ({ toolCall ("get_plugin_chain", track) }) };

    REQUIRE (run.finished());

    const auto& parameters = run.result (0).at ("plugins").at (0).at ("parameters");

    const auto withId = [&] (const std::string& parameterId)
    {
        for (const auto& each : parameters)
            if (each.at ("paramId") == parameterId)
                return each;

        return Json::object();
    };

    // The engine gives every plugin it hosts a dry and a wet level of its own,
    // ahead of the ones the plugin declares. Those two are Duet's dependency
    // and not the plugin, so they cross exactly as a built-in's parameters do:
    // a name, a number in the unit it is measured in, and the two ends it moves
    // between.
    const auto wet = withId (duet::model::hostedWetLevelParameterId);

    REQUIRE (wet.at ("name") == "Wet Level");
    REQUIRE (wet.at ("unit") == "dB");
    REQUIRE_THAT (wet.at ("value").get<double>(), WithinAbs (0.0, decibelTolerance));
    REQUIRE_THAT (wet.at ("min").get<double>(),
                  WithinAbs (duet::model::hostedLevelMinimumDb, decibelTolerance));
    REQUIRE_THAT (wet.at ("max").get<double>(),
                  WithinAbs (duet::model::hostedLevelMaximumDb, decibelTolerance));

    const auto dry = withId (duet::model::hostedDryLevelParameterId);

    REQUIRE (dry.at ("name") == "Dry Level");
    REQUIRE (dry.at ("unit") == "dB");
    REQUIRE_THAT (dry.at ("value").get<double>(),
                  WithinAbs (duet::model::hostedLevelMinimumDb, decibelTolerance));

    // Nothing about either of them is the vendor's, and nothing about either is
    // a guess: the name the engine gave it is not a vendor name, and the words
    // that explain its number are the engine's own.
    for (const auto& ours : { wet, dry })
    {
        REQUIRE_FALSE (ours.contains ("vendorName"));
        REQUIRE_FALSE (ours.contains ("normalizedValue"));
        REQUIRE_FALSE (ours.contains ("displayString"));
    }

    // The vendor's parameters are untouched by that: the two shapes sit side by
    // side in one plugin's list, and which one a parameter is in says whose
    // meaning it carries.
    const auto theirs = vendorParameters (run.result (0).at ("plugins").at (0));

    REQUIRE_FALSE (theirs.empty());
    REQUIRE (theirs.front().at ("vendorName") == "Gain");
    REQUIRE (duet::testing::isAnEstimate (theirs.front().at ("displayString")));
    REQUIRE_FALSE (theirs.front().contains ("unit"));
}

TEST_CASE ("reading a scanned plugin's parameters marks the run, and reading built-ins does not",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, project.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        session, bundle.parent_path(), duet::testing::goodVst3FixtureName);

    TrackRef hosting = duet::model::noTrack;
    TrackRef ours = duet::model::noTrack;
    PluginRef hosted = duet::model::noPlugin;

    session.performAction ("Build both chains",
                           [&] (auto& ops)
                           {
                               hosting = ops.createTrack (TrackKind::audio, "Tone");
                               hosted = ops.addPlugin (hosting, fixture.identifier, 0);

                               ours = ops.createTrack (TrackKind::audio, "Bass");
                               ops.addPlugin (ours, BuiltinPlugin::eq, 0);
                           });

    duet::collab::EstimateLedger ledger;
    duet::testing::ToolRunOptions options;
    options.ledger = &ledger;

    const ToolRun builtins { session,
                             Json::array (
                                 { toolCall ("get_plugin_chain", ours),
                                   duet::testing::toolCommentary ("the EQ is doing it.") }),
                             options };

    REQUIRE (builtins.finished());
    REQUIRE (builtins.markedCommentary().empty());
    REQUIRE (ledger.entries (builtins.id()).empty());
    REQUIRE_FALSE (ledger.basedOnEstimates (builtins.id()));

    const ToolRun scanned { session,
                            Json::array ({ duet::testing::toolCommentary ("before. "),
                                           toolCall ("get_plugin_chain", hosting),
                                           duet::testing::toolCommentary ("after.") }),
                            options };

    REQUIRE (scanned.finished());

    // The plugin's own words about its value are a guess, so the run that was
    // handed them is marked from there on, exactly as a guessed key marks one.
    REQUIRE (scanned.commentary() == "before. after.");
    REQUIRE (scanned.markedCommentary() == "after.");

    const auto held = vendorParameters (session, hosted);
    const auto entries = ledger.entries (scanned.id());

    // One line for each parameter whose meaning is the vendor's, and none for
    // the two the engine added: what marks a run is a guess, and the engine
    // saying in decibels what its own number means is not one.
    REQUIRE (session.pluginParameters (hosted).size() == held.size() + 2);
    REQUIRE (entries.size() == held.size());
    REQUIRE (entries.front().tool == "get_plugin_chain");
    REQUIRE (entries.front().field
             == duet::collab::toolId::forPlugin (hosted) + "." + held.front().parameterId
                    + ".displayString");
    REQUIRE (entries.front().estimate.method == duet::collab::displayStringMethod);
    REQUIRE (entries.front().estimate.value == held.front().displayValue);

    // What the ledger holds is what crossed the seam, so the two say the same
    // thing about the same value.
    REQUIRE (duet::collab::wrapped (entries.front().estimate)
             == vendorParameters (scanned.result (0).at ("plugins").at (0))
                    .front()
                    .at ("displayString"));
}

TEST_CASE ("a built-in and a hosted VST3 in one chain are each read in the terms Duet owns",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, project.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        session, bundle.parent_path(), duet::testing::goodVst3FixtureName);

    TrackRef track = duet::model::noTrack;
    PluginRef compressor = duet::model::noPlugin;
    PluginRef hosted = duet::model::noPlugin;

    session.performAction ("Build the chain",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::audio, "Tone");
                               compressor = ops.addPlugin (track, BuiltinPlugin::compressor, 0);
                               hosted = ops.addPlugin (track, fixture.identifier, 1);

                               ops.setPluginParameter (compressor, "ratio", 4.0);
                           });

    const ToolRun run { session, Json::array ({ toolCall ("get_plugin_chain", track) }) };

    REQUIRE (run.finished());

    const auto& plugins = run.result (0).at ("plugins");

    REQUIRE (plugins.size() == 2);

    const auto& ours = plugins.at (0);
    const auto& theirs = plugins.at (1);

    // Which kind of plugin each one is, said outright rather than left to be
    // inferred from the shape of its parameters.
    REQUIRE (ours.at ("pluginId") == duet::collab::toolId::forPlugin (compressor));
    REQUIRE (ours.at ("name") == "Compressor");
    REQUIRE (ours.at ("format") == "builtin");
    REQUIRE (ours.at ("available") == true);
    REQUIRE (ours.at ("latencySamples").is_number());

    REQUIRE (theirs.at ("pluginId") == duet::collab::toolId::forPlugin (hosted));
    REQUIRE (theirs.at ("name") == duet::testing::goodVst3FixtureName);
    REQUIRE (theirs.at ("format") == "vst3");
    REQUIRE (theirs.at ("available") == true);
    REQUIRE (theirs.at ("latencySamples").is_number());

    // Duet ships the compressor, so every one of its numbers crosses bare, in
    // the units the producer reads, and nothing about it anywhere is a guess.
    REQUIRE_FALSE (duet::testing::holdsAnEstimate (ours));

    const auto ratio = [&]
    {
        for (const auto& parameter : ours.at ("parameters"))
            if (parameter.at ("paramId") == "ratio")
                return parameter;

        return Json::object();
    }();

    REQUIRE (ratio.at ("name").is_string());
    REQUIRE (ratio.at ("unit") == ":1");
    REQUIRE_THAT (ratio.at ("value").get<double>(), WithinAbs (4.0, 0.001));

    // The fixture's meaning is its vendor's, so its parameter carries the
    // vendor's own name, the plugin's own normalised number, and its own words
    // about what that number means — wrapped, because the words are a guess.
    const auto parameter = [&]
    {
        for (const auto& each : vendorParameters (theirs))
            if (each.at ("vendorName") == "Gain")
                return each;

        return Json::object();
    }();

    REQUIRE (parameter.at ("vendorName") == "Gain");
    REQUIRE (parameter.at ("normalizedValue").get<double>() >= 0.0);
    REQUIRE (parameter.at ("normalizedValue").get<double>() <= 1.0);
    REQUIRE (duet::testing::isAnEstimate (parameter.at ("displayString")));

    // The two shapes do not overlap: a vendor's parameter has no unit and no
    // real-unit value, so nothing about it can be read as Duet's own.
    REQUIRE_FALSE (parameter.contains ("unit"));
    REQUIRE_FALSE (parameter.contains ("value"));
    REQUIRE_FALSE (parameter.contains ("name"));
}

TEST_CASE ("a plugin the machine no longer has is in the chain, named and unavailable", "[collab]")
{
    const TempProject temp;
    const auto projectFolder = temp.folder() / "project";
    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, temp.folder() / "vst3");

    {
        const auto made = duet::persistence::Project::create (projectFolder);

        REQUIRE (made != nullptr);

        auto& session = made->session();
        const auto fixture = duet::testing::scanVst3Fixture (
            session, bundle.parent_path(), duet::testing::goodVst3FixtureName);

        session.performAction ("Build the chain",
                               [&] (auto& ops)
                               {
                                   const auto track = ops.createTrack (TrackKind::audio, "Tone");
                                   ops.addPlugin (track, fixture.identifier, 0);
                                   ops.addPlugin (track, BuiltinPlugin::eq, 1);
                               });

        REQUIRE (made->save());
    }

    std::filesystem::remove_all (bundle);

    const auto reopened = duet::persistence::Project::open (projectFolder);

    REQUIRE (reopened != nullptr);

    auto& session = reopened->session();
    const auto track = session.tracks().back().track;

    REQUIRE (session.track (track).plugins.front().missing);

    // Back on disk before the tool call, and nothing may go and fetch it: a
    // tool answers about the project as the producer left it, and loading a
    // plugin is the producer's own act.
    duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, temp.folder() / "vst3");

    const auto knownBefore = session.knownVst3Plugins().size();

    const ToolRun run { session, Json::array ({ toolCall ("get_plugin_chain", track) }) };

    REQUIRE (run.finished());

    const auto& plugins = run.result (0).at ("plugins");

    // Never omitted silently: a chain missing one of its links is a different
    // chain, and the Collaborator is told which link and that it is not there.
    REQUIRE (plugins.size() == 2);
    REQUIRE (plugins.at (0).at ("name") == duet::testing::goodVst3FixtureName);
    REQUIRE (plugins.at (0).at ("format") == "vst3");
    REQUIRE (plugins.at (0).at ("available") == false);

    REQUIRE (plugins.at (1).at ("name") == "4-Band Equaliser");
    REQUIRE (plugins.at (1).at ("available") == true);

    // No scan and no load happened behind the call: the known list is what the
    // scan left it, and the plugin the tool just reported is still the one the
    // project opened with.
    REQUIRE (session.knownVst3Plugins().size() == knownBefore);
    REQUIRE (session.track (track).plugins.front().missing);
}

TEST_CASE ("the engine's own parameters write no line into the run's estimate ledger", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, project.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        session, bundle.parent_path(), duet::testing::goodVst3FixtureName);

    TrackRef track = duet::model::noTrack;
    PluginRef hosted = duet::model::noPlugin;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::audio, "Tone");
                               hosted = ops.addPlugin (track, fixture.identifier, 0);
                           });

    duet::collab::EstimateLedger ledger;
    duet::testing::ToolRunOptions options;
    options.ledger = &ledger;

    const ToolRun run { session, Json::array ({ toolCall ("get_plugin_chain", track) }), options };

    REQUIRE (run.finished());

    // The ledger is per value handed over, so what it holds says which values
    // were guesses. The engine's own two are not among them: a chain read that
    // handed the model nothing but those would leave the ledger empty and the
    // run unmarked. That plugin is not this one and cannot be a fixture — JUCE's
    // VST3 wrapper gives a processor with no parameters a "Bypass" of its own,
    // so every fixture declares something — and the ledger is where the claim
    // is decided anyway, one line to a value.
    const auto theirs = vendorParameters (session, hosted);
    const auto entries = ledger.entries (run.id());

    REQUIRE (entries.size() == theirs.size());

    for (const auto& entry : entries)
    {
        REQUIRE (entry.field.find (duet::model::hostedDryLevelParameterId) == std::string::npos);
        REQUIRE (entry.field.find (duet::model::hostedWetLevelParameterId) == std::string::npos);
    }
}

TEST_CASE ("a hosted plugin that raises when read is confined to its own chain entry", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_RAISING_VST3_FIXTURE, project.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        session, bundle.parent_path(), duet::testing::raisingVst3FixtureName);

    TrackRef track = duet::model::noTrack;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::audio, "Tone");
                               ops.addPlugin (track, fixture.identifier, 0);
                               ops.addPlugin (track, BuiltinPlugin::eq, 1);
                           });

    // The plugin is hosted and behaving, and turns hostile only now: what is
    // asserted is a read failing on a plugin the producer already has in a
    // chain, not a plugin that could never be loaded at all.
    duet::testing::raiseWhenRead (bundle);

    const ToolRun run { session,
                        Json::array ({ toolCall ("get_plugin_chain", track),
                                       toolCall ("get_arrangement") }) };

    // The run reached its ending, which is the whole of it: the read raised on
    // the message thread, and the thread that would have carried the raise into
    // the DAW confined it to the plugin that raised instead.
    REQUIRE (run.finished());

    const auto& plugins = run.result (0).at ("plugins");

    // The chain is still a chain: both links, in the producer's order, and the
    // one that would not answer says that of itself rather than of the call.
    REQUIRE (plugins.size() == 2);

    REQUIRE (plugins.at (0).at ("name") == duet::testing::raisingVst3FixtureName);
    REQUIRE (plugins.at (0).at ("available") == true);
    REQUIRE (plugins.at (0).at ("parametersReadable") == false);
    REQUIRE (plugins.at (0).at ("parameters").empty());

    REQUIRE (plugins.at (1).at ("name") == "4-Band Equaliser");
    REQUIRE (plugins.at (1).at ("parametersReadable") == true);
    REQUIRE (! plugins.at (1).at ("parameters").empty());

    // And the vocabulary still answers, which is what the run continuing means.
    REQUIRE (run.result (1).contains ("tempoBpm"));
}

TEST_CASE ("the track list answers for a project holding a plugin that raises when read",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_RAISING_VST3_FIXTURE, project.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        session, bundle.parent_path(), duet::testing::raisingVst3FixtureName);

    const auto built = buildBassProject (session);
    TrackRef tone = duet::model::noTrack;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               tone = ops.createTrack (TrackKind::audio, "Tone");
                               ops.addPlugin (tone, fixture.identifier, 0);
                               ops.setTrackVolumeDb (tone, -3.0);
                               ops.setAutomationPoints (AutomationTarget::trackPanOf (tone),
                                                        { AutomationPoint { 0.0, -1.0, 0.0 },
                                                          AutomationPoint { 4.0, 1.0, 0.0 } });
                           });

    duet::testing::raiseWhenRead (bundle);

    const ToolRun run {
        session, Json::array ({ toolCall ("list_tracks"), toolCall ("get_automation", tone) })
    };

    REQUIRE (run.finished());

    // Every track, not every track but that one: the list is what tells the
    // Collaborator what a project is made of, and one hostile plugin on one
    // track is not an answer the other tracks deserve to lose.
    const auto bass = trackEntry (run.result (0), built.bass);

    REQUIRE (bass.at ("pluginNames") == Json::array ({ "4OSC", "4-Band Equaliser" }));
    REQUIRE (bass.at ("automatedParameters") == Json::array ({ "volume" }));

    const auto entry = trackEntry (run.result (0), tone);

    // The plugin is on the track and named there, its own curves are the only
    // ones missing, and the curves the project itself owns are still answered.
    REQUIRE (entry.at ("pluginNames") == Json::array ({ duet::testing::raisingVst3FixtureName }));
    REQUIRE (entry.at ("automatedParameters") == Json::array ({ "pan" }));
    REQUIRE_THAT (entry.at ("mixer").at ("volumeDb").get<double>(),
                  WithinAbs (-3.0, decibelTolerance));

    // The lanes read the same way, since it is the same question asked in more
    // detail: the curve the producer drew, with its points, and nothing about
    // the plugin that would not say what it has.
    const auto& lanes = run.result (1).at ("lanes");

    REQUIRE (lanes.size() == 1);
    REQUIRE (lanes.at (0).at ("target").at ("kind") == "pan");
    REQUIRE (lanes.at (0).at ("points").size() == 2);
}
