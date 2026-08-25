#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE ("the specialised Master has mixer values and inserts without becoming a track")
{
    const duet::testing::TempProject project;
    duet::model::Session session { project.editFile() };
    const auto trackCount = session.tracks().size();
    const auto before = session.stateDigest();
    duet::model::PluginRef effect = duet::model::noPlugin;

    session.performAction ("Mix Master",
                           [&] (auto& ops)
                           {
                               ops.setTrackVolumeDb (duet::model::masterChannel, -6.0);
                               ops.setTrackPan (duet::model::masterChannel, 0.25);
                               effect = ops.addPlugin (
                                   duet::model::masterChannel, duet::model::BuiltinPlugin::eq, 0);
                           });

    const auto master = session.master();
    REQUIRE (session.tracks().size() == trackCount);
    REQUIRE (master.channel == duet::model::masterChannel);
    REQUIRE_THAT (master.volumeDb, WithinAbs (-6.0, 0.001));
    REQUIRE_THAT (master.pan, WithinAbs (0.25, 0.000001));
    REQUIRE (master.plugins.size() == 1);
    REQUIRE (master.plugins.front().plugin == effect);

    session.performAction (
        "Mute Master", [&] (auto& ops) { ops.setTrackMute (duet::model::masterChannel, true); });
    REQUIRE (session.master().muted);
    REQUIRE_THAT (session.master().volumeDb, WithinAbs (-6.0, 0.001));
    REQUIRE (session.undo());
    REQUIRE_FALSE (session.master().muted);

    session.performAction ("Bypass Plugin",
                           [&] (auto& ops) { ops.setPluginBypassed (effect, true); });
    REQUIRE (session.master().plugins.front().bypassed);
    REQUIRE (session.undo());
    REQUIRE_FALSE (session.master().plugins.front().bypassed);

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == before);
}
