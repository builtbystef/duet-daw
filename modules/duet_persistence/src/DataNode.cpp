#include <duet/persistence/DataNode.h>

#include <array>
#include <charconv>
#include <iterator>
#include <system_error>
#include <utility>

namespace duet::persistence
{
namespace
{
    /** A number as text that reads back as the same number.

        `std::to_string` writes a double at six decimal places, which is a
        rounding, and a zoom or a scroll position that came back rounded would
        put the timeline somewhere the producer did not leave it. The shortest
        round-trip form is what `to_chars` writes.
    */
    template <typename Number>
    std::string toText (Number value)
    {
        // Long enough for every double the shortest round-trip form can need.
        std::array<char, 32> digits {};
        const auto written = std::to_chars (digits.data(), std::to_address (digits.end()), value);

        return std::string { digits.data(), written.ptr };
    }

    template <typename Number>
    Number fromText (const std::string& text, Number fallback)
    {
        Number value {};
        const auto* const end = std::to_address (text.end());
        const auto read = std::from_chars (text.data(), end, value);

        // A partial parse is a broken value, not a number with something after
        // it: what is on the node was not written by this code.
        return read.ec == std::errc {} && read.ptr == end ? value : fallback;
    }
} // namespace

DataNode::DataNode (std::string nodeType) : name (std::move (nodeType)) {}

void DataNode::set (std::string_view key, std::string_view value)
{
    attributes.insert_or_assign (std::string { key }, std::string { value });
}

void DataNode::set (std::string_view key, const char* value)
{
    set (key, std::string_view { value });
}

void DataNode::set (std::string_view key, bool value)
{
    set (key, value ? std::string_view { "1" } : std::string_view { "0" });
}

void DataNode::set (std::string_view key, int value) { set (key, toText (value)); }

void DataNode::set (std::string_view key, double value) { set (key, toText (value)); }

void DataNode::set (std::string_view key, std::uint64_t value) { set (key, toText (value)); }

bool DataNode::has (std::string_view key) const
{
    return attributes.find (key) != attributes.end();
}

std::string DataNode::stringValue (std::string_view key, std::string_view fallback) const
{
    const auto found = attributes.find (key);

    return found == attributes.end() ? std::string { fallback } : found->second;
}

bool DataNode::boolValue (std::string_view key, bool fallback) const
{
    const auto found = attributes.find (key);

    return found == attributes.end() ? fallback : found->second != "0";
}

int DataNode::intValue (std::string_view key, int fallback) const
{
    const auto found = attributes.find (key);

    return found == attributes.end() ? fallback : fromText (found->second, fallback);
}

double DataNode::doubleValue (std::string_view key, double fallback) const
{
    const auto found = attributes.find (key);

    return found == attributes.end() ? fallback : fromText (found->second, fallback);
}

std::uint64_t DataNode::uint64Value (std::string_view key, std::uint64_t fallback) const
{
    const auto found = attributes.find (key);

    return found == attributes.end() ? fallback : fromText (found->second, fallback);
}

void DataNode::add (DataNode child) { childNodes.push_back (std::move (child)); }
} // namespace duet::persistence
