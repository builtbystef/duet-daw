#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using duet::model::ClipRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

TEST_CASE ("a clip is duplicated to a bar, on its own track and on another")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 4.0, 440.0);
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;
    TrackRef pad = duet::model::noTrack;
    ClipRef clip = duet::model::noClip;

    session.performAction ("Lay out the loop",
                           [&] (auto& ops)
                           {
                               keys = ops.createTrack (TrackKind::audio, "Keys");
                               pad = ops.createTrack (TrackKind::audio, "Pad");
                               clip = ops.insertAudioClip (keys, "tone", tone, 0.0, 4.0);
                           });

    const auto barFive = session.barStartSeconds (5);
    const auto beforeDuplicate = session.stateDigest();

    SECTION ("to bar five of the same track")
    {
        session.performAction ("Duplicate the clip",
                               [&] (auto& ops)
                               { ops.duplicateClip (clip, duet::model::noTrack, barFive); });

        const auto clips = session.track (keys).clips;

        REQUIRE (clips.size() == 2);

        const auto& copy = clips.back();

        REQUIRE_THAT (copy.startSeconds, WithinAbs (barFive, 0.000001));
        REQUIRE_THAT (copy.lengthSeconds, WithinAbs (4.0, 0.000001));

        // The copy carries the original's reference, which the project pinned
        // relative to itself: a duplicate never resolves against TEMP either.
        REQUIRE_FALSE (std::filesystem::path { copy.sourceReference }.is_absolute());
        REQUIRE (copy.sourceReference == clips.front().sourceReference);
        REQUIRE (copy.sourceFile == tone);

        REQUIRE (session.undoNames().front() == "Duplicate the clip");
        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeDuplicate);
    }

    SECTION ("to bar five of another track")
    {
        session.performAction ("Duplicate to the pad",
                               [&] (auto& ops) { ops.duplicateClip (clip, pad, barFive); });

        REQUIRE (session.track (keys).clips.size() == 1);

        const auto onPad = session.track (pad).clips;

        REQUIRE (onPad.size() == 1);
        REQUIRE_THAT (onPad.front().startSeconds, WithinAbs (barFive, 0.000001));
        REQUIRE (onPad.front().sourceFile == tone);

        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeDuplicate);
    }
}

TEST_CASE ("a duplicated MIDI clip carries the notes of the one it came from")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;
    ClipRef phrase = duet::model::noClip;

    session.performAction ("Write a phrase",
                           [&] (auto& ops)
                           {
                               keys = ops.createTrack (
                                   TrackKind::midi, "Keys", duet::model::BuiltinPlugin::synth);
                               phrase = ops.insertMidiClip (keys, "Phrase", 0.0, 8.0);
                               ops.addNote (phrase, 60, 0.0, 1.0, 100);
                               ops.addNote (phrase, 64, 1.0, 1.0, 90);
                           });

    const auto barFive = session.barStartSeconds (5);

    session.performAction ("Repeat the phrase",
                           [&] (auto& ops)
                           { ops.duplicateClip (phrase, duet::model::noTrack, barFive); });

    const auto clips = session.track (keys).clips;

    REQUIRE (clips.size() == 2);
    REQUIRE (clips.back().holdsMidi);
    REQUIRE_THAT (clips.back().startSeconds, WithinAbs (barFive, 0.000001));

    const auto copied = session.notes (clips.back().clip);

    REQUIRE (copied.size() == 2);
    REQUIRE (copied.front().pitch == 60);
    REQUIRE (copied.back().pitch == 64);
    REQUIRE (copied.back().velocity == 90);

    // The copy's notes are its own: editing one is not editing the original's.
    REQUIRE (copied.front().note != session.notes (phrase).front().note);
}

TEST_CASE ("a clip is set looping for a length in beats, and set back")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 2.0, 440.0);
    Session session { project.editFile() };

    ClipRef clip = duet::model::noClip;

    session.performAction (
        "Lay out the loop",
        [&] (auto& ops)
        { clip = ops.insertAudioClip (session.tracks().front().track, "tone", tone, 0.0, 8.0); });

    const auto beforeLooping = session.stateDigest();

    // Two bars of four beats: the loop is stated in beats, so it stays two bars
    // long whatever the tempo does afterwards.
    constexpr double twoBars = 8.0;

    session.performAction ("Loop the clip",
                           [&] (auto& ops) { ops.setClipLoop (clip, true, twoBars); });

    const auto looped = session.tracks().front().clips.front();

    REQUIRE (looped.looped);
    REQUIRE_THAT (looped.loopLengthBeats, WithinAbs (twoBars, 0.000001));

    REQUIRE (session.undoNames().front() == "Loop the clip");
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforeLooping);
    REQUIRE_FALSE (session.tracks().front().clips.front().looped);

    REQUIRE (session.redo());
    REQUIRE (session.tracks().front().clips.front().looped);

    session.performAction ("Stop the clip looping",
                           [&] (auto& ops) { ops.setClipLoop (clip, false, 0.0); });

    REQUIRE_FALSE (session.tracks().front().clips.front().looped);
    REQUIRE (session.tracks().front().clips.front().loopLengthBeats == 0.0);
}
