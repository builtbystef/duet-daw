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
    showMixer,

    /** The arrangement's, and every later surface that draws musical time: the
        zoom keys spec 535bbo names.
    */
    zoomIn,
    zoomOut,
    zoomToFit,

    selectAll,
    cut,
    copy,
    paste,
    duplicate,
    deleteSelection,
    rename,
    cancel,

    togglePlayback,
    toggleRecording,
    toggleLoop,
    toggleMetronome,
    toggleFollowPlayhead,
    goToStart,
    goToEnd,
    undo,
    redo,
    save
};

/** A key press, as the policy sees it. No JUCE type crosses this seam, so what
    a key means can be asserted with no window on screen.
*/
struct KeyStroke
{
    /** The character the key would type. Case is not significant: a producer
        with Caps Lock on means the same key.
    */
    int character = 0;

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

    /** Registers everything another table holds, the same way: what the main
        window does with the tables its surfaces bring it.
    */
    void add (const Shortcuts& other);

    /** What a press means right now, or nothing when it means nothing. */
    [[nodiscard]] std::optional<Command> commandFor (KeyStroke key, bool textFieldHasFocus) const;

private:
    std::vector<std::pair<KeyStroke, Command>> entries;
};

/** The keys the main window shell registers: the panel toggles and the two
    bottom tabs (spec 535bbo).
*/
[[nodiscard]] Shortcuts panelShortcuts();

/** The keys the surfaces that draw musical time register: zoom in, zoom out,
    and zoom to fit.
*/
[[nodiscard]] Shortcuts timelineShortcuts();

/** Selection and clipboard routes of the smart tool. */
[[nodiscard]] Shortcuts arrangementShortcuts();

/** Global transport, history and save routes. */
[[nodiscard]] Shortcuts transportShortcuts();

inline constexpr int deleteKeyCode = 0x10000;
inline constexpr int escapeKeyCode = 0x10001;
inline constexpr int f2KeyCode = 0x10002;
inline constexpr int homeKeyCode = 0x10003;
inline constexpr int endKeyCode = 0x10004;
} // namespace duet::gui
