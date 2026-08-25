#include <duet/model/Session.h>

#include <duet/testing/RenderHarness.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

using duet::model::AutomationPoint;
using duet::model::AutomationTarget;
using duet::model::BuiltinPlugin;
using duet::model::ClipRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::oneRenderBlockSeconds;
using duet::testing::renderProject;
using duet::testing::renderTrack;
using duet::testing::TempProject;

/** The audio-correctness harness, and what it is allowed to say.

    ADR 0006: a render is asserted on its measured features with the tolerances
    the domain gives them, never against a golden file or a stored fingerprint,
    and the one exception is the determinism canary at the bottom of this file.
    Every signal these cases measure is one they wrote themselves, so what the
    render should hold is true by construction rather than by an earlier run
    having agreed.

    None of it says what a producer hears. The engine builds one graph to render
    an Edit and another to play it, and that question belongs to the meters over
    the playback graph (PlaybackLevelTests).
*/
namespace
{
/** The pitch tolerance these cases ask for. The measurement counts rising zero
    crossings over a fifth of a second, which lands within a hertz of a steady
    tone — nowhere near tight enough to hear, and far tighter than a wrong note.
*/
constexpr double pitchToleranceHz = 1.0;

/** How far a measured level may be from the level asked for, in decibels: a
    tenth of a decibel is below what anyone can hear and well above the
    arithmetic.
*/
constexpr double levelToleranceDb = 0.1;

/** A tone that spans one second of the timeline from one second in, which is
    the shape the worked examples share: silence, then a known pitch.
*/
struct ToneProject
{
    explicit ToneProject (double frequencyHz = 440.0)
    {
        const auto tone = project.writeTone ("tone.wav", 1.0, frequencyHz);

        session.useNoAudioDevice();
        session.suppressDeviceRebuild();
        session.performAction ("Add a tone",
                               [&] (auto& ops)
                               {
                                   track = ops.createTrack (TrackKind::audio, "Tone");
                                   clip = ops.insertAudioClip (track, "tone", tone, 1.0, 1.0);
                               });
    }

    TempProject project;
    Session session { project.editFile() };
    TrackRef track = duet::model::noTrack;
    ClipRef clip = duet::model::noClip;
};
} // namespace

//==============================================================================
TEST_CASE ("a project renders to a readable audio file with no audio device at all")
{
    ToneProject tone;

    const auto render = renderProject (tone.session, tone.project.folder());

    REQUIRE (render.readable());
    REQUIRE (render.ranOffTheMessageThread());
    REQUIRE (render.channelCount() == 2);
    REQUIRE (render.lengthSeconds() == Catch::Approx (2.0).margin (oneRenderBlockSeconds));
}

TEST_CASE ("a rendered tone measures as silence, then as the pitch it was written at")
{
    ToneProject tone;

    const auto render = renderProject (tone.session, tone.project.folder());
    REQUIRE (render.readable());

    // Nothing before the clip. The tone may begin one render block early, so
    // the stretch asserted silent stops one block short of it.
    INFO ("level before the clip: " << render.rmsBetween (0.0, 1.0 - oneRenderBlockSeconds));
    REQUIRE (render.isSilentBetween (0.0, 1.0 - oneRenderBlockSeconds));

    // One sound, and it starts where the clip does: the engine may place it at
    // the beginning of the block that contains its position, so up to one block
    // early is expected and any lateness at all is not.
    const auto onsets = render.onsetsSeconds();
    REQUIRE (onsets.size() == 1);
    INFO ("onset: " << onsets.front());
    REQUIRE (onsets.front() <= 1.0);
    REQUIRE (onsets.front() >= 1.0 - oneRenderBlockSeconds);

    INFO ("pitch: " << render.pitchHzBetween (1.1, 1.9));
    REQUIRE (render.pitchHzBetween (1.1, 1.9) == Catch::Approx (440.0).margin (pitchToleranceHz));
}

TEST_CASE ("a MIDI note through a built-in instrument sounds when it was asked to")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();
    session.suppressDeviceRebuild();

    // Two beats at the project's own 120 bpm is one second in.
    constexpr double noteStartBeats = 2.0;
    const auto noteStartSeconds = noteStartBeats * 60.0 / session.tempoBpm();

    session.performAction ("Add a note",
                           [&] (auto& ops)
                           {
                               const auto track = ops.createTrack (TrackKind::midi, "Synth");
                               ops.addPlugin (track, BuiltinPlugin::synth, 0);

                               const auto clip = ops.insertMidiClip (track, "Note", 0.0, 3.0);
                               ops.addNote (clip, 69, noteStartBeats, 2.0, 100);
                           });

    const auto render = renderProject (session, project.folder());
    REQUIRE (render.readable());

    const auto onsets = render.onsetsSeconds();
    REQUIRE (onsets.size() == 1);
    INFO ("onset: " << onsets.front() << ", asked for " << noteStartSeconds);
    REQUIRE (onsets.front() <= noteStartSeconds);
    REQUIRE (onsets.front() >= noteStartSeconds - oneRenderBlockSeconds);

    // A4 is 440 Hz, which is what makes the instrument's output the note's and
    // not something the render happened to have in it.
    INFO ("pitch: " << render.pitchHzBetween (1.2, 1.9));
    REQUIRE (render.pitchHzBetween (1.2, 1.9) == Catch::Approx (440.0).margin (pitchToleranceHz));
}

TEST_CASE ("one track renders on its own out of a project that holds two")
{
    const TempProject project;
    const auto low = project.writeTone ("low.wav", 2.0, 440.0);
    const auto high = project.writeTone ("high.wav", 2.0, 1760.0);

    Session session { project.editFile() };
    session.useNoAudioDevice();
    session.suppressDeviceRebuild();

    TrackRef lowTrack = duet::model::noTrack;

    session.performAction ("Two tones at once",
                           [&] (auto& ops)
                           {
                               lowTrack = ops.createTrack (TrackKind::audio, "Low");
                               ops.insertAudioClip (lowTrack, "low", low, 0.0, 2.0);

                               const auto highTrack = ops.createTrack (TrackKind::audio, "High");
                               ops.insertAudioClip (highTrack, "high", high, 0.0, 2.0);
                           });

    const auto whole = renderProject (session, project.folder());
    const auto alone = renderTrack (session, lowTrack, project.folder());

    REQUIRE (whole.readable());
    REQUIRE (alone.readable());
    REQUIRE (alone.ranOffTheMessageThread());

    // Both tones are in the project's render.
    INFO ("whole project: 440 Hz at " << whole.toneLevelDbBetween (440.0, 0.1, 1.9) << " dB, 1760 "
                                      << whole.toneLevelDbBetween (1760.0, 0.1, 1.9) << " dB");
    REQUIRE (whole.toneLevelDbBetween (440.0, 0.1, 1.9) > -20.0);
    REQUIRE (whole.toneLevelDbBetween (1760.0, 0.1, 1.9) > -20.0);

    // The track's own render holds its tone and nothing of the other track's,
    // which is what makes a measurement of it a measurement of that track.
    INFO ("the low track alone: 440 Hz at "
          << alone.toneLevelDbBetween (440.0, 0.1, 1.9) << " dB, 1760 "
          << alone.toneLevelDbBetween (1760.0, 0.1, 1.9) << " dB");
    REQUIRE (alone.toneLevelDbBetween (440.0, 0.1, 1.9) > -20.0);
    REQUIRE (alone.toneLevelDbBetween (1760.0, 0.1, 1.9) < -80.0);
    REQUIRE (alone.pitchHzBetween (0.1, 1.9) == Catch::Approx (440.0).margin (pitchToleranceHz));
}

TEST_CASE ("a level change halfway through a render is measured, and an unchanged render is not")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 2.0, 440.0);

    Session session { project.editFile() };
    session.useNoAudioDevice();
    session.suppressDeviceRebuild();

    TrackRef track = duet::model::noTrack;

    session.performAction ("A steady tone",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::audio, "Tone");
                               ops.insertAudioClip (track, "tone", tone, 0.0, 2.0);
                           });

    // The assertion is only worth anything if it can also say nothing changed.
    const auto steady = renderProject (session, project.folder());
    REQUIRE (steady.readable());
    INFO ("steady: " << steady.levelChangeDb (0.1, 0.9, 1.1, 1.9) << " dB");
    REQUIRE (steady.levelChangeDb (0.1, 0.9, 1.1, 1.9)
             == Catch::Approx (0.0).margin (levelToleranceDb));

    session.performAction ("Drop the second half",
                           [&] (auto& ops)
                           {
                               ops.setAutomationPoints (AutomationTarget::trackVolumeOf (track),
                                                        { AutomationPoint { 0.0, 0.0 },
                                                          AutomationPoint { 0.99, 0.0 },
                                                          AutomationPoint { 1.0, -6.0 },
                                                          AutomationPoint { 2.0, -6.0 } });
                           });

    const auto dropped = renderProject (session, project.folder());
    REQUIRE (dropped.readable());

    INFO ("dropped: " << dropped.levelChangeDb (0.1, 0.9, 1.1, 1.9) << " dB");
    REQUIRE (dropped.levelChangeDb (0.1, 0.9, 1.1, 1.9)
             == Catch::Approx (-6.0).margin (levelToleranceDb));

    // The same drop read at the one frequency the render holds: the spectral
    // half of the question, and the half that survives a change which leaves
    // the overall level where it was.
    const auto spectralChange =
        dropped.toneLevelDbBetween (440.0, 1.1, 1.9) - dropped.toneLevelDbBetween (440.0, 0.1, 0.9);
    INFO ("at 440 Hz: " << spectralChange << " dB");
    REQUIRE (spectralChange == Catch::Approx (-6.0).margin (levelToleranceDb));
}

//==============================================================================
TEST_CASE ("the same project rendered twice in one process renders the same samples")
{
    // The determinism canary, and the only sample comparison ADR 0006 allows.
    // It says the render is a function of the project within this process; it
    // deliberately says nothing about another machine, another compiler, or
    // another day, which is why there is no file here to compare against.
    ToneProject tone;

    const auto first = renderProject (tone.session, tone.project.folder());
    const auto second = renderProject (tone.session, tone.project.folder());

    REQUIRE (first.readable());
    REQUIRE (second.readable());
    REQUIRE (first.isBitIdenticalTo (second));
}

TEST_CASE ("a render after an edit is the edit's render and not the last one")
{
    // The engine answers a repeated render out of its audio-file cache, keyed on
    // the destination, so a harness that reused a name would hand a test the
    // previous project's audio and every assertion would still pass.
    ToneProject tone;

    const auto before = renderProject (tone.session, tone.project.folder());
    REQUIRE (before.readable());

    tone.session.performAction ("Quieter",
                                [&] (auto& ops) { ops.setTrackVolumeDb (tone.track, -6.0); });

    const auto after = renderProject (tone.session, tone.project.folder());
    REQUIRE (after.readable());

    REQUIRE (before.file() != after.file());
    REQUIRE_FALSE (before.isBitIdenticalTo (after));
    INFO ("before " << before.rmsBetween (1.1, 1.9) << ", after " << after.rmsBetween (1.1, 1.9));
    REQUIRE (after.rmsBetween (1.1, 1.9) < before.rmsBetween (1.1, 1.9));
}

TEST_CASE ("no audio file is checked in for a test to compare a render against")
{
    // The rule ADR 0006 states, kept by the shape of the tree rather than by
    // remembering: a rendered file kept from an earlier run is a golden file
    // whatever it is called, so every signal a test measures is one the test
    // wrote itself, from a description of what it should be.
    static constexpr std::array audioSuffixes { ".wav", ".aif", ".aiff", ".flac", ".ogg", ".mp3",
                                                ".m4a", ".caf", ".w64",  ".raw",  ".pcm", ".mid" };

    std::vector<std::string> found;

    for (const auto& entry : std::filesystem::recursive_directory_iterator { DUET_TESTS_DIR })
    {
        if (! entry.is_regular_file())
            continue;

        auto suffix = entry.path().extension().string();
        std::transform (suffix.begin(),
                        suffix.end(),
                        suffix.begin(),
                        [] (unsigned char character)
                        { return static_cast<char> (std::tolower (character)); });

        if (std::find (audioSuffixes.begin(), audioSuffixes.end(), suffix) != audioSuffixes.end())
            found.push_back (entry.path().filename().string());
    }

    INFO ("audio files in the test tree: " << found.size());
    REQUIRE (found.empty());
}
