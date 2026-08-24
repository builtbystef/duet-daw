#include <duet/gui/Snap.h>

#include <cmath>

namespace duet::gui
{
double snapBeats (double beats, GridSpec grid, bool altHeld)
{
    if (altHeld || grid.subdivisionBeats <= 0.0)
        return beats;

    return std::round (beats / grid.subdivisionBeats) * grid.subdivisionBeats;
}
} // namespace duet::gui
