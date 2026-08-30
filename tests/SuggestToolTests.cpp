#include "ProjectToolsHarness.h"
#include "Vst3FixtureHarness.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>

using duet::collab::Json;
using duet::model::BuiltinPlugin;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::suggestCall;
using duet::testing::suggestElement;
using duet::testing::TempProject;
using duet::testing::ToolRun;

namespace
{
/** One operation of the edit vocabulary, as a call writes it. */
Json operation (const std::string& name, Json fields = Json::object())
{
    Json out = Json::object();
    out["op"] = name;

    for (const auto& field : fields.items())
        out[field.key()] = field.value();

    return out;
}

TrackRef bassTrackOf (Session& session)
{
    TrackRef bass = duet::model::noTrack;

    session.performAction ("Build the example",
                           [&] (auto& ops) {
                               bass =
                                   ops.createTrack (TrackKind::midi, "Bass", BuiltinPlugin::synth);
                           });

    return bass;
}
} // namespace

TEST_CASE ("a suggest call makes a Suggestion and leaves the project untouched", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bass = bassTrackOf (session);

    const auto digestBefore = session.stateDigest();
    const auto undoBefore = session.undoNames();
    const auto redoBefore = session.redoNames();

    const ToolRun run {
        session,
        Json::array ({ suggestCall (
            "Pull the bass down a little",
            Json::array ({ suggestElement (
                "Trim the bass fader to -3 dB",
                Json::array ({ operation ("mixer.set",
                                          { { "trackId", duet::collab::toolId::forTrack (bass) },
                                            { "volumeDb", -3.0 } }) })) })) })
    };

    REQUIRE (run.finished());
    REQUIRE (run.error (0).empty());
    REQUIRE (run.result (0).contains ("suggestionId"));

    const auto made = run.suggestion (0);

    REQUIRE_FALSE (made.id.empty());
    REQUIRE (made.summary == "Pull the bass down a little");
    REQUIRE (made.elements.size() == 1);
    REQUIRE (made.elements.front().description == "Trim the bass fader to -3 dB");
    REQUIRE (made.elements.front().operations.size() == 1);
    REQUIRE (made.elements.front().operations.front().at ("op") == "mixer.set");
    REQUIRE (made.elements.front().operations.front().at ("volumeDb") == -3.0);

    REQUIRE (session.stateDigest() == digestBefore);
    REQUIRE (session.undoNames() == undoBefore);
    REQUIRE (session.redoNames() == redoBefore);
}

namespace
{
/** A project with one of everything the edit vocabulary can name. */
struct Corpus
{
    TrackRef bass = duet::model::noTrack;
    TrackRef drums = duet::model::noTrack;
    TrackRef bus = duet::model::noTrack;
    duet::model::ClipRef riff = duet::model::noClip;
    duet::model::ClipRef loop = duet::model::noClip;
    duet::model::PluginRef eq = duet::model::noPlugin;
    duet::model::PluginRef compressor = duet::model::noPlugin;
    duet::model::NoteRef note = duet::model::noNote;
};

Corpus buildCorpus (Session& session, const TempProject& project)
{
    const auto tone = project.writeTone ("loop.wav", 2.0, 220.0);
    Corpus built;

    session.performAction (
        "Build the corpus",
        [&] (auto& ops)
        {
            built.bass = ops.createTrack (TrackKind::midi, "Bass", BuiltinPlugin::synth);
            built.drums = ops.createTrack (TrackKind::audio, "Drums");
            built.bus = ops.createTrack (TrackKind::group, "Low Bus");

            built.riff = ops.insertMidiClip (built.bass, "riff", 0.0, 4.0);
            ops.addNote (built.riff, 36, 0.0, 1.0, 100);
            built.loop = ops.insertAudioClip (built.drums, "loop", tone, 0.0, 2.0);

            built.eq = ops.addPlugin (built.bass, BuiltinPlugin::eq, 0);
            built.compressor = ops.addPlugin (built.bass, BuiltinPlugin::compressor, 1);
        });

    built.note = session.notes (built.riff).front().note;

    return built;
}

/** The scanned VST3 fixture's known-list identifier, and nothing when this
    build cannot scan one.
*/
std::optional<std::string> scannedFixture (Session& session, const TempProject& project)
{
    const auto pluginDirectory = project.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);
    std::filesystem::copy (DUET_GOOD_VST3_FIXTURE,
                           pluginDirectory
                               / std::filesystem::path { DUET_GOOD_VST3_FIXTURE }.filename(),
                           std::filesystem::copy_options::recursive
                               | std::filesystem::copy_options::overwrite_existing);

    if (! session.canHostVst3() || ! session.scanVst3Plugins (pluginDirectory).completed)
        return {};

    const auto known = session.knownVst3Plugins();
    const auto scanned = std::find_if (known.begin(),
                                       known.end(),
                                       [] (const duet::model::KnownPluginInfo& plugin)
                                       { return plugin.name == "Duet Good VST3 Fixture"; });

    return scanned == known.end() ? std::nullopt : std::optional { scanned->identifier };
}

std::string trackId (TrackRef track) { return duet::collab::toolId::forTrack (track); }
std::string clipId (duet::model::ClipRef clip) { return duet::collab::toolId::forClip (clip); }
std::string pluginId (duet::model::PluginRef plugin)
{
    return duet::collab::toolId::forPlugin (plugin);
}
} // namespace

TEST_CASE ("every operation domain round-trips into a Suggestion and reads back identically",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildCorpus (session, project);
    const auto noteId = duet::collab::toolId::forNote (built.note);
    const auto parameter = session.pluginParameters (built.eq).front();

    const auto midi = Json::array (
        { operation ("midi.addNotes",
                     { { "clipId", clipId (built.riff) },
                       { "notes",
                         Json::array ({ Json { { "pitch", 43 },
                                               { "startBeats", 1.0 },
                                               { "lengthBeats", 0.5 },
                                               { "velocity", 96 },
                                               { "ref", "#fifth" } } }) } }),
          operation ("midi.modifyNotes",
                     { { "clipId", clipId (built.riff) },
                       { "changes",
                         Json::array ({ Json { { "noteId", "#fifth" }, { "velocity", 110 } },
                                        Json { { "noteId", noteId },
                                               { "pitch", 38 },
                                               { "startBeats", 0.5 },
                                               { "lengthBeats", 0.25 } } }) } }),
          operation (
              "midi.removeNotes",
              { { "clipId", clipId (built.riff) }, { "noteIds", Json::array ({ noteId }) } }) });

    const auto clips = Json::array (
        { operation ("clip.createMidi",
                     { { "trackId", trackId (built.bass) },
                       { "startBar", 5 },
                       { "lengthBars", 2 },
                       { "name", "Turnaround" },
                       { "ref", "#turnaround" } }),
          operation ("clip.move", { { "clipId", "#turnaround" }, { "startBar", 7 } }),
          operation ("clip.trim",
                     { { "clipId", "#turnaround" }, { "startBar", 7 }, { "lengthBars", 1 } }),
          operation ("clip.setLoop",
                     { { "clipId", "#turnaround" }, { "looped", true }, { "loopLengthBars", 1 } }),
          operation ("clip.duplicate",
                     { { "clipId", "#turnaround" },
                       { "toTrackId", trackId (built.bass) },
                       { "atBar", 9 },
                       { "ref", "#again" } }),
          operation ("clip.delete", { { "clipId", "#again" } }),
          operation ("clip.move",
                     { { "clipId", clipId (built.loop) },
                       { "trackId", trackId (built.drums) },
                       { "startBar", 3 } }) });

    const auto tracks = Json::array (
        { operation ("track.create",
                     { { "kind", "midi" },
                       { "name", "Pad" },
                       { "instrument", "synth" },
                       { "ref", "#pad" } }),
          operation ("track.rename", { { "trackId", "#pad" }, { "name", "Warm Pad" } }),
          operation ("track.setOutput",
                     { { "trackId", "#pad" }, { "busId", trackId (built.bus) } }),
          operation ("track.delete", { { "trackId", "#pad" } }) });

    const auto mixer = Json::array ({ operation ("mixer.set",
                                                 { { "trackId", trackId (built.bass) },
                                                   { "volumeDb", -4.5 },
                                                   { "pan", -0.25 },
                                                   { "mute", false },
                                                   { "solo", false } }),
                                      operation ("mixer.setSend",
                                                 { { "trackId", trackId (built.bass) },
                                                   { "busId", trackId (built.bus) },
                                                   { "levelDb", -12.0 } }) });

    const auto plugins =
        Json::array ({ operation ("plugin.add",
                                  { { "trackId", trackId (built.bus) },
                                    { "position", 0 },
                                    { "plugin", Json { { "builtin", "reverb" } } },
                                    { "ref", "#reverb" } }),
                       operation ("plugin.reorder",
                                  { { "trackId", trackId (built.bass) },
                                    { "pluginId", pluginId (built.eq) },
                                    { "position", 1 } }),
                       operation ("plugin.setParam",
                                  { { "pluginId", pluginId (built.eq) },
                                    { "paramId", parameter.parameterId },
                                    { "value", parameter.value } }),
                       operation ("plugin.setSidechainSource",
                                  { { "pluginId", pluginId (built.compressor) },
                                    { "sourceTrackId", trackId (built.drums) } }),
                       operation ("plugin.remove", { { "pluginId", "#reverb" } }) });

    const auto automation = Json::array (
        { operation ("automation.setPoints",
                     { { "trackId", trackId (built.bass) },
                       { "target", Json { { "kind", "volume" } } },
                       { "points",
                         Json::array ({ Json { { "timeBeats", 0.0 }, { "value", -6.0 } },
                                        Json { { "timeBeats", 4.0 }, { "value", -3.0 } } }) } }),
          operation ("automation.removePoints",
                     { { "trackId", trackId (built.bass) },
                       { "target", Json { { "kind", "pan" } } },
                       { "range", Json::array ({ 0.0, 4.0 }) } }) });

    const auto wholeProject = Json::array (
        { operation ("project.setTempo", { { "bpm", 132.0 } }),
          operation ("project.setTimeSignature", { { "numerator", 6 }, { "denominator", 8 } }) });

    const auto elements =
        Json::array ({ suggestElement ("MIDI notes", midi),
                       suggestElement ("Clip lifecycle and placement", clips),
                       suggestElement ("Track lifecycle and routing", tracks),
                       suggestElement ("Mixer values and sends", mixer),
                       suggestElement ("Plugins", plugins),
                       suggestElement ("Automation points", automation),
                       suggestElement ("Tempo and time signature", wholeProject) });

    const ToolRun run {
        session, Json::array ({ suggestCall ("Everything the vocabulary can say", elements) })
    };

    REQUIRE (run.finished());
    INFO (run.error (0).dump());
    REQUIRE (run.result (0).contains ("suggestionId"));

    const auto made = run.suggestion (0);

    REQUIRE_FALSE (made.id.empty());
    REQUIRE (made.elements.size() == elements.size());

    for (std::size_t index = 0; index < elements.size(); ++index)
    {
        const auto& element = made.elements.at (index);
        const auto& sent = elements.at (index);

        REQUIRE (element.description == sent.at ("description").get<std::string>());
        REQUIRE (element.operations.size() == sent.at ("operations").size());

        for (std::size_t at = 0; at < element.operations.size(); ++at)
            REQUIRE (element.operations.at (at).dump() == sent.at ("operations").at (at).dump());
    }
}

TEST_CASE ("the vocabulary can move an audio clip but can never create one", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildCorpus (session, project);

    // Nothing that would bring audio into being is in the vocabulary, whatever
    // it is called: the Collaborator writes MIDI, and audio it can only arrange.
    for (const auto& invented : { "clip.createAudio",
                                  "clip.insertAudio",
                                  "clip.recordAudio",
                                  "audio.generate",
                                  "track.renderAudio" })
    {
        const ToolRun refused {
            session,
            Json::array (
                { suggestCall ("Make me a sound",
                               Json::array ({ suggestElement (
                                   "Bring audio into being",
                                   Json::array ({ operation (invented,
                                                             { { "trackId", trackId (built.drums) },
                                                               { "startBar", 1 },
                                                               { "lengthBars", 2 } }) })) })) })
        };

        REQUIRE (refused.finished());
        REQUIRE (refused.result (0).empty());
        REQUIRE_THAT (refused.error (0).at ("message").get<std::string>(),
                      Catch::Matchers::ContainsSubstring (invented));
        REQUIRE (refused.suggestions().empty());
    }

    // What it can do to an audio clip that is already there: move it, trim it,
    // loop it, duplicate it, delete it — and that is the whole of the list.
    const auto arranging = Json::array (
        { operation ("clip.move", { { "clipId", clipId (built.loop) }, { "startBar", 3 } }),
          operation ("clip.trim",
                     { { "clipId", clipId (built.loop) }, { "startBar", 3 }, { "lengthBars", 1 } }),
          operation (
              "clip.setLoop",
              { { "clipId", clipId (built.loop) }, { "looped", true }, { "loopLengthBars", 1 } }),
          operation ("clip.duplicate",
                     { { "clipId", clipId (built.loop) }, { "atBar", 5 }, { "ref", "#copy" } }),
          operation ("clip.delete", { { "clipId", "#copy" } }) });

    const ToolRun run { session,
                        Json::array ({ suggestCall ("Arrange what is already there",
                                                    Json::array ({ suggestElement (
                                                        "Move the loop about", arranging) })) }) };

    REQUIRE (run.finished());
    INFO (run.error (0).dump());
    REQUIRE (run.result (0).contains ("suggestionId"));
    REQUIRE (run.suggestion (0).elements.front().operations.size() == arranging.size());
}

TEST_CASE ("an operation naming a clip that is not there is refused, and a corrected retry is not",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildCorpus (session, project);
    const auto digestBefore = session.stateDigest();

    const auto element = [] (const std::string& clip)
    {
        return Json::array ({ suggestElement (
            "Pull the riff back a bar",
            Json::array (
                { operation ("mixer.set", { { "trackId", "track-master" }, { "volumeDb", -1.0 } }),
                  operation ("clip.move", { { "clipId", clip }, { "startBar", 2 } }) })) });
    };

    const ToolRun run { session,
                        Json::array (
                            { suggestCall ("Move the riff", element ("clip-4242")),
                              suggestCall ("Move the riff", element (clipId (built.riff))) }) };

    REQUIRE (run.finished());

    const auto refusal = run.error (0).at ("message").get<std::string>();

    REQUIRE_THAT (refusal, Catch::Matchers::ContainsSubstring ("clip-4242"));
    REQUIRE_THAT (refusal, Catch::Matchers::ContainsSubstring ("elements[0].operations[1]"));
    REQUIRE (run.result (0).empty());

    // The refused call made nothing: the corrected one is the run's first and
    // only Suggestion, and the project never moved for either of them.
    REQUIRE (run.result (1).contains ("suggestionId"));
    REQUIRE (run.suggestions().size() == 1);
    REQUIRE (run.suggestion (1).elements.front().operations.size() == 2);
    REQUIRE (session.stateDigest() == digestBefore);
}

TEST_CASE ("a mixer value outside the fader's travel is refused and one at its end is not",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildCorpus (session, project);

    const auto call = [&] (double volumeDb)
    {
        return suggestCall ("Set the bass fader",
                            Json::array ({ suggestElement (
                                "Set the bass fader",
                                Json::array ({ operation ("mixer.set",
                                                          { { "trackId", trackId (built.bass) },
                                                            { "volumeDb", volumeDb } }) })) }));
    };

    const ToolRun run { session,
                        Json::array ({ call (duet::model::faderMaximumDb + 6.0),
                                       call (duet::model::faderMaximumDb) }) };

    REQUIRE (run.finished());

    const auto refusal = run.error (0).at ("message").get<std::string>();

    REQUIRE_THAT (refusal, Catch::Matchers::ContainsSubstring ("volumeDb"));
    REQUIRE_THAT (refusal, Catch::Matchers::ContainsSubstring ("elements[0].operations[0]"));
    REQUIRE (run.result (0).empty());

    REQUIRE (run.result (1).contains ("suggestionId"));
    REQUIRE (run.suggestions().size() == 1);
}

TEST_CASE ("an element may build on itself, and may not build on another element", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    buildCorpus (session, project);

    const auto digestBefore = session.stateDigest();

    const auto onItsOwn =
        Json::array ({ operation ("track.create",
                                  { { "kind", "midi" },
                                    { "name", "Pad" },
                                    { "instrument", "synth" },
                                    { "ref", "#pad" } }),
                       operation ("clip.createMidi",
                                  { { "trackId", "#pad" },
                                    { "startBar", 1 },
                                    { "lengthBars", 2 },
                                    { "name", "Chords" },
                                    { "ref", "#chords" } }),
                       operation ("midi.addNotes",
                                  { { "clipId", "#chords" },
                                    { "notes",
                                      Json::array ({ Json { { "pitch", 60 },
                                                            { "startBeats", 0.0 },
                                                            { "lengthBeats", 2.0 },
                                                            { "velocity", 80 } } }) } }),
                       operation ("mixer.set", { { "trackId", "#pad" }, { "volumeDb", -8.0 } }) });

    const ToolRun run {
        session,
        Json::array ({ suggestCall ("Add a pad",
                                    Json::array ({ suggestElement ("Add the pad", onItsOwn) })) })
    };

    REQUIRE (run.finished());
    INFO (run.error (0).dump());
    REQUIRE (run.result (0).contains ("suggestionId"));

    // Independently applicable is the whole point of an element, so the proof is
    // that this one alone materializes what its own operations describe.
    const auto made = run.suggestion (0);

    REQUIRE_FALSE (made.id.empty());
    REQUIRE (session.auditionSuggestion (made.elements.front().changes));

    const auto pad = session.tracks().back();

    REQUIRE (pad.name == "Pad");
    REQUIRE (pad.clips.size() == 1);
    REQUIRE (session.notes (pad.clips.front().clip).size() == 1);

    session.stopAudition();
    REQUIRE (session.stateDigest() == digestBefore);

    // The same operations split across two elements are not applicable one at a
    // time, and that is what makes the second element's reference refusable.
    const ToolRun split {
        session,
        Json::array ({ suggestCall (
            "Add a pad in two halves",
            Json::array (
                { suggestElement ("Make the track", Json::array ({ onItsOwn.at (0) })),
                  suggestElement ("Put a clip on it", Json::array ({ onItsOwn.at (1) })) })) })
    };

    REQUIRE (split.finished());
    REQUIRE (split.result (0).empty());

    const auto refusal = split.error (0).at ("message").get<std::string>();

    REQUIRE_THAT (refusal, Catch::Matchers::ContainsSubstring ("#pad"));
    REQUIRE_THAT (refusal, Catch::Matchers::ContainsSubstring ("another element"));
    REQUIRE_THAT (refusal, Catch::Matchers::ContainsSubstring ("elements[1].operations[0]"));
    REQUIRE (split.suggestions().empty());
}

TEST_CASE ("a Task Run makes at most one Suggestion", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildCorpus (session, project);

    const auto call = [&] (const std::string& summary, double volumeDb)
    {
        return suggestCall (summary,
                            Json::array ({ suggestElement (
                                summary,
                                Json::array ({ operation ("mixer.set",
                                                          { { "trackId", trackId (built.bass) },
                                                            { "volumeDb", volumeDb } }) })) }));
    };

    const ToolRun run {
        session, Json::array ({ call ("Pull the bass down", -3.0), call ("No, push it up", 3.0) })
    };

    REQUIRE (run.finished());
    REQUIRE (run.result (0).contains ("suggestionId"));

    REQUIRE (run.result (1).empty());
    REQUIRE (run.error (1).at ("code") == duet::collab::rpcError::suggestionAlreadyMade);

    // The first one stands, unaltered by the refusal of the second.
    REQUIRE (run.suggestions().size() == 1);
    REQUIRE (run.suggestions().front().summary == "Pull the bass down");
    REQUIRE (run.suggestion (0).elements.front().operations.front().at ("volumeDb") == -3.0);
}

TEST_CASE ("a run may say something, suggest something, or both", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildCorpus (session, project);

    const auto quieter = suggestCall (
        "Pull the bass down",
        Json::array ({ suggestElement (
            "Trim the bass fader",
            Json::array ({ operation (
                "mixer.set", { { "trackId", trackId (built.bass) }, { "volumeDb", -3.0 } }) })) }));

    const ToolRun said { session,
                         Json::array ({ duet::testing::toolCommentary ("The low end is crowded"),
                                        duet::testing::toolCommentary (" below 120 Hz.") }) };

    REQUIRE (said.finished());
    REQUIRE (said.commentary() == "The low end is crowded below 120 Hz.");
    REQUIRE (said.suggestions().empty());

    const ToolRun both {
        session,
        Json::array ({ duet::testing::toolCommentary ("The bass is fighting the kick."), quieter })
    };

    REQUIRE (both.finished());
    REQUIRE (both.commentary() == "The bass is fighting the kick.");
    REQUIRE (both.suggestions().size() == 1);
    REQUIRE (both.suggestions().front().summary == "Pull the bass down");
}

TEST_CASE ("a built-in parameter is written in real units and an external one normalized",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildCorpus (session, project);

    const auto ratio = [&] (double value)
    {
        return suggestCall (
            "Set the compressor",
            Json::array ({ suggestElement (
                "Set the compressor's ratio",
                Json::array ({ operation ("plugin.setParam",
                                          { { "pluginId", pluginId (built.compressor) },
                                            { "paramId", "ratio" },
                                            { "value", value } }) })) }));
    };

    // Duet owns what its own devices mean, so four to one is written as 4, and
    // the 0.05 the engine keeps underneath is not a ratio anyone can ask for.
    const ToolRun run { session, Json::array ({ ratio (0.05), ratio (4.0) }) };

    REQUIRE (run.finished());
    REQUIRE (run.result (0).empty());
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("value"));
    REQUIRE (run.result (1).contains ("suggestionId"));
}

TEST_CASE ("a parameter of a built-in the element is adding is held to what that plugin has",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildCorpus (session, project);

    const auto addAndSet = [&] (const std::string& paramId, double value)
    {
        return suggestCall (
            "Compress the bus",
            Json::array ({ suggestElement (
                "Put a compressor on the bus and set it",
                Json::array ({ operation ("plugin.add",
                                          { { "trackId", trackId (built.bus) },
                                            { "position", 0 },
                                            { "plugin", Json { { "builtin", "compressor" } } },
                                            { "ref", "#compressor" } }),
                               operation ("plugin.setParam",
                                          { { "pluginId", "#compressor" },
                                            { "paramId", paramId },
                                            { "value", value } }) })) }));
    };

    // Duet ships the compressor, so what it has and what its ratio may be are
    // both known before the element's own first operation makes one.
    const ToolRun run { session,
                        Json::array ({ addAndSet ("mix", 0.5),
                                       addAndSet ("ratio", 0.05),
                                       addAndSet ("ratio", 4.0) }) };

    REQUIRE (run.finished());

    REQUIRE (run.result (0).empty());
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("mix"));
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("elements[0].operations[1]"));

    REQUIRE (run.result (1).empty());
    REQUIRE_THAT (run.error (1).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("value"));
    REQUIRE_THAT (run.error (1).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("elements[0].operations[1]"));

    REQUIRE (run.result (2).contains ("suggestionId"));
}

TEST_CASE ("a scanned plugin's parameter is written normalized and a real-unit value is refused",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto fixture = scannedFixture (session, project);

    if (! fixture.has_value())
        SKIP ("this build cannot scan VST3s");

    duet::model::PluginRef hosted = duet::model::noPlugin;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               const auto track = ops.createTrack (TrackKind::audio, "Tone");
                               hosted = ops.addPlugin (track, *fixture, 0);
                           });

    // A parameter the fixture itself declares, found by whose meaning it
    // carries: the engine puts two of its own ahead of the vendor's on every
    // plugin it hosts, and those two are Duet's to state and not the vendor's.
    const auto held = session.pluginParameters (hosted);
    const auto theirs =
        std::ranges::find (held, false, &duet::model::PluginParameterInfo::duetOwnsMeaning);

    REQUIRE (theirs != held.end());

    const auto parameterId = theirs->parameterId;

    const auto set = [&] (double value)
    {
        return suggestCall ("Set the fixture",
                            Json::array ({ suggestElement (
                                "Set the fixture's own parameter",
                                Json::array ({ operation ("plugin.setParam",
                                                          { { "pluginId", pluginId (hosted) },
                                                            { "paramId", parameterId },
                                                            { "value", value } }) })) }));
    };

    // Duet does not own this plugin's mapping, so its parameter is the 0..1 the
    // plugin itself speaks, and a number in the other domain is refused rather
    // than quietly turned into one.
    const ToolRun run { session, Json::array ({ set (4.0), set (0.5) }) };

    REQUIRE (run.finished());
    REQUIRE (run.result (0).empty());
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("value"));
    REQUIRE (run.result (1).contains ("suggestionId"));
}

TEST_CASE ("the engine's own parameter on a hosted plugin is written in the unit it was read in",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto fixture = scannedFixture (session, project);

    if (! fixture.has_value())
        SKIP ("this build cannot scan VST3s");

    duet::model::PluginRef hosted = duet::model::noPlugin;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               const auto track = ops.createTrack (TrackKind::audio, "Tone");
                               hosted = ops.addPlugin (track, *fixture, 0);
                           });

    const auto set = [&] (double value)
    {
        return suggestCall (
            "Pull the fixture back",
            Json::array ({ suggestElement (
                "Bring the wet level down",
                Json::array ({ operation ("plugin.setParam",
                                          { { "pluginId", pluginId (hosted) },
                                            { "paramId", duet::model::hostedWetLevelParameterId },
                                            { "value", value } }) })) }));
    };

    // The wet level is the engine's own parameter and not the plugin's, so it
    // is read in decibels and written in decibels: one scale per parameter,
    // wherever it is read. The plugin's own normalised domain is not this
    // parameter's, and a number from it above unity is refused.
    const ToolRun run { session, Json::array ({ set (0.5), set (-6.0) }) };

    REQUIRE (run.finished());
    REQUIRE (run.result (0).empty());
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("value"));
    REQUIRE (run.result (1).contains ("suggestionId"));

    // What was written reads back as what was asked for.
    REQUIRE (session.auditionSuggestion (run.suggestion (1).changes));

    const auto held = session.pluginParameters (hosted);
    const auto wet = std::ranges::find (held,
                                        std::string { duet::model::hostedWetLevelParameterId },
                                        &duet::model::PluginParameterInfo::parameterId);

    REQUIRE (wet != held.end());
    REQUIRE_THAT (wet->value, Catch::Matchers::WithinAbs (-6.0, 0.01));

    session.stopAudition();
}

TEST_CASE ("a parameter of a scanned plugin the element is adding is held to 0 and 1", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto fixture = scannedFixture (session, project);

    if (! fixture.has_value())
        SKIP ("this build cannot scan VST3s");

    TrackRef tone = duet::model::noTrack;

    session.performAction ("Make room for it",
                           [&] (auto& ops) { tone = ops.createTrack (TrackKind::audio, "Tone"); });

    const auto addAndSet = [&] (double value)
    {
        return suggestCall (
            "Put the fixture on the tone",
            Json::array ({ suggestElement (
                "Add the fixture and set its first parameter",
                Json::array ({ operation ("plugin.add",
                                          { { "trackId", trackId (tone) },
                                            { "position", 0 },
                                            { "plugin", Json { { "external", *fixture } } },
                                            { "ref", "#fixture" } }),
                               operation ("plugin.setParam",
                                          { { "pluginId", "#fixture" },
                                            { "paramId", "gain" },
                                            { "value", value } }) })) }));
    };

    // Which parameters a plugin Duet did not ship has is the vendor's answer
    // and there is nothing to ask until the plugin exists. What its numbers
    // are is not: they are the normalised 0..1 every VST3 speaks, so a value
    // written in some other domain is refused here as it is anywhere else.
    const ToolRun run { session, Json::array ({ addAndSet (4.0), addAndSet (0.5) }) };

    REQUIRE (run.finished());

    REQUIRE (run.result (0).empty());
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("value"));
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("elements[0].operations[1]"));

    REQUIRE (run.result (1).contains ("suggestionId"));
}

TEST_CASE ("the engine's own parameter on a plugin an element is adding is held to its decibels",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto fixture = scannedFixture (session, project);

    if (! fixture.has_value())
        SKIP ("this build cannot scan VST3s");

    TrackRef tone = duet::model::noTrack;

    session.performAction ("Make room for it",
                           [&] (auto& ops) { tone = ops.createTrack (TrackKind::audio, "Tone"); });

    const auto addAndSet = [&] (double value)
    {
        return suggestCall (
            "Put the fixture on the tone, half wet",
            Json::array ({ suggestElement (
                "Add the fixture and pull its wet level back",
                Json::array ({ operation ("plugin.add",
                                          { { "trackId", trackId (tone) },
                                            { "position", 0 },
                                            { "plugin", Json { { "external", *fixture } } },
                                            { "ref", "#fixture" } }),
                               operation ("plugin.setParam",
                                          { { "pluginId", "#fixture" },
                                            { "paramId", duet::model::hostedWetLevelParameterId },
                                            { "value", value } }) })) }));
    };

    // Which parameters a plugin Duet did not ship has is the vendor's answer,
    // and there is nothing to ask until the plugin exists — but the two the
    // engine adds to every plugin it hosts are Duet's own and known before any
    // one of them. They are decibels here as they are everywhere else, so a
    // number from the vendor's normalised domain is refused.
    const ToolRun run { session, Json::array ({ addAndSet (0.5), addAndSet (-6.0) }) };

    REQUIRE (run.finished());

    REQUIRE (run.result (0).empty());
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("value"));
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("elements[0].operations[1]"));

    REQUIRE (run.result (1).contains ("suggestionId"));
}

TEST_CASE ("a Suggestion applies whole, and one element of it applies alone", "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto built = buildCorpus (session, project);
    const auto digestBefore = session.stateDigest();

    const auto elements = Json::array (
        { suggestElement ("Call it what it is",
                          Json::array ({ operation (
                              "track.rename",
                              { { "trackId", trackId (built.bass) }, { "name", "Sub Bass" } }) })),
          suggestElement ("Make room for the kick",
                          Json::array ({ operation (
                              "mixer.set",
                              { { "trackId", trackId (built.bass) }, { "volumeDb", -3.0 } }) })) });

    const ToolRun run { session, Json::array ({ suggestCall ("Tame the bass", elements) }) };

    REQUIRE (run.finished());

    const auto made = run.suggestion (0);

    REQUIRE_FALSE (made.id.empty());

    // Whole: everything the Suggestion says, under the name the Collaborator
    // gave the whole of it.
    REQUIRE (session.auditionSuggestion (made.changes));
    REQUIRE (session.track (built.bass).name == "Sub Bass");
    REQUIRE (session.track (built.bass).volumeDb < -2.0);

    session.stopAudition();
    REQUIRE (session.stateDigest() == digestBefore);

    // One element: exactly that element, and nothing the other one said.
    REQUIRE (session.auditionSuggestion (made.elements.back().changes));
    REQUIRE (session.track (built.bass).name == "Bass");
    REQUIRE (session.track (built.bass).volumeDb < -2.0);

    session.stopAudition();
    REQUIRE (session.stateDigest() == digestBefore);
}

TEST_CASE ("a Suggestion naming a parameter of a plugin that will not answer is refused, not fatal",
           "[collab]")
{
    const TempProject project;
    Session session { project.editFile() };

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_RAISING_VST3_FIXTURE, project.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        session, bundle.parent_path(), duet::testing::raisingVst3FixtureName);

    duet::model::PluginRef hosted = duet::model::noPlugin;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               const auto track = ops.createTrack (TrackKind::audio, "Tone");
                               hosted = ops.addPlugin (track, fixture.identifier, 0);
                           });

    // The parameter the plugin does declare, read while it is still behaving.
    const auto held = session.pluginParameters (hosted);
    const auto theirs =
        std::ranges::find (held, false, &duet::model::PluginParameterInfo::duetOwnsMeaning);

    REQUIRE (theirs != held.end());

    const auto parameterId = theirs->parameterId;

    duet::testing::raiseWhenRead (bundle);

    const auto set = [&] (const std::string& id)
    {
        return suggestCall ("Set the fixture",
                            Json::array ({ suggestElement (
                                "Set the fixture's own parameter",
                                Json::array ({ operation ("plugin.setParam",
                                                          { { "pluginId", pluginId (hosted) },
                                                            { "paramId", id },
                                                            { "value", 0.5 } }) })) }));
    };

    const ToolRun run {
        session, Json::array ({ set (parameterId), duet::testing::toolCall ("get_arrangement") })
    };

    // The run reached its ending, which is the whole of it: validating the
    // Suggestion asked the plugin what it has, the plugin refused, and the
    // refusal came back as an answer rather than as a raise in the DAW's own
    // message loop.
    REQUIRE (run.finished());
    REQUIRE (run.result (0).empty());

    // And it is refused for what is actually wrong. A plugin that would not say
    // what it has is not a plugin without the parameter: the first is a fact
    // about the plugin the model can do nothing about, and telling the model
    // the second would send it hunting for a name that was right all along.
    REQUIRE_THAT (run.error (0).at ("message").get<std::string>(),
                  Catch::Matchers::ContainsSubstring ("would not say what parameters it has"));

    REQUIRE (run.result (1).contains ("tempoBpm"));
}
