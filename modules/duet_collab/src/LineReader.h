#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace duet::collab
{
/** The framing: a stream of bytes in, whole newline-terminated messages out.

    A socket read has nothing to do with a message boundary — one read can carry
    two messages, or half of one — so the only thing that ends a message is the
    newline that terminates it. Bytes that do not reach one are held until the
    read that completes them. A newline inside a JSON string is escaped by JSON
    itself and never appears here as a byte, which is what makes the newline
    usable as the terminator.
*/
class LineReader
{
public:
    /** Adds bytes, and returns the messages they completed — none, when the
        bytes did not reach a newline.
    */
    [[nodiscard]] std::vector<std::string> consume (std::string_view bytes);

    /** Forgets the partial message, for a connection that has gone. */
    void reset();

private:
    std::string partial;
};
} // namespace duet::collab
