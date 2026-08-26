#include <duet/gui/Text.h>

#include <catch2/catch_test_macros.hpp>

using namespace duet::gui;

TEST_CASE ("a literal that is not plain ASCII crosses as one character per code point")
{
    // juce::String reads 8-bit data as ASCII unless it is told what it is, and
    // an em dash is three bytes of UTF-8. Read as ASCII it becomes three
    // characters and trips the assertion in the String constructor; read as
    // what it is, it is the one character U+2014.
    const auto dash = utf8 ("—");

    REQUIRE (dash.length() == 1);
    REQUIRE (dash[0] == static_cast<juce::juce_wchar> (0x2014));
}

TEST_CASE ("the ASCII around such a character crosses unchanged")
{
    // The window title's shape: fifteen characters, one of them the em dash.
    const auto title = utf8 ("Duet — Untitled");

    REQUIRE (title.length() == 15);
    REQUIRE (title.startsWith ("Duet "));
    REQUIRE (title.endsWith (" Untitled"));
}
