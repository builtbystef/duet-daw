#include <duet/gui/Rendering.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

using duet::gui::hardwareAccelerationEnabled;
using duet::gui::setHardwareAccelerationEnabled;
using duet::testing::StoredSettings;

TEST_CASE ("the surfaces are on the software renderer until the producer says otherwise")
{
    const StoredSettings store;

    REQUIRE_FALSE (hardwareAccelerationEnabled (store));
}

TEST_CASE ("the escape hatch the producer opened is still open at the next launch")
{
    StoredSettings store;

    setHardwareAccelerationEnabled (store, true);
    REQUIRE (hardwareAccelerationEnabled (store));

    setHardwareAccelerationEnabled (store, false);
    REQUIRE_FALSE (hardwareAccelerationEnabled (store));
}
