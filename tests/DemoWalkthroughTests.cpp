#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using duet::model::Session;
using duet::model::TrackKind;
using duet::testing::TempProject;

/** The app shell's vocabulary demo, step for step.

    The demo is the one place every domain of the vocabulary is exercised in
    sequence against one project, which makes it the case where an undo has the
    most to put back. It is scaffolding, but until 535bbo's surfaces replace it
    it is also what the producer judges the vocabulary by, so it is worth a test
    that walks it forwards and all the way back.
*/
TEST_CASE ("the vocabulary demo undoes step by step back to where it started")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();

    const auto track = session.tracks().front().track;
    const auto clip = session.tracks().front().clips.front().clip;

    duet::model::TrackRef bus = duet::model::noTrack;
    duet::model::TrackRef reverbBus = duet::model::noTrack;
    duet::model::PluginRef reverb = duet::model::noPlugin;

    // Every state the demo passes through, oldest first, so that undoing back
    // through them can be checked one at a time.
    std::vector<std::string> states { session.stateDigest() };

    const auto step = [&] (std::string_view name, auto&& body)
    {
        session.performAction (name, body);
        duet::testing::pumpMessages (200);
        states.push_back (session.stateDigest());
    };

    step ("Add notes",
          [clip] (auto& ops)
          {
              for (int note = 0; note < 8; ++note)
                  ops.addNote (clip, 45 + note * 2, note * 1.0, 0.9, 90);
          });

    step ("Duplicate and loop the clip",
          [&] (auto& ops)
          {
              ops.setClipLoop (clip, true, 8.0);
              ops.duplicateClip (clip, duet::model::noTrack, session.barStartSeconds (9));
          });

    step ("Route into a group bus",
          [&] (auto& ops)
          {
              bus = ops.createTrack (TrackKind::group, "Bus");
              ops.setTrackOutput (track, bus);
          });

    step ("Set the mixer and a send",
          [&] (auto& ops)
          {
              ops.setTrackVolumeDb (track, -6.0);
              ops.setTrackPan (track, -0.4);
              reverbBus = ops.createTrack (TrackKind::group, "Reverb");
              ops.setSend (track, reverbBus, -12.0);
          });

    step ("Add a reverb and set it",
          [&] (auto& ops)
          {
              // After the return, which setSend puts at the head of the bus.
              reverb = ops.addPlugin (reverbBus, duet::model::BuiltinPlugin::reverb, 1);
              ops.setPluginParameter (reverb, "room size", 0.9);
              ops.setPluginParameter (reverb, "wet level", 1.0);
              ops.setPluginParameter (reverb, "dry level", 0.0);
          });

    step ("Draw a volume curve",
          [track] (auto& ops)
          {
              ops.setAutomationPoints (duet::model::AutomationTarget::trackVolumeOf (track),
                                       { { 0.0, -24.0 }, { 4.0, -6.0 }, { 8.0, -24.0 } });
          });

    step ("Change the tempo",
          [] (auto& ops)
          {
              ops.setTempo (140.0);
              ops.setTimeSignature (6, 8);
          });

    REQUIRE (states.size() == 8);

    // The copy step 2 makes is meant to be out of earshot, so it has to land
    // clear of the stretch the transport loops over — not merely outside it,
    // and certainly not on the seam, where its first note would sound on every
    // wrap and read as a stray note nobody put there.
    const auto copy = session.tracks().front().clips.back();
    const auto loop = session.loopRangeSeconds();

    INFO ("copy at " << copy.startSeconds << "s, loop ends at " << loop.endSeconds << "s");
    REQUIRE (copy.startSeconds > loop.endSeconds);

    // Back down the stack: each undo has to land on the state the step before
    // it left behind, exactly.
    for (auto stateIndex = states.size() - 1; stateIndex > 0; --stateIndex)
    {
        INFO ("undoing step " << stateIndex);
        REQUIRE (session.undo());
        duet::testing::pumpMessages (200);
        REQUIRE (session.stateDigest() == states[stateIndex - 1]);
    }

    // And no further: the phrase the project starts from is not an edit, so
    // there is nothing left to undo and the producer is never left with an
    // empty project.
    REQUIRE_FALSE (session.undo());
    REQUIRE (session.tracks().front().clips.size() == 1);
    REQUIRE_FALSE (session.notes (session.tracks().front().clips.front().clip).empty());

    // And back up again.
    for (std::size_t stateIndex = 1; stateIndex < states.size(); ++stateIndex)
    {
        INFO ("redoing step " << stateIndex);
        REQUIRE (session.redo());
        duet::testing::pumpMessages (200);
        REQUIRE (session.stateDigest() == states[stateIndex]);
    }
}
