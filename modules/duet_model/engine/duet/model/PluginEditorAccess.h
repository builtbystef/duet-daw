#pragma once

#include <duet/model/Session.h>

namespace juce
{
class AudioProcessor;
}

namespace duet::model
{
/** Component-only access to a hosted processor for ADR 0008. */
struct PluginEditorAccess
{
    [[nodiscard]] static juce::AudioProcessor* processorOf (Session& session, PluginRef plugin);
};
} // namespace duet::model
