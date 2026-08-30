#include "Vst3FixtureHarness.h"

#include <duet/model/Session.h>
#include <duet/persistence/Project.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <fstream>

using Catch::Matchers::WithinAbs;
using duet::model::PluginRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::persistence::Project;
using duet::testing::TempProject;

namespace
{
double parameterValue (const Session& session, PluginRef plugin, const std::string& parameterId)
{
    const auto parameters = session.pluginParameters (plugin);
    const auto found =
        std::ranges::find (parameters, parameterId, &duet::model::PluginParameterInfo::parameterId);
    return found != parameters.end() ? found->value : -1.0;
}

int lineCount (const std::filesystem::path& file)
{
    std::ifstream input { file };
    return static_cast<int> (std::count (
        std::istreambuf_iterator<char> { input }, std::istreambuf_iterator<char> {}, '\n'));
}

} // namespace

TEST_CASE ("VST3 hosting and out-of-process scanning are enabled")
{
    const TempProject temp;
    const Session session { temp.editFile() };

    REQUIRE (session.canHostVst3());
    REQUIRE (session.scansPluginsOutOfProcess());
}

TEST_CASE ("scanning an empty VST3 directory completes, and a missing directory fails")
{
    const TempProject temp;
    Session session { temp.editFile() };
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);

    const auto empty = session.scanVst3Plugins (pluginDirectory);

    REQUIRE (empty.completed);
    REQUIRE (empty.failedFiles.empty());
    REQUIRE (empty.badFiles.empty());

    const auto missing = session.scanVst3Plugins (temp.folder() / "missing");

    REQUIRE_FALSE (missing.completed);
}

TEST_CASE ("a scanned VST3 joins the known list and survives a restart without a rescan")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);
    const auto fixture = duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);

    std::string identifier;

    {
        Session session { temp.editFile() };
        const auto scanned = session.scanVst3Plugins (pluginDirectory);
        REQUIRE (scanned.completed);

        const auto known = session.knownVst3Plugins();
        const auto found = std::ranges::find (known, fixture, &duet::model::KnownPluginInfo::file);

        REQUIRE (found != known.end());
        REQUIRE (found->name == "Duet Good VST3 Fixture");
        REQUIRE_FALSE (found->identifier.empty());
        identifier = found->identifier;
    }

    // A new engine is the next app launch. The bundle need not be touched to
    // restore the list: scanning put the description in app-global settings.
    const Session restarted { temp.editFile() };
    const auto knownAfterRestart = restarted.knownVst3Plugins();

    REQUIRE (std::ranges::any_of (
        knownAfterRestart, [&] (const auto& plugin) { return plugin.identifier == identifier; }));
}

TEST_CASE ("a crashing VST3 kills only the scanner, and is skipped after it is marked bad")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);

    const auto good = duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);
    const auto crashing =
        duet::testing::copyVst3Fixture (DUET_CRASHING_VST3_FIXTURE, pluginDirectory);
    const auto crashMarker = crashing / "crash-loads.txt";

    Session session { temp.editFile() };
    const auto firstScan = session.scanVst3Plugins (pluginDirectory);

    REQUIRE (firstScan.completed);
    REQUIRE (std::ranges::any_of (session.knownVst3Plugins(),
                                  [&] (const auto& plugin) { return plugin.file == good; }));
    REQUIRE (std::ranges::find (firstScan.badFiles, crashing) != firstScan.badFiles.end());

    const auto attemptsBeforeSkip = lineCount (crashMarker);
    REQUIRE (attemptsBeforeSkip > 0);

    const auto secondScan = session.scanVst3Plugins (pluginDirectory);

    REQUIRE (secondScan.completed);
    REQUIRE (lineCount (crashMarker) == attemptsBeforeSkip);
}

TEST_CASE ("the engine's two parameters on a hosted VST3 are read and written in decibels")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);
    duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);

    Session session { temp.editFile() };

    if (! session.canHostVst3() || ! session.scanVst3Plugins (pluginDirectory).completed)
        SKIP ("this build cannot scan VST3s");

    const auto known = session.knownVst3Plugins();
    const auto fixture = std::ranges::find (
        known, std::string { "Duet Good VST3 Fixture" }, &duet::model::KnownPluginInfo::name);

    if (fixture == known.end())
        SKIP ("the VST3 fixture did not scan");

    PluginRef plugin = duet::model::noPlugin;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               const auto track = ops.createTrack (TrackKind::audio, "Tone");
                               plugin = ops.addPlugin (track, fixture->identifier, 0);
                           });

    const auto parameters = session.pluginParameters (plugin);

    // The ids the engine names its own two by. Duet states them for a plugin
    // that does not exist yet, so that a Suggestion adding one can be held to
    // the right scale, and this is what keeps that statement true.
    for (const auto* parameterId :
         { duet::model::hostedDryLevelParameterId, duet::model::hostedWetLevelParameterId })
    {
        const auto found = std::ranges::find (parameters,
                                              std::string { parameterId },
                                              &duet::model::PluginParameterInfo::parameterId);

        REQUIRE (found != parameters.end());
        REQUIRE (found->duetOwnsMeaning);
        REQUIRE (found->unit == "dB");
        REQUIRE_THAT (found->minValue, WithinAbs (duet::model::hostedLevelMinimumDb, 0.001));
        REQUIRE_THAT (found->maxValue, WithinAbs (duet::model::hostedLevelMaximumDb, 0.001));
    }

    // The plugin's own parameters keep the vendor's normalised domain: two
    // regimes in one plugin's list, and which one a parameter is in is what the
    // facade states.
    const auto theirs =
        std::ranges::find (parameters, false, &duet::model::PluginParameterInfo::duetOwnsMeaning);

    REQUIRE (theirs != parameters.end());
    REQUIRE (theirs->unit.empty());

    // A level goes in in decibels and comes back the same, which is the
    // identity every parameter Duet states the meaning of holds to.
    session.performAction (
        "Half the wet level",
        [&] (auto& ops)
        { ops.setPluginParameter (plugin, duet::model::hostedWetLevelParameterId, -6.0); });

    REQUIRE_THAT (parameterValue (session, plugin, duet::model::hostedWetLevelParameterId),
                  WithinAbs (-6.0, 0.01));
}

TEST_CASE (
    "a scanned VST3 inserts through the vocabulary, processes audio, and undoes a parameter exactly")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);
    duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);

    Session session { temp.editFile() };
    REQUIRE (session.scanVst3Plugins (pluginDirectory).completed);

    const auto known = session.knownVst3Plugins();
    const auto fixture = std::ranges::find (
        known, std::string { "Duet Good VST3 Fixture" }, &duet::model::KnownPluginInfo::name);
    REQUIRE (fixture != known.end());

    const auto tone = temp.writeTone ("tone.wav", 2.0, 440.0);
    TrackRef track = duet::model::noTrack;
    PluginRef plugin = duet::model::noPlugin;

    session.performAction ("Lay down a tone",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::audio, "Tone");
                               ops.insertAudioClip (track, "tone", tone, 0.0, 2.0);
                           });
    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           { plugin = ops.addPlugin (track, fixture->identifier, 0); });

    REQUIRE (plugin != duet::model::noPlugin);
    REQUIRE_FALSE (session.track (track).plugins.front().missing);
    REQUIRE (session.track (track).plugins.front().externalIdentifier == fixture->identifier);

    const auto parameters = session.pluginParameters (plugin);
    const auto gain = std::ranges::find (
        parameters, std::string { "Gain" }, &duet::model::PluginParameterInfo::name);
    REQUIRE (gain != parameters.end());
    REQUIRE_FALSE (gain->displayValue.empty());

    const auto parameterId = gain->parameterId;
    const auto oldValue = parameterValue (session, plugin, parameterId);
    const auto baselineFile = temp.folder() / "baseline.wav";
    REQUIRE (session.renderToFile (baselineFile));
    const auto baselinePeak = duet::testing::peakLevelOf (baselineFile);
    const auto beforeParameter = session.stateDigest();

    session.performAction ("Turn the fixture down",
                           [&] (auto& ops) { ops.setPluginParameter (plugin, parameterId, 0.25); });

    REQUIRE (session.undoNames().front() == "Turn the fixture down");
    REQUIRE_THAT (parameterValue (session, plugin, parameterId), WithinAbs (0.25, 0.000001));

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforeParameter);
    REQUIRE_THAT (parameterValue (session, plugin, parameterId), WithinAbs (oldValue, 0.000001));

    REQUIRE (session.redo());
    const auto rendered = temp.folder() / "processed.wav";
    REQUIRE (session.renderToFile (rendered));
    REQUIRE_THAT (duet::testing::peakLevelOf (rendered), WithinAbs (baselinePeak * 0.25, 0.001));

    const auto beforeBypass = session.stateDigest();
    session.performAction ("Bypass Plugin",
                           [&] (auto& ops) { ops.setPluginBypassed (plugin, true); });
    const auto bypassed = temp.folder() / "bypassed.wav";
    REQUIRE (session.renderToFile (bypassed));
    REQUIRE_THAT (duet::testing::peakLevelOf (bypassed), WithinAbs (baselinePeak, 0.001));
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforeBypass);

    const auto quietPreset = session.pluginOpaqueState (plugin);
    REQUIRE_FALSE (quietPreset.empty());
    session.performAction ("Turn the fixture up",
                           [&] (auto& ops) { ops.setPluginParameter (plugin, parameterId, 0.8); });
    const auto beforePresetLoad = session.stateDigest();
    session.performAction ("Load Plugin Preset",
                           [&] (auto& ops) { ops.setPluginOpaqueState (plugin, quietPreset); });
    REQUIRE (session.undoNames().front() == "Load Plugin Preset");
    REQUIRE_THAT (parameterValue (session, plugin, parameterId), WithinAbs (0.25, 0.000001));
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforePresetLoad);
    REQUIRE_THAT (parameterValue (session, plugin, parameterId), WithinAbs (0.8, 0.000001));
}

TEST_CASE ("a project restores a VST3's state, and still opens when the VST3 is missing")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    const auto projectFolder = temp.folder() / "project";
    std::filesystem::create_directory (pluginDirectory);
    const auto fixtureFile =
        duet::testing::copyVst3Fixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);

    PluginRef plugin = duet::model::noPlugin;
    std::string parameterId;

    {
        const auto project = Project::create (projectFolder);
        REQUIRE (project != nullptr);
        auto& session = project->session();
        REQUIRE (session.scanVst3Plugins (pluginDirectory).completed);

        const auto known = session.knownVst3Plugins();
        const auto fixture = std::ranges::find (
            known, std::string { "Duet Good VST3 Fixture" }, &duet::model::KnownPluginInfo::name);
        REQUIRE (fixture != known.end());

        session.performAction ("Insert the fixture",
                               [&] (auto& ops)
                               {
                                   const auto track =
                                       ops.createTrack (TrackKind::audio, "Processed");
                                   plugin = ops.addPlugin (track, fixture->identifier, 0);
                               });

        const auto parameters = session.pluginParameters (plugin);
        const auto gain = std::ranges::find (
            parameters, std::string { "Gain" }, &duet::model::PluginParameterInfo::name);
        REQUIRE (gain != parameters.end());
        parameterId = gain->parameterId;

        session.performAction ("Set the fixture state",
                               [&] (auto& ops)
                               { ops.setPluginParameter (plugin, parameterId, 0.4); });
        REQUIRE (project->save());
    }

    {
        const auto reopened = Project::open (projectFolder);
        REQUIRE (reopened != nullptr);
        REQUIRE_FALSE (reopened->session().tracks().back().plugins.front().missing);
        REQUIRE_THAT (parameterValue (reopened->session(), plugin, parameterId),
                      WithinAbs (0.4, 0.000001));
    }

    std::filesystem::remove_all (fixtureFile);

    const auto missing = Project::open (projectFolder);
    REQUIRE (missing != nullptr);
    REQUIRE (missing->session().tracks().back().plugins.front().missing);
}

TEST_CASE ("a hosted plugin that raises when read is a read that answers, not a raise")
{
    const TempProject temp;
    Session session { temp.editFile() };

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_RAISING_VST3_FIXTURE, temp.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        session, bundle.parent_path(), duet::testing::raisingVst3FixtureName);

    PluginRef plugin = duet::model::noPlugin;

    session.performAction ("Insert the fixture",
                           [&] (auto& ops)
                           {
                               const auto track = ops.createTrack (TrackKind::audio, "Tone");
                               plugin = ops.addPlugin (track, fixture.identifier, 0);
                           });

    // Behaving, and hosted: what is being asserted is a plugin the producer
    // already has, which turns hostile under them.
    REQUIRE_FALSE (session.pluginParameters (plugin).empty());
    REQUIRE (session.readPluginParameters (plugin).wereRead);

    duet::testing::raiseWhenRead (bundle);

    // The facade puts no engine type across it, and an exception thrown inside
    // a plugin is one: asking is asking the plugin, and the plugin refusing to
    // answer is an answer this side of the seam rather than a raise on whatever
    // thread asked. Everything that reads a plugin's parameters — the tool
    // vocabulary, the Suggestion layer, the automation lane list and a native
    // editor's gesture — reads them here, so this is the one place the raise
    // has to stop.
    REQUIRE_NOTHROW (session.pluginParameters (plugin));
    REQUIRE (session.pluginParameters (plugin).empty());

    // Empty, and a plugin with no parameters at all is empty too: what tells
    // the two apart is the read itself saying the plugin would not answer.
    const auto read = session.readPluginParameters (plugin);

    REQUIRE_FALSE (read.wereRead);
    REQUIRE (read.held.empty());
}
