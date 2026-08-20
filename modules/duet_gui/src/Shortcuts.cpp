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
        key.character =
            static_cast<char> (std::tolower (static_cast<unsigned char> (key.character)));

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
