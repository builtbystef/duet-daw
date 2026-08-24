#pragma once

#include <duet/persistence/Project.h>

namespace duet::gui
{
class Settings;

/** The autosave interval stored once for the application, whichever project is
    open. A missing setting is the product default of ten minutes.
*/
[[nodiscard]] duet::persistence::AutosaveInterval autosaveInterval (const Settings& settings);

/** Stores the interval selected by the producer. */
void setAutosaveInterval (Settings& settings, duet::persistence::AutosaveInterval interval);
} // namespace duet::gui
