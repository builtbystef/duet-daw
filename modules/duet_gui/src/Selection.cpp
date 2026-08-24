#include <duet/gui/Selection.h>

#include <algorithm>

namespace duet::gui
{
void Selection::focus (SelectionKind kind)
{
    if (focused != kind)
    {
        focused = kind;
        clear();
    }
}

void Selection::click (SelectedItem item,
                       const std::vector<SelectedItem>& surfaceOrder,
                       bool ctrlHeld,
                       bool shiftHeld)
{
    focus (item.kind);

    if (shiftHeld && hasAnchor)
    {
        const auto first = std::find (surfaceOrder.begin(), surfaceOrder.end(), anchor);
        const auto last = std::find (surfaceOrder.begin(), surfaceOrder.end(), item);

        if (first != surfaceOrder.end() && last != surfaceOrder.end())
        {
            const auto from = std::min (first, last);
            const auto through = std::max (first, last);
            selected.assign (from, through + 1);
            return;
        }
    }

    if (ctrlHeld)
    {
        const auto found = std::find (selected.begin(), selected.end(), item);
        if (found == selected.end())
            selected.push_back (item);
        else
            selected.erase (found);
        sortLike (surfaceOrder);

        if (! selected.empty())
            anchor = selected.back();
        else
            anchor = item;
    }
    else
    {
        selected = { item };
        anchor = item;
    }

    hasAnchor = true;
}

void Selection::rubberBand (const std::vector<SelectedItem>& intersected, bool ctrlHeld)
{
    if (! intersected.empty())
        focus (intersected.front().kind);

    if (! ctrlHeld)
        selected.clear();

    for (const auto item : intersected)
        if (! contains (item))
            selected.push_back (item);

    sortLike (intersected);
}

void Selection::selectAll (const std::vector<SelectedItem>& focusedSurfaceItems)
{
    selected.clear();
    for (const auto item : focusedSurfaceItems)
        if (item.kind == focused)
            selected.push_back (item);

    if (! selected.empty())
    {
        anchor = selected.front();
        hasAnchor = true;
    }
}

void Selection::clear()
{
    selected.clear();
    hasAnchor = false;
}

bool Selection::contains (SelectedItem item) const
{
    return std::find (selected.begin(), selected.end(), item) != selected.end();
}

void Selection::sortLike (const std::vector<SelectedItem>& surfaceOrder)
{
    std::stable_sort (selected.begin(),
                      selected.end(),
                      [&surfaceOrder] (SelectedItem left, SelectedItem right)
                      {
                          return std::find (surfaceOrder.begin(), surfaceOrder.end(), left)
                                 < std::find (surfaceOrder.begin(), surfaceOrder.end(), right);
                      });
}
} // namespace duet::gui
