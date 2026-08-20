#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace duet::persistence
{
/** A node of Duet's own project data: a named node with named values on it and
    named nodes under it.

    This is the shape the DUET tree stores (ADR 0005) with none of the engine's
    types in it, so that what writes a node and what reads one need no engine and
    no JUCE. The interface's per-project view state travels through here: the
    view-model builds one of these, and this module is the only place that knows
    it becomes a `juce::ValueTree`.

    Every value is stored as text, because the project file stores text. The
    typed accessors are what a caller uses instead of parsing, and each takes the
    value to fall back to — which is what a node written by an older Duet, or by
    none at all, gives back.
*/
class DataNode
{
public:
    explicit DataNode (std::string nodeType);

    /** The node's name — "VIEW" for the interface's view state. */
    [[nodiscard]] const std::string& type() const { return name; }

    void set (std::string_view key, std::string_view value);

    /** Text written as a literal. Without this a literal is a `const char*`,
        which converts to `bool` before it converts to a `std::string_view`, and
        the name of a thing would be stored as the number one.
    */
    void set (std::string_view key, const char* value);

    void set (std::string_view key, bool value);
    void set (std::string_view key, int value);
    void set (std::string_view key, double value);
    void set (std::string_view key, std::uint64_t value);

    [[nodiscard]] bool has (std::string_view key) const;

    [[nodiscard]] std::string stringValue (std::string_view key,
                                           std::string_view fallback = {}) const;
    [[nodiscard]] bool boolValue (std::string_view key, bool fallback) const;
    [[nodiscard]] int intValue (std::string_view key, int fallback) const;
    [[nodiscard]] double doubleValue (std::string_view key, double fallback) const;
    [[nodiscard]] std::uint64_t uint64Value (std::string_view key, std::uint64_t fallback) const;

    void add (DataNode child);

    /** The values on this node, in the order their names sort. A node's text is
        what a save writes, so the order has to be the node's own and not the
        order a caller happened to set them in.
    */
    [[nodiscard]] const std::map<std::string, std::string, std::less<>>& values() const
    {
        return attributes;
    }

    /** The nodes under this one, in the order they were added. */
    [[nodiscard]] const std::vector<DataNode>& children() const { return childNodes; }

private:
    std::string name;
    std::map<std::string, std::string, std::less<>> attributes;
    std::vector<DataNode> childNodes;
};
} // namespace duet::persistence
