#pragma once

#include <duet/gui/TimelineGeometry.h>

namespace duet::gui
{
/** Snaps to the visible grid, unless Alt is held at the moment of the read. */
[[nodiscard]] double snapBeats (double beats, GridSpec grid, bool altHeld);
} // namespace duet::gui
