#include <duet/gui/WindowGeometry.h>

#include <duet/gui/Settings.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

using duet::gui::storedWindowBounds;
using duet::gui::storeWindowBounds;
using duet::gui::WindowBounds;
using duet::testing::StoredSettings;

TEST_CASE ("a first launch has no window to put back")
{
    const StoredSettings store;

    REQUIRE_FALSE (storedWindowBounds (store).has_value());
}

TEST_CASE ("the window opens where the producer left it, whichever project opens")
{
    StoredSettings store;
    const WindowBounds left { 120, 64, 1680, 1020 };

    storeWindowBounds (store, left);

    // The store outlives every project and every window: the next launch reads
    // the same geometry whatever it goes on to open.
    REQUIRE (storedWindowBounds (store) == left);
}

TEST_CASE ("a geometry the window could not be opened at is no geometry")
{
    StoredSettings store;

    SECTION ("a window with no size")
    {
        storeWindowBounds (store, { 10, 10, 0, 800 });

        REQUIRE_FALSE (storedWindowBounds (store).has_value());
    }

    SECTION ("a stored value nothing wrote")
    {
        store.setValue (duet::gui::settingKey::windowBounds, "the window, roughly");

        REQUIRE_FALSE (storedWindowBounds (store).has_value());
    }

    SECTION ("a stored value with a number missing")
    {
        store.setValue (duet::gui::settingKey::windowBounds, "120 64 1680");

        REQUIRE_FALSE (storedWindowBounds (store).has_value());
    }
}
