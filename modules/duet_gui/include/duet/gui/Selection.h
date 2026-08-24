#pragma once

#include <cstdint>
#include <vector>

namespace duet::gui
{
/** Which focused surface an item in the one current selection belongs to. */
enum class SelectionKind : std::uint8_t
{
    clip,
    note
};

/** An engine-free identity in the current selection. */
struct SelectedItem
{
    SelectionKind kind = SelectionKind::clip;
    std::uint64_t ref = 0;

    friend constexpr bool operator== (const SelectedItem&, const SelectedItem&) = default;
};

/** The one current selection shared by the interface's editing surfaces. */
class Selection
{
public:
    void focus (SelectionKind kind);
    [[nodiscard]] SelectionKind focusedKind() const { return focused; }

    void click (SelectedItem item,
                const std::vector<SelectedItem>& surfaceOrder,
                bool ctrlHeld,
                bool shiftHeld);
    void rubberBand (const std::vector<SelectedItem>& intersected, bool ctrlHeld);
    void selectAll (const std::vector<SelectedItem>& focusedSurfaceItems);
    void clear();

    [[nodiscard]] bool contains (SelectedItem item) const;
    [[nodiscard]] bool empty() const { return selected.empty(); }
    [[nodiscard]] const std::vector<SelectedItem>& items() const { return selected; }

private:
    void sortLike (const std::vector<SelectedItem>& surfaceOrder);

    SelectionKind focused = SelectionKind::clip;
    std::vector<SelectedItem> selected;
    SelectedItem anchor;
    bool hasAnchor = false;
};
} // namespace duet::gui
