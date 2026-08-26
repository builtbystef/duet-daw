#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{
/** Where a string literal begins in a source, and whether it holds a byte that
    ASCII cannot spell.
*/
struct Literal
{
    std::size_t quote = 0;
    bool nonAscii = false;
};

/** Every string literal in a source, comments and character literals skipped.

    A byte above 127 means as little to this scan as it does to `juce::String`:
    what it looks for is the literal that holds one, wherever in the file it
    sits. Duet's sources have no raw string literals, so a quote always opens
    and a backslash always escapes; the one quote that opens nothing is the
    digit separator in a number like `3'600'000`, which follows a digit.
*/
[[nodiscard]] std::vector<Literal> stringLiterals (const std::string& source)
{
    enum class State : std::uint8_t
    {
        code,
        lineComment,
        blockComment,
        character,
        string
    };

    std::vector<Literal> found;
    auto state = State::code;
    Literal current;

    for (std::size_t index = 0; index < source.size(); ++index)
    {
        const auto here = source[index];
        const auto next = index + 1 < source.size() ? source[index + 1] : '\0';
        const auto previous = index > 0 ? source[index - 1] : '\0';

        switch (state)
        {
            case State::code:
                if (here == '/' && next == '/')
                {
                    state = State::lineComment;
                    ++index;
                }
                else if (here == '/' && next == '*')
                {
                    state = State::blockComment;
                    ++index;
                }
                else if (here == '\'' && std::isdigit (static_cast<unsigned char> (previous)) == 0)
                {
                    state = State::character;
                }
                else if (here == '"')
                {
                    state = State::string;
                    current = { index, false };
                }

                break;

            case State::lineComment:
                if (here == '\n')
                    state = State::code;

                break;

            case State::blockComment:
                if (here == '*' && next == '/')
                {
                    state = State::code;
                    ++index;
                }

                break;

            case State::character:
                if (here == '\\')
                    ++index;
                else if (here == '\'')
                    state = State::code;

                break;

            case State::string:
                if (here == '\\')
                {
                    ++index;
                }
                else if (here == '"')
                {
                    found.push_back (current);
                    state = State::code;
                }
                else if (static_cast<unsigned char> (here) > 127)
                {
                    current.nonAscii = true;
                }

                break;
        }
    }

    return found;
}

/** Whether the literal opening at this quote is the argument of the crossing. */
[[nodiscard]] bool crossesAsUtf8 (const std::string& source, std::size_t quote)
{
    constexpr std::string_view crossing = "utf8 (";

    return quote >= crossing.size()
           && source.compare (quote - crossing.size(), crossing.size(), crossing) == 0;
}

[[nodiscard]] std::size_t lineOf (const std::string& source, std::size_t position)
{
    return 1
           + static_cast<std::size_t> (std::count (
               source.begin(), source.begin() + static_cast<std::ptrdiff_t> (position), '\n'));
}

/** The line of every literal in a source that crosses as 8-bit text. */
[[nodiscard]] std::vector<std::size_t> offendingLines (const std::string& source)
{
    std::vector<std::size_t> lines;

    for (const auto& literal : stringLiterals (source))
        if (literal.nonAscii && ! crossesAsUtf8 (source, literal.quote))
            lines.push_back (lineOf (source, literal.quote));

    return lines;
}
} // namespace

TEST_CASE ("the scan reads a literal apart from a comment, a character and a number")
{
    // What the guard below is worth is what this scan can tell apart, and a
    // scan that had gone blind would pass it silently. The em dash is the one
    // character in each of these; only the last is a literal that crosses.
    const std::string source = "// an em dash — in a line comment\n"
                               "/* and — in a block one */\n"
                               "const auto tick = 3'600'000;\n"
                               "const auto quote = '\\'';\n"
                               "const auto safe = utf8 (\"Duet — \");\n"
                               "const auto broken = juce::String { \"Duet — \" };\n";

    REQUIRE (offendingLines (source) == std::vector<std::size_t> { 6 });
}

TEST_CASE ("a literal that is not plain ASCII reaches juce::String only through the crossing")
{
    // `juce::String (const char*)` reads its bytes as ASCII, so a UTF-8 literal
    // handed to it arrives as one character per byte and trips an assertion on
    // the way. `duet::gui::utf8` is the one crossing that says what the bytes
    // are, and this is the rule about the whole tree that keeps it the only one.
    //
    // A source that never mentions JUCE is exempt, and structurally so: it has
    // no juce::String to build, and when its text does reach one it is a
    // std::string by then, which JUCE already reads as UTF-8.
    std::vector<std::string> offenders;

    for (const auto& entry : std::filesystem::recursive_directory_iterator { DUET_MODULES_DIR })
    {
        const auto& file = entry.path();

        if (! entry.is_regular_file() || (file.extension() != ".h" && file.extension() != ".cpp"))
            continue;

        std::ifstream source { file };
        const std::string text { std::istreambuf_iterator<char> { source },
                                 std::istreambuf_iterator<char> {} };

        if (text.find ("juce") == std::string::npos)
            continue;

        for (const auto& literal : stringLiterals (text))
            if (literal.nonAscii && ! crossesAsUtf8 (text, literal.quote))
                offenders.push_back (file.filename().string() + ":"
                                     + std::to_string (lineOf (text, literal.quote)));
    }

    std::string named;

    for (const auto& offender : offenders)
        named += offender + " ";

    INFO ("literals that cross as 8-bit text: " << named);
    REQUIRE (offenders.empty());
}
