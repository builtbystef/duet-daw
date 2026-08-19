#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using duet::model::InputKind;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
int midiInputCount (const Session& session)
{
    const auto inputs = session.availableInputs();
    return static_cast<int> (std::count_if (inputs.begin(),
                                            inputs.end(),
                                            [] (const auto& input)
                                            { return input.kind == InputKind::midi; }));
}

/** An armed audio track on the hosted device, so a Record can start a take. */
TrackRef armATrack (Session& session)
{
    TrackRef guitar = duet::model::noTrack;
    session.performAction (
        "Add a track", [&] (auto& ops) { guitar = ops.createTrack (TrackKind::audio, "Guitar"); });

    for (const auto& input : session.availableInputs())
    {
        if (input.kind == InputKind::audio)
        {
            session.setTrackInput (guitar, input.input);
            session.setTrackRecordArmed (guitar, true);
            return guitar;
        }
    }

    return duet::model::noTrack;
}
} // namespace

TEST_CASE ("a test can make the engine's device rebuild happen, or not happen", "[devices]")
{
    const TempProject project;
    Session session { project.editFile() };

    session.suppressDeviceRebuild();

    // Long enough for an asked-for apply (5 ms, then another 5 ms) and nowhere
    // near the engine's own four-second timer.
    duet::testing::pumpMessages (50);

    REQUIRE (midiInputCount (session) == 0);

    session.rebuildDevices();

    REQUIRE (midiInputCount (session) > 0);
}

TEST_CASE ("a commanded device rebuild stops a rolling transport, and the model starts it again",
           "[devices]")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();
    session.loadDemoContent();
    session.suppressDeviceRebuild();

    session.startPlayback();
    REQUIRE (session.isPlaying());

    session.rebuildDevices();

    // The rebuild frees the playback graph (hazard 6). Visible here, with no
    // audio device and no four-second wait, so it runs in CI.
    REQUIRE_FALSE (session.isPlaying());

    // The model's keeper asks again, which is what one press of Play is for.
    duet::testing::pumpMessages (200);
    REQUIRE (session.isPlaying());
}

TEST_CASE ("a take waits while the engine's devices are still churning", "[devices]")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();
    session.setDeviceWait (10000, 1, 1000);
    REQUIRE (armATrack (session) != duet::model::noTrack);

    // The hosted switch broadcasts a device change. Let that land so the
    // settle-wait has something recent to measure against.
    duet::testing::pumpMessages (10);

    session.startRecording();
    REQUIRE_FALSE (session.isRecording());

    duet::testing::pumpMessages (20);
    REQUIRE_FALSE (session.isRecording());
}

TEST_CASE ("a take starts once the engine's devices go still", "[devices]")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();
    session.setDeviceWait (10000, 1, 1000);
    REQUIRE (armATrack (session) != duet::model::noTrack);

    duet::testing::pumpMessages (10);

    session.startRecording();
    REQUIRE_FALSE (session.isRecording());

    session.setDeviceWait (0, 1, 1000);
    duet::testing::pumpMessages (5);
    REQUIRE (session.isRecording());

    session.stopRecording();
}

TEST_CASE ("a take starts at the bound even if the devices never settle", "[devices]")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();
    session.setDeviceWait (10000, 1, 0);
    REQUIRE (armATrack (session) != duet::model::noTrack);

    duet::testing::pumpMessages (10);

    session.startRecording();
    REQUIRE_FALSE (session.isRecording());

    duet::testing::pumpMessages (5);
    REQUIRE (session.isRecording());

    session.stopRecording();
}

TEST_CASE ("Record as the first gesture of a session waits when the device list is not built",
           "[devices]")
{
    const TempProject project;
    Session session { project.editFile() };
    session.suppressDeviceRebuild();
    session.setDeviceWait (100, 1, 0);

    // The take itself cannot start: nothing is armed, and arming needs an
    // input, and an input needs the device list. What this can still say is
    // that Record as the first gesture enters the wait, which is the lower
    // bound's door. The take starting at the bound is the case above; the
    // take surviving on a real unbuilt list is the hardware first-gesture
    // case.
    session.startRecording();
    REQUIRE_FALSE (session.isRecording());
    REQUIRE (midiInputCount (session) == 0);

    duet::testing::pumpMessages (20);
    REQUIRE_FALSE (session.isRecording());
}
