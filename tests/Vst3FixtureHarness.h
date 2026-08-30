#pragma once

#include <duet/model/Session.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

/** The VST3 fixtures, from a suite's point of view.

    Duet builds two plugins of its own to be hosted by its own tests, and every
    suite that hosts one does the same three things with it: copy the bundle
    where a scan will find it, scan it, and — for the one that turns hostile —
    say when it should start refusing. They live here because the raise a
    hostile plugin makes is not one module's problem: it reaches the tool
    vocabulary, the Suggestion layer and the interface alike, and a suite
    asserting its own end of that should not have to reinvent the fixture.
*/
namespace duet::testing
{
/** The names the two fixtures answer to, which is how a scan finds them. */
constexpr const char* goodVst3FixtureName = "Duet Good VST3 Fixture";
constexpr const char* raisingVst3FixtureName = "Duet Raising VST3 Fixture";

/** Copies a VST3 bundle into a directory of the project's own, so that a test
    which takes the bundle away, or writes into it, is touching its own copy.
*/
inline std::filesystem::path copyVst3Fixture (const std::filesystem::path& fixture,
                                              const std::filesystem::path& directory)
{
    std::filesystem::create_directories (directory);

    const auto copy = directory / fixture.filename();
    std::filesystem::copy (fixture,
                           copy,
                           std::filesystem::copy_options::recursive
                               | std::filesystem::copy_options::overwrite_existing);

    return copy;
}

/** Scans a directory and answers the description of the fixture in it.

    A machine that cannot host VST3s at all — a CI runner with no plugin host —
    skips rather than fails: what is being asserted is what Duet says about a
    hosted plugin, and there is nothing to say where none can be hosted.
*/
inline duet::model::KnownPluginInfo scanVst3Fixture (duet::model::Session& session,
                                                     const std::filesystem::path& directory,
                                                     const std::string& name)
{
    if (! session.canHostVst3() || ! session.scanVst3Plugins (directory).completed)
        SKIP ("this build cannot scan VST3s");

    const auto known = session.knownVst3Plugins();
    const auto found = std::ranges::find (known, name, &duet::model::KnownPluginInfo::name);

    if (found == known.end())
        SKIP ("the VST3 fixture did not scan");

    return *found;
}

/** Turns the raising fixture hostile, from the moment this returns.

    It is well behaved until a test says otherwise, because a plugin that raised
    while it was being scanned would never reach a chain at all, and what these
    tests are about is a plugin the producer already has. The marker file beside
    the bundle is how the fixture is told (`tests/vst3_fixtures/RaisingPlugin.cpp`).
*/
inline void raiseWhenRead (const std::filesystem::path& bundle)
{
    std::ofstream { bundle / "raise-on-read.txt" } << "raise\n";
}
} // namespace duet::testing
