#include "RealtimeProbes.h"

#include <duet/model/EngineAccess.h>
#include <duet/realtime/Callback.h>

#include <utility>

namespace te = tracktion::engine;

namespace
{
/** The engine-native probe: a plugin in a track's chain, entered on the audio
    thread at `applyToBuffer`.

    Everything a Plugin must answer is answered here as briefly as the base
    class allows, because none of it is what the probe is for. What it is for is
    the callback: bounded work over a buffer the caller owns, no allocation, no
    lock, nothing that reaches the operating system — the Real-time audio
    standard, in the smallest form that still processes audio.
*/
class NativeRealtimeProbe final : public te::Plugin
{
public:
    explicit NativeRealtimeProbe (te::PluginCreationInfo info) : Plugin (std::move (info)) {}

    ~NativeRealtimeProbe() override { notifyListenersOfDeletion(); }

    static const char* getPluginName() { return "Duet Realtime Probe"; }
    static const char* xmlTypeName;

    [[nodiscard]] juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }
    [[nodiscard]] BusLayout getBusses() const override { return BusLayout::singleStereoInOut(); }

    void initialise (const te::PluginInitialisationInfo& /*info*/) override {}
    void deinitialise() override {}
    void restorePluginStateFromValueTree (const juce::ValueTree& /*state*/) override {}

    /** The callback. `noexcept` and annotated, which is where RealtimeSanitizer
        starts watching: the instrumentation is in this function, so every frame
        below it is under the rule however uninstrumented the engine's call into
        it was.
    */
    void applyToBuffer (const te::PluginRenderContext& context) noexcept DUET_NONBLOCKING override
    {
        if (context.destBuffer == nullptr)
            return;

        // The buffer's own gain over the samples the caller named: a multiply
        // that takes nothing and allocates nothing.
        context.destBuffer->applyGain (context.bufferStartSample,
                                       context.bufferNumSamples,
                                       static_cast<float> (duet::testing::realtimeProbeGain));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativeRealtimeProbe)
};

const char* NativeRealtimeProbe::xmlTypeName = "duetRealtimeProbe";
} // namespace

namespace duet::testing
{
void addNativeRealtimeProbe (duet::model::Session& session, duet::model::TrackRef track)
{
    auto& edit = duet::model::EngineAccess::editOf (session);

    // Registering the same type twice is what the engine ignores, so a second
    // probe on a second track costs nothing and needs no bookkeeping here.
    edit.engine.getPluginManager().createBuiltInType<NativeRealtimeProbe>();

    auto* audioTrack = dynamic_cast<te::AudioTrack*> (
        te::findTrackForID (edit, te::EditItemID::fromRawID (static_cast<juce::uint64> (track))));

    if (audioTrack == nullptr)
        return;

    auto created = edit.getPluginCache().createNewPlugin (NativeRealtimeProbe::xmlTypeName, {});

    if (created == nullptr)
        return;

    // The head of the chain, written straight onto the track and with no undo
    // manager: the probe is the test's, not the producer's, so it may not appear
    // in the project's history at all, and a plugin added outside an Action
    // would be a step no producer made. PluginList::insertPlugin cannot do
    // that — it always writes through the Edit's own UndoManager, and the
    // argument that reads like one is a SelectionManager.
    auto& chain = audioTrack->state;
    int head = chain.getNumChildren();

    for (int index = 0; index < chain.getNumChildren(); ++index)
        if (chain.getChild (index).hasType (te::IDs::PLUGIN))
        {
            head = index;
            break;
        }

    chain.addChild (created->state, head, nullptr);
}
} // namespace duet::testing
