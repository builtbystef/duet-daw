#include <duet/gui/Shortcuts.h>

#include <algorithm>
#include <cctype>

namespace duet::gui
{
namespace
{
    /** The key, with the case the producer happened to type it in taken out. */
    KeyStroke normalised (KeyStroke key)
    {
        if (key.character >= 0 && key.character <= 127)
            key.character = std::tolower (key.character);

        return key;
    }

    /** True for a key press a text field would turn into a character. */
    bool isBareKey (const KeyStroke& key) { return ! key.ctrl && ! key.alt; }
} // namespace

void Shortcuts::add (KeyStroke key, Command command)
{
    const auto registered = normalised (key);
    const auto existing =
        std::find_if (entries.begin(),
                      entries.end(),
                      [&registered] (const auto& entry) { return entry.first == registered; });

    if (existing != entries.end())
        existing->second = command;
    else
        entries.emplace_back (registered, command);
}

void Shortcuts::add (const Shortcuts& other)
{
    for (const auto& [key, command] : other.entries)
        add (key, command);
}

std::optional<Command> Shortcuts::commandFor (KeyStroke key, bool textFieldHasFocus) const
{
    const auto pressed = normalised (key);

    if (textFieldHasFocus && isBareKey (pressed))
        return std::nullopt;

    const auto found =
        std::find_if (entries.begin(),
                      entries.end(),
                      [&pressed] (const auto& entry) { return entry.first == pressed; });

    return found == entries.end() ? std::nullopt : std::optional { found->second };
}

Shortcuts timelineShortcuts()
{
    Shortcuts shortcuts;

    // A punctuation key says nothing about whether Shift produced the character
    // or modified it: a + is a Shift and an = on one keyboard and a key of its
    // own on another, and the producer means the same thing either way.
    shortcuts.add ({ '+' }, Command::zoomIn);
    shortcuts.add ({ '+', false, false, true }, Command::zoomIn);
    shortcuts.add ({ '=' }, Command::zoomIn);
    shortcuts.add ({ '-' }, Command::zoomOut);
    shortcuts.add ({ '_', false, false, true }, Command::zoomOut);
    shortcuts.add ({ '0' }, Command::zoomToFit);

    return shortcuts;
}

Shortcuts arrangementShortcuts()
{
    Shortcuts shortcuts;
    shortcuts.add ({ 'a', true }, Command::selectAll);
    shortcuts.add ({ 'x', true }, Command::cut);
    shortcuts.add ({ 'c', true }, Command::copy);
    shortcuts.add ({ 'v', true }, Command::paste);
    shortcuts.add ({ 'd', true }, Command::duplicate);
    shortcuts.add ({ deleteKeyCode }, Command::deleteSelection);
    shortcuts.add ({ escapeKeyCode }, Command::cancel);
    shortcuts.add ({ f2KeyCode }, Command::rename);
    return shortcuts;
}

Shortcuts panelShortcuts()
{
    Shortcuts shortcuts;

    shortcuts.add ({ 'b' }, Command::toggleBrowser);
    shortcuts.add ({ 'c' }, Command::toggleCollaborator);
    shortcuts.add ({ 'e' }, Command::toggleBottomPanel);
    shortcuts.add ({ 'p' }, Command::showPianoRoll);
    shortcuts.add ({ 'x' }, Command::showMixer);

    return shortcuts;
}
} // namespace duet::gui
