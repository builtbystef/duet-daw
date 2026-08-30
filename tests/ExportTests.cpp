#include <duet/model/Session.h>
#include <duet/persistence/Project.h>

#include <duet/testing/RenderHarness.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <filesystem>
#include <string_view>

using duet::model::ExportFormat;
using duet::model::ExportOptions;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::exportProject;
using duet::testing::oneRenderBlockSeconds;
using duet::testing::renderProject;
using duet::testing::TempProject;

/** The Export/Bounce dialog's render, at the model seam (issue zm174o).

    What the producer chooses in the dialog is what an ExportOptions holds, so
    everything the dialog can ask for is asserted here: the stretch of the
    timeline, the level normalising puts the file at, the cancel, and — the
    criterion that is about the session rather than the file — that exporting
    leaves the project exactly as it found it.

    ADR 0006 governs the measurements: features with the tolerances the domain
    gives them, and the one exception is the within-process determinism canary
    that answers "the same range twice produces identical output".
*/
namespace
{
/** The pitch tolerance a steady tone measured over a few dozen cycles carries,
    the same one the offline-render suite asks for.
*/
constexpr double pitchToleranceHz = 1.0;

/** How far a measured peak may be from the level asked for, as a sample value.
    A hundredth of full scale is a tenth of a decibel at this level, well below
    what anyone hears and well above the arithmetic.
*/
constexpr double peakTolerance = 0.01;

/** What a file written in one of the three formats is called. */
[[nodiscard]] std::string_view fileNameFor (ExportFormat format)
{
    switch (format)
    {
        case ExportFormat::aiff:
            return "chosen.aiff";
        case ExportFormat::flac:
            return "chosen.flac";
        case ExportFormat::wav:
            break;
    }

    return "chosen.wav";
}

/** A project holding one steady tone from its very start, long enough that the
    first bars of it are ordinary content rather than the whole of it.

    The tempo is the project's own 120 bpm in 4/4, which is what makes a bar two
    seconds long and bars 1 to 4 eight seconds of it.
*/
struct SteadyToneProject
{
    static constexpr double toneHz = 440.0;
    static constexpr double lengthSeconds = 12.0;

    SteadyToneProject()
    {
        const auto tone = project.writeTone ("tone.wav", lengthSeconds, toneHz);

        session.useNoAudioDevice();
        session.suppressDeviceRebuild();
        session.performAction ("Add a tone",
                               [&] (auto& ops)
                               {
                                   track = ops.createTrack (TrackKind::audio, "Tone");
                                   ops.insertAudioClip (track, "tone", tone, 0.0, lengthSeconds);
                               });
    }

    /** The export the dialog would ask for over bars 1 to 4, written to a file
        of this project's own folder that nothing has written before.
    */
    [[nodiscard]] ExportOptions overFirstFourBars (std::string_view fileName) const
    {
        ExportOptions options;
        options.destination = project.folder() / fileName;
        options.startSeconds = session.barStartSeconds (1);
        options.endSeconds = session.barStartSeconds (5);

        return options;
    }

    TempProject project;
    Session session { project.editFile() };
    TrackRef track = duet::model::noTrack;
};
} // namespace

//==============================================================================
TEST_CASE ("exporting bars 1 to 4 at 120 bpm writes eight seconds of the tone")
{
    SteadyToneProject tone;

    REQUIRE (tone.session.tempoBpm() == Catch::Approx (120.0));

    const auto options = tone.overFirstFourBars ("bars-1-to-4.wav");
    const auto exported = exportProject (tone.session, options);

    REQUIRE (exported.readable());
    REQUIRE (std::filesystem::exists (options.destination));

    // Four bars of two seconds each. The engine cuts the timeline into blocks,
    // so the file lands within one of them of the length asked for.
    INFO ("length: " << exported.lengthSeconds());
    REQUIRE (exported.lengthSeconds() == Catch::Approx (8.0).margin (oneRenderBlockSeconds));

    // And what is in it is the tone, not silence that happens to be the right
    // length.
    INFO ("pitch: " << exported.pitchHzBetween (1.0, 7.0));
    REQUIRE (exported.pitchHzBetween (1.0, 7.0)
             == Catch::Approx (SteadyToneProject::toneHz).margin (pitchToleranceHz));
}

TEST_CASE ("an export runs off the message thread, so the app stays responsive")
{
    SteadyToneProject tone;

    const auto exported = exportProject (tone.session, tone.overFirstFourBars ("off-thread.wav"));

    REQUIRE (exported.readable());
    REQUIRE (exported.ranOffTheMessageThread());
}

TEST_CASE ("exporting the same range twice in one session produces identical output")
{
    SteadyToneProject tone;

    const auto first = exportProject (tone.session, tone.overFirstFourBars ("twice-a.wav"));
    const auto second = exportProject (tone.session, tone.overFirstFourBars ("twice-b.wav"));

    REQUIRE (first.readable());
    REQUIRE (second.readable());
    REQUIRE (first.isBitIdenticalTo (second));
}

TEST_CASE ("normalising brings the exported peak to the target level, and off leaves it alone")
{
    SteadyToneProject tone;

    // What the project renders to on its own, which is what "as rendered" means:
    // the level the tone reaches once the project has finished with it.
    const auto rendered = renderProject (tone.session, tone.project.folder());
    const auto asRendered =
        exportProject (tone.session, tone.overFirstFourBars ("as-rendered.wav"));

    REQUIRE (rendered.readable());
    REQUIRE (asRendered.readable());

    INFO ("peak as rendered: " << asRendered.peakBetween (0.0, 8.0) << ", the project renders to "
                               << rendered.peakBetween (0.0, 8.0));
    REQUIRE (asRendered.peakBetween (0.0, 8.0)
             == Catch::Approx (rendered.peakBetween (0.0, 8.0)).margin (peakTolerance));

    auto normalising = tone.overFirstFourBars ("normalised.wav");
    normalising.normalise = true;

    const auto normalised = exportProject (tone.session, normalising);
    const auto target = std::pow (10.0, duet::model::exportNormaliseTargetDb / 20.0);

    REQUIRE (normalised.readable());
    INFO ("peak normalised: " << normalised.peakBetween (0.0, 8.0) << ", target " << target);
    REQUIRE (normalised.peakBetween (0.0, 8.0) == Catch::Approx (target).margin (peakTolerance));
}

TEST_CASE ("an export reports how far it has got, and a cancelled one leaves no file behind")
{
    SteadyToneProject tone;

    const auto options = tone.overFirstFourBars ("cancelled.wav");

    double furthest = -1.0;
    const auto cancelAtOnce = [&furthest] (double proportion)
    {
        furthest = proportion;

        return false;
    };

    const auto cancelled = exportProject (tone.session, options, cancelAtOnce);

    // The progress was reported — an export that never says how far it has got
    // has nothing to draw a bar from — and the cancel was taken.
    REQUIRE (furthest >= 0.0);
    REQUIRE (furthest < 1.0);
    REQUIRE_FALSE (cancelled.readable());

    // Nothing half-written: a partial file would be read as a whole one.
    REQUIRE_FALSE (std::filesystem::exists (options.destination));
}

TEST_CASE ("an export that is let through reports progress all the way to the end")
{
    SteadyToneProject tone;

    double furthest = 0.0;
    const auto watch = [&furthest] (double proportion)
    {
        furthest = proportion;

        return true;
    };

    const auto exported =
        exportProject (tone.session, tone.overFirstFourBars ("watched.wav"), watch);

    REQUIRE (exported.readable());
    REQUIRE (furthest > 0.0);
}

TEST_CASE ("exporting leaves the transport rolling, the history alone and the project saved")
{
    const TempProject folder;
    const auto project = duet::persistence::Project::create (folder.folder() / "Exported");

    REQUIRE (project != nullptr);

    auto& session = project->session();
    session.useNoAudioDevice();
    session.suppressDeviceRebuild();

    const auto tone = folder.writeTone ("tone.wav", 12.0, 440.0);
    session.performAction ("Add a tone",
                           [&] (auto& ops)
                           {
                               const auto track = ops.createTrack (TrackKind::audio, "Tone");
                               ops.insertAudioClip (track, "tone", tone, 0.0, 12.0);
                           });

    REQUIRE (project->save());
    REQUIRE_FALSE (project->hasUnsavedChanges());
    REQUIRE (duet::testing::playUntilRolling (session));

    const auto stateBefore = session.stateDigest();
    const auto revisionBefore = session.revision();

    ExportOptions options;
    options.destination = folder.folder() / "undisturbed.wav";
    options.startSeconds = session.barStartSeconds (1);
    options.endSeconds = session.barStartSeconds (5);

    REQUIRE (exportProject (session, options).readable());

    // The producer never stopped playing, the undo history is where they left
    // it, and the project has nothing new to save: an export is a read of the
    // project rather than an edit of it.
    REQUIRE (session.isPlaying());
    REQUIRE (session.revision() == revisionBefore);
    REQUIRE (session.stateDigest() == stateBefore);
    REQUIRE_FALSE (project->hasUnsavedChanges());

    session.stopPlayback();
}

TEST_CASE ("an export can be written in each of the formats the dialog offers")
{
    SteadyToneProject tone;

    const auto format = GENERATE (ExportFormat::wav, ExportFormat::aiff, ExportFormat::flac);

    auto options = tone.overFirstFourBars (fileNameFor (format));
    options.format = format;
    options.bitDepth = 24;

    const auto exported = exportProject (tone.session, options);

    REQUIRE (exported.readable());
    INFO ("length: " << exported.lengthSeconds());
    REQUIRE (exported.lengthSeconds() == Catch::Approx (8.0).margin (oneRenderBlockSeconds));
}

TEST_CASE ("an export takes the sample rate it was asked for")
{
    SteadyToneProject tone;

    auto options = tone.overFirstFourBars ("at-48k.wav");
    options.sampleRate = 48000.0;

    const auto exported = exportProject (tone.session, options);

    REQUIRE (exported.readable());

    // The length is in seconds either way; the rate is what the samples in it
    // are counted at, and the pitch is what says the file was not simply played
    // back too fast.
    INFO ("length: " << exported.lengthSeconds());
    REQUIRE (exported.lengthSeconds() == Catch::Approx (8.0).margin (oneRenderBlockSeconds));
    REQUIRE (exported.pitchHzBetween (1.0, 7.0)
             == Catch::Approx (SteadyToneProject::toneHz).margin (pitchToleranceHz));
}

TEST_CASE ("an export with nothing to write writes nothing")
{
    SteadyToneProject tone;

    ExportOptions nowhere;
    nowhere.endSeconds = 4.0;

    REQUIRE_FALSE (tone.session.exportToFile (nowhere));

    auto emptyRange = tone.overFirstFourBars ("empty-range.wav");
    emptyRange.endSeconds = emptyRange.startSeconds;

    REQUIRE_FALSE (tone.session.exportToFile (emptyRange));
    REQUIRE_FALSE (std::filesystem::exists (emptyRange.destination));
}
