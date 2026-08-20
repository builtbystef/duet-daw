#include <duet/gui/Shortcuts.h>

#include <catch2/catch_test_macros.hpp>

using duet::gui::Command;
using duet::gui::panelShortcuts;
using duet::gui::Shortcuts;
using duet::gui::timelineShortcuts;

TEST_CASE ("the panel keys are the ones the interface is driven by")
{
    const auto shortcuts = panelShortcuts();

    REQUIRE (shortcuts.commandFor ({ 'b' }, false) == Command::toggleBrowser);
    REQUIRE (shortcuts.commandFor ({ 'c' }, false) == Command::toggleCollaborator);
    REQUIRE (shortcuts.commandFor ({ 'e' }, false) == Command::toggleBottomPanel);
    REQUIRE (shortcuts.commandFor ({ 'p' }, false) == Command::showPianoRoll);
    REQUIRE (shortcuts.commandFor ({ 'x' }, false) == Command::showMixer);
}

TEST_CASE ("a bare letter is the producer typing while a text field has focus")
{
    const auto shortcuts = panelShortcuts();

    for (const auto letter : { 'b', 'c', 'e', 'p', 'x' })
        REQUIRE_FALSE (shortcuts.commandFor ({ letter }, true).has_value());
}

TEST_CASE ("a letter typed with Caps Lock on means what the letter means")
{
    const auto shortcuts = panelShortcuts();

    REQUIRE (shortcuts.commandFor ({ 'B' }, false) == Command::toggleBrowser);
}

TEST_CASE ("a key the interface has registered nothing for means nothing")
{
    const auto shortcuts = panelShortcuts();

    REQUIRE_FALSE (shortcuts.commandFor ({ 'q' }, false).has_value());
    REQUIRE_FALSE (shortcuts.commandFor ({ 'b', true }, false).has_value());
}

TEST_CASE ("a key held with a modifier is not something a text field could be typing")
{
    Shortcuts shortcuts;
    shortcuts.add ({ 'b', true }, Command::toggleBrowser);

    REQUIRE (shortcuts.commandFor ({ 'b', true }, true) == Command::toggleBrowser);
    REQUIRE_FALSE (shortcuts.commandFor ({ 'b' }, false).has_value());
}

TEST_CASE ("the zoom keys are the ones spec 535bbo names")
{
    const auto shortcuts = timelineShortcuts();

    REQUIRE (shortcuts.commandFor ({ '-' }, false) == Command::zoomOut);
    REQUIRE (shortcuts.commandFor ({ '0' }, false) == Command::zoomToFit);

    // A keyboard that types + with Shift and one that has a key of its own for
    // it both mean zoom in, and so does the = the + is printed above.
    REQUIRE (shortcuts.commandFor ({ '+' }, false) == Command::zoomIn);
    REQUIRE (shortcuts.commandFor ({ '+', false, false, true }, false) == Command::zoomIn);
    REQUIRE (shortcuts.commandFor ({ '=' }, false) == Command::zoomIn);
}
