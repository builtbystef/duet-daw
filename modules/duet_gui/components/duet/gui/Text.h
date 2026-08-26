#pragma once

#include <juce_core/juce_core.h>

namespace duet::gui
{
/** A literal of Duet's own text, as JUCE spells a string.

    `juce::String (const char*)` reads 8-bit data as ASCII, because nothing in
    the bytes says which encoding they are; anything above 127 trips an
    assertion in a Debug build and arrives as one character per byte. Duet's
    sources are UTF-8, so an em dash written in one of them is three bytes that
    have to cross as the one character they spell — and this is the crossing.

    The rule the whole tree keeps: a literal that is not plain ASCII reaches
    `juce::String` only through here. A source that never mentions JUCE may hold
    one as it is, because `juce::String` already reads a `std::string` as UTF-8
    and there is no `const char*` for it to take instead.
*/
[[nodiscard]] inline juce::String utf8 (const char* literal)
{
    return juce::String { juce::CharPointer_UTF8 (literal) };
}
} // namespace duet::gui
