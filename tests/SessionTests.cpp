#include <duet/model/Session.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE ("a new session opens one empty audio track at 120 bpm")
{
    const duet::model::Session session;

    REQUIRE (session.audioTrackCount() == 1);
    REQUIRE_THAT (session.tempoBpm(), WithinAbs (120.0, 0.001));
    REQUIRE_THAT (session.editLengthSeconds(), WithinAbs (0.0, 0.001));
    REQUIRE_FALSE (session.isPlaying());
}

TEST_CASE ("the demo phrase gives the session eight seconds to play")
{
    duet::model::Session session;
    session.loadDemoContent();

    REQUIRE_THAT (session.editLengthSeconds(), WithinAbs (8.0, 0.001));
}
