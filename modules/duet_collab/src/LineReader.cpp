#include "LineReader.h"

namespace duet::collab
{
std::vector<std::string> LineReader::consume (std::string_view bytes)
{
    std::vector<std::string> messages;
    partial.append (bytes);

    std::string::size_type start = 0;

    for (auto newline = partial.find ('\n'); newline != std::string::npos;
         newline = partial.find ('\n', start))
    {
        auto length = newline - start;

        // A peer that writes CRLF is framing the same messages; the carriage
        // return is not part of one.
        if (length > 0 && partial[start + length - 1] == '\r')
            --length;

        messages.emplace_back (partial, start, length);
        start = newline + 1;
    }

    partial.erase (0, start);

    return messages;
}

void LineReader::reset() { partial.clear(); }
} // namespace duet::collab
