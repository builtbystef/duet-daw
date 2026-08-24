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
std::filesystem::path copyFixture (const std::filesystem::path& fixture,
                                   const std::filesystem::path& directory)
{
    const auto copy = directory / fixture.filename();
    std::filesystem::copy (fixture,
                           copy,
                           std::filesystem::copy_options::recursive
                               | std::filesystem::copy_options::overwrite_existing);
    return copy;
}

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
    const auto fixture = copyFixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);

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

    const auto good = copyFixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);
    const auto crashing = copyFixture (DUET_CRASHING_VST3_FIXTURE, pluginDirectory);
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

TEST_CASE (
    "a scanned VST3 inserts through the vocabulary, processes audio, and undoes a parameter exactly")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);
    copyFixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);

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
}

TEST_CASE ("a project restores a VST3's state, and still opens when the VST3 is missing")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    const auto projectFolder = temp.folder() / "project";
    std::filesystem::create_directory (pluginDirectory);
    const auto fixtureFile = copyFixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);

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
