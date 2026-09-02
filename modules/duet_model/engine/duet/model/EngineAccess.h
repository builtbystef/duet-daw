#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace duet::model
{
class Session;

/** The narrow ways through the engine seam.

    Persistence reaches the Edit to snapshot the whole project. The component
    half of duet_gui reaches a hosted AudioProcessor to place its native editor
    in Duet's plugin window (ADR 0008). Paintless GUI code and every edit still
    use the engine-free vocabulary.

    This header is not on duet_model's public include path — only an explicitly
    linked engine-access target sees it.
*/
struct EngineAccess
{
    /** The Edit a session is editing. */
    [[nodiscard]] static tracktion::engine::Edit& editOf (Session& session);

    /** The machine's audio device manager, so Source audition can mix into the
        open device after the engine — heard at Main Output, never an input.
    */
    [[nodiscard]] static juce::AudioDeviceManager& audioDevicesOf (Session& session);
};
} // namespace duet::model
