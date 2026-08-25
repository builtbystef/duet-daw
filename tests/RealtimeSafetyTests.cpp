#include <duet/model/Session.h>

#include <RealtimeProbes.h>
#include <duet/testing/RenderHarness.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

using duet::model::PluginRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::addNativeRealtimeProbe;
using duet::testing::juceRealtimeProbeName;
using duet::testing::realtimeProbeGain;
using duet::testing::renderProject;
using duet::testing::TempProject;

/** The real-time backstop, entered from outside.

    The Real-time audio standard is enforced by review; RealtimeSanitizer is the
    dynamic backstop behind it, and a backstop only reaches what actually runs.
    These two cases are what makes it run: each drives one of the two callback
    entry points the standard names — the engine-native `applyToBuffer` and the
    JUCE-hosted `processBlock` — through an offline render, on a machine with no
    audio hardware, which is the shape the `linux-rtsan` nightly needs (ADR
    0006).

    In the ordinary configurations these are two plain tests, and what they
    assert is the only thing a test can see of a callback from outside: that the
    audio came back changed the way the callback changes it. In the rtsan
    configuration the same two runs are the backstop, and a violation inside
    either callback ends the run rather than the assertion.

    A probe that stops being entered is the failure this file exists to catch:
    the sanitizer says nothing at all about a callback that never ran, so a
    silent pass would look exactly like a green nightly.
*/
namespace
{
/** How far a measured level may sit from the level asked for, in decibels. */
constexpr double levelToleranceDb = 0.1;

/** What halving the audio measures as: −6.02 dB, and the tolerance above is far
    tighter than the difference between that and no change at all.
*/
const double probeLevelChangeDb = 20.0 * std::log10 (realtimeProbeGain);

/** The frequency every case here measures, and the one its clip is written at. */
constexpr double toneHz = 440.0;

/** A project holding one second of a 440 Hz tone, one second in — the same
    shape the render harness's own cases use, because what a probe needs is
    simply something to be heard changing.
*/
struct ToneProject
{
    ToneProject()
    {
        const auto tone = project.writeTone ("tone.wav", 1.0, toneHz);

        session.useNoAudioDevice();
        session.suppressDeviceRebuild();
        session.performAction ("Add a tone",
                               [&] (auto& ops)
                               {
                                   track = ops.createTrack (TrackKind::audio, "Tone");
                                   ops.insertAudioClip (track, "tone", tone, 1.0, 1.0);
                               });
    }

    /** The level of the tone in a render of this project, in decibels of full
        scale: what a probe in the chain moves and nothing else does.
    */
    [[nodiscard]] double renderedToneLevelDb()
    {
        const auto render = renderProject (session, project.folder());
        REQUIRE (render.readable());
        return render.toneLevelDbBetween (toneHz, 1.1, 1.9);
    }

    TempProject project;
    Session session { project.editFile() };
    TrackRef track = duet::model::noTrack;
};
} // namespace

//==============================================================================
TEST_CASE ("an offline render enters the engine-native probe's applyToBuffer")
{
    ToneProject tone;

    const auto plain = tone.renderedToneLevelDb();

    addNativeRealtimeProbe (tone.session, tone.track);
    duet::testing::pumpMessages (100);

    const auto probed = tone.renderedToneLevelDb();

    INFO ("tone without the probe: " << plain << " dB, with it: " << probed << " dB");
    REQUIRE (probed - plain == Catch::Approx (probeLevelChangeDb).margin (levelToleranceDb));
}

TEST_CASE ("an offline render enters the JUCE-hosted probe's processBlock")
{
    ToneProject tone;

    const auto pluginDirectory = tone.project.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);
    const std::filesystem::path bundle { DUET_REALTIME_PROBE_VST3 };
    std::filesystem::copy (bundle,
                           pluginDirectory / bundle.filename(),
                           std::filesystem::copy_options::recursive
                               | std::filesystem::copy_options::overwrite_existing);

    REQUIRE (tone.session.scanVst3Plugins (pluginDirectory).completed);

    const auto known = tone.session.knownVst3Plugins();
    const auto probe = std::ranges::find (
        known, std::string { juceRealtimeProbeName }, &duet::model::KnownPluginInfo::name);
    REQUIRE (probe != known.end());

    const auto plain = tone.renderedToneLevelDb();

    PluginRef plugin = duet::model::noPlugin;
    tone.session.performAction ("Insert the probe",
                                [&] (auto& ops)
                                { plugin = ops.addPlugin (tone.track, probe->identifier, 0); });

    REQUIRE (plugin != duet::model::noPlugin);
    REQUIRE_FALSE (tone.session.track (tone.track).plugins.front().missing);

    const auto probed = tone.renderedToneLevelDb();

    INFO ("tone without the probe: " << plain << " dB, with it: " << probed << " dB");
    REQUIRE (probed - plain == Catch::Approx (probeLevelChangeDb).margin (levelToleranceDb));
}
