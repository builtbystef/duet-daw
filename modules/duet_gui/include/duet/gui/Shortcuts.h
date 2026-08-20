#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace duet::gui
{
/** What a key press means to the interface.

    The shell's own, this slice: later surfaces add their enumerators here and
    register their keys with the same table, so that there is one answer to what
    a key does and one place the rule below is applied.
*/
enum class Command : std::uint8_t
{
    toggleBrowser,
    toggleCollaborator,
    toggleBottomPanel,
    showPianoRoll,
    showMixer
};

/** A key press, as the policy sees it. No JUCE type crosses this seam, so what
    a key means can be asserted with no window on screen.
*/
struct KeyStroke
{
    /** The character the key would type. Case is not significant: a producer
        with Caps Lock on means the same key.
    */
    char character = 0;

    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    friend constexpr bool operator== (const KeyStroke& first, const KeyStroke& second) = default;
};

/** The interface's keyboard policy: which command a key press means, and when.

    One rule decides it, and it is why this is a policy and not a switch inside
    a component: a key held with no Ctrl and no Alt is a bare key, and a bare key
    is the producer typing whenever a text field has the focus. B does not
    toggle the browser while a track is being renamed. A key held with a modifier
    is never something a text field could be typing, so it always means what it
    means.
*/
class Shortcuts
{
public:
    /** Registers what a key press means. The last registration for a key wins. */
    void add (KeyStroke key, Command command);

    /** What a press means right now, or nothing when it means nothing. */
    [[nodiscard]] std::optional<Command> commandFor (KeyStroke key, bool textFieldHasFocus) const;

private:
    std::vector<std::pair<KeyStroke, Command>> entries;
};

/** The keys the main window shell registers: the panel toggles and the two
    bottom tabs (spec 535bbo).
*/
[[nodiscard]] Shortcuts panelShortcuts();
} // namespace duet::gui
