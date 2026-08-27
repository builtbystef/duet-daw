#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** Loud enough that nobody would call it silence, and far enough below the
    level the demo phrase actually reaches that the assertion is about being
    heard and not about a gain staying put.
*/
constexpr double audibleDb = -40.0;

/** How much of the project each case plays. A second is several hundred blocks
    and covers the demo phrase's first notes with room to spare.
*/
constexpr double playedSeconds = 1.0;
} // namespace

/** What the playback graph puts out.

    These are the assertions the offline render cannot make. The engine builds
    one graph to play through and another to render, and the playing one is the
    one that wraps a track with nowhere to go in a node that blocks its audio —
    which is how a group bus that swallowed everything routed into it shipped
    through a green suite (ADR 0006).
*/
TEST_CASE ("a track routed through a group bus is heard at the output")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();

    const auto track = session.tracks().front().track;
    TrackRef bus = duet::model::noTrack;

    session.performAction ("Route into a group bus",
                           [&] (auto& ops)
                           {
                               bus = ops.createTrack (TrackKind::group, "Bus");
                               ops.setTrackOutput (track, bus);
                           });

    duet::testing::pumpMessages (200);

    REQUIRE (session.playWithoutAudioDevice (playedSeconds));

    // Read before asserting: a meter clears as it is read, so the INFO has to
    // hold the numbers the assertions are about.
    const auto trackPeak = session.trackPeakDb (track);
    const auto busPeak = session.trackPeakDb (bus);
    const auto outputPeak = session.outputPeakDb();

    INFO ("track " << trackPeak << " dB, bus " << busPeak << " dB, output " << outputPeak << " dB");

    REQUIRE (trackPeak > audibleDb);
    REQUIRE (busPeak > audibleDb);
    REQUIRE (outputPeak > audibleDb);
}

TEST_CASE ("muting a track silences it during playback")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();

    const auto track = session.tracks().front().track;
    session.performAction ("Mute Track", [track] (auto& ops) { ops.setTrackMute (track, true); });
    duet::testing::pumpMessages (200);

    REQUIRE (session.playWithoutAudioDevice (playedSeconds));
    REQUIRE (session.trackPeakDb (track) <= duet::model::silentDb);
    REQUIRE (session.outputPeakDb() <= duet::model::silentDb);
}

TEST_CASE ("solo silences the other tracks and unsolo restores them")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();
    const auto first = session.tracks().front().track;
    TrackRef second = duet::model::noTrack;
    session.performAction ("Duplicate the phrase",
                           [&] (auto& ops) { second = ops.duplicateTrack (first); });
    session.performAction ("Solo Track", [first] (auto& ops) { ops.setTrackSolo (first, true); });
    duet::testing::pumpMessages (200);

    REQUIRE (session.playWithoutAudioDevice (playedSeconds));
    REQUIRE (session.trackPeakDb (first) > audibleDb);
    REQUIRE (session.trackPeakDb (second) <= duet::model::silentDb);

    session.setPlaybackPositionSeconds (0.0);
    session.performAction ("Unsolo Track",
                           [first] (auto& ops) { ops.setTrackSolo (first, false); });
    duet::testing::pumpMessages (200);
    REQUIRE (session.playWithoutAudioDevice (playedSeconds));
    REQUIRE (session.trackPeakDb (second) > audibleDb);
}

TEST_CASE ("undoing a sounding MIDI note leaves no voice stuck behind")
{
    const TempProject project;
    Session session { project.editFile() };
    const auto track = session.tracks().front().track;
    duet::model::ClipRef clip = duet::model::noClip;

    session.performAction ("Set up the instrument",
                           [&] (auto& ops)
                           {
                               ops.addPlugin (track, duet::model::BuiltinPlugin::synth, 0);
                               clip = ops.insertMidiClip (track, "Tone", 0.0, 4.0);
                           });
    session.performAction ("Add the note",
                           [clip] (auto& ops) { ops.addNote (clip, 57, 0.0, 0.4, 100); });

    session.useNoAudioDevice();
    session.startPlayback();
    session.runWithoutAudioDevice (0.1);
    REQUIRE (session.outputPeakDb() > audibleDb);

    REQUIRE (session.undo());
    REQUIRE (session.notes (clip).empty());

    session.runWithoutAudioDevice (0.5);
    const auto afterUndo = session.outputPeakDb();
    INFO ("after undoing its note, the voice peaks at " << afterUndo << " dB");
    REQUIRE (afterUndo <= duet::model::silentDb);

    session.stopPlayback();
}

TEST_CASE ("fast undo presses leave no MIDI voice behind")
{
    const TempProject project;
    Session session { project.editFile() };
    const auto track = session.tracks().front().track;
    duet::model::ClipRef clip = duet::model::noClip;

    session.performAction ("Set up the instrument",
                           [&] (auto& ops)
                           {
                               ops.addPlugin (track, duet::model::BuiltinPlugin::synth, 0);
                               clip = ops.insertMidiClip (track, "Tone", 0.0, 4.0);
                           });
    session.performAction ("Add the note",
                           [clip] (auto& ops) { ops.addNote (clip, 57, 0.0, 0.4, 100); });

    for (int change = 0; change < 6; ++change)
        session.performAction ("Rename the track",
                               [track, change] (auto& ops)
                               { ops.renameTrack (track, "Tone " + std::to_string (change)); });

    session.useNoAudioDevice();
    session.startPlayback();
    session.runWithoutAudioDevice (0.1);
    REQUIRE (session.outputPeakDb() > audibleDb);

    for (int press = 0; press < 7; ++press)
        REQUIRE (session.undo());

    REQUIRE (session.notes (clip).empty());
    session.runWithoutAudioDevice (0.5);
    REQUIRE (session.outputPeakDb() <= duet::model::silentDb);

    session.stopPlayback();
}

TEST_CASE ("a loop pass after an undo sounds only the notes the project contains")
{
    const TempProject project;
    Session session { project.editFile() };
    const auto track = session.tracks().front().track;
    duet::model::ClipRef clip = duet::model::noClip;

    session.performAction ("Set up the loop",
                           [&] (auto& ops)
                           {
                               ops.addPlugin (track, duet::model::BuiltinPlugin::synth, 0);
                               clip = ops.insertMidiClip (track, "Loop", 0.0, 2.0);
                               ops.addNote (clip, 57, 0.0, 0.2, 100);
                           });
    session.performAction ("Add the note to remove",
                           [clip] (auto& ops) { ops.addNote (clip, 69, 0.0, 0.4, 100); });
    session.setLoopRangeSeconds (0.0, 1.0);
    session.setLooping (true);

    session.useNoAudioDevice();
    session.startPlayback();
    session.runWithoutAudioDevice (0.1);
    REQUIRE (session.outputPeakDb() > audibleDb);

    REQUIRE (session.undo());
    REQUIRE (session.notes (clip).size() == 1);

    // Finish this pass, then cross the seam. The short note that remains in
    // the project sounds at the opening of the next pass.
    session.runWithoutAudioDevice (0.8);
    static_cast<void> (session.outputPeakDb());
    session.runWithoutAudioDevice (0.3);
    REQUIRE (session.outputPeakDb() > audibleDb);

    // Past that note and its release, the pass is silent. Before the fix the
    // removed long note stayed at about -11 dB through this whole stretch.
    session.runWithoutAudioDevice (0.3);
    static_cast<void> (session.outputPeakDb());
    session.runWithoutAudioDevice (0.25);
    REQUIRE (session.outputPeakDb() <= duet::model::silentDb);

    session.stopPlayback();
}

TEST_CASE ("redoing the removal of a sounding MIDI note leaves no voice stuck behind")
{
    const TempProject project;
    Session session { project.editFile() };
    const auto track = session.tracks().front().track;
    duet::model::ClipRef clip = duet::model::noClip;
    duet::model::NoteRef note = duet::model::noNote;

    session.performAction ("Set up the note",
                           [&] (auto& ops)
                           {
                               ops.addPlugin (track, duet::model::BuiltinPlugin::synth, 0);
                               clip = ops.insertMidiClip (track, "Tone", 0.0, 4.0);
                               note = ops.addNote (clip, 57, 0.0, 0.4, 100);
                           });
    session.performAction ("Remove the note", [note] (auto& ops) { ops.removeNote (note); });
    REQUIRE (session.undo());

    session.useNoAudioDevice();
    session.startPlayback();
    session.runWithoutAudioDevice (0.1);
    REQUIRE (session.outputPeakDb() > audibleDb);

    REQUIRE (session.redo());
    REQUIRE (session.notes (clip).empty());

    session.runWithoutAudioDevice (0.5);
    const auto afterRedo = session.outputPeakDb();
    INFO ("after redoing its removal, the voice peaks at " << afterRedo << " dB");
    REQUIRE (afterRedo <= duet::model::silentDb);

    session.stopPlayback();
}

TEST_CASE ("the output says when a project has run out of headroom")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();

    const auto track = session.tracks().front().track;

    REQUIRE (session.playWithoutAudioDevice (playedSeconds));

    const auto asPlayed = session.outputPeakDb();
    INFO ("the demo phrase peaks at " << asPlayed << " dB");
    REQUIRE (asPlayed > audibleDb);
    REQUIRE (asPlayed < 0.0);

    // Full scale is 0 dB, and a project can be pushed past it the way any
    // producer pushes one past it: the fader as far up as it goes, and the same
    // signal a second time through a bus on top of that.
    session.performAction ("Pile the gain up",
                           [track] (auto& ops)
                           {
                               ops.setTrackVolumeDb (track, 6.0);
                               const auto bus = ops.createTrack (TrackKind::group, "Loud");
                               ops.setSend (track, bus, 6.0);
                               ops.setTrackVolumeDb (bus, 6.0);
                           });

    duet::testing::pumpMessages (200);

    REQUIRE (session.playWithoutAudioDevice (playedSeconds));

    const auto pushed = session.outputPeakDb();
    INFO ("piled up, the output peaks at " << pushed << " dB");

    REQUIRE (pushed >= 0.0);
}

TEST_CASE ("a project played through a real audio device is heard at the output")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();

    if (session.audioDeviceDescription().empty())
        SKIP ("this machine has no audio device to play through");

    REQUIRE (duet::testing::playUntilRolling (session));
    duet::testing::pumpMessages (1000);

    const auto outputPeak = session.outputPeakDb();
    INFO ("output " << outputPeak << " dB on " << session.audioDeviceDescription());

    REQUIRE (outputPeak > audibleDb);

    session.stopPlayback();
}
