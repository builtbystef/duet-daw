#pragma once

#include <duet/model/Session.h>

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace duet::gui
{
struct MixerStrip
{
    duet::model::TrackRef channel = duet::model::noTrack;
    std::string name;
    std::optional<duet::model::TrackColour> colour;
    duet::model::TrackKind kind = duet::model::TrackKind::audio;
    double volumeDb = 0.0;
    double pan = 0.0;
    double meterDb = duet::model::silentDb;
    bool muted = false;
    bool soloed = false;
    bool canSolo = true;
    bool canRoute = true;
    duet::model::TrackRef output = duet::model::noTrack;
    std::vector<duet::model::PluginInfo> plugins;
};

struct RoutingDestination
{
    duet::model::TrackRef channel = duet::model::noTrack;
    std::string name;
};

/** The paintless Mixer seam. Gesture previews are audible but do not enter the
    project; completion restores the preview and commits its final value as one
    Action. Meter time is supplied by the caller, making hold and decay fully
    deterministic in tests.
*/
class Mixer
{
public:
    void setSession (duet::model::Session* openProject);

    [[nodiscard]] std::vector<MixerStrip> strips() const;
    [[nodiscard]] MixerStrip strip (duet::model::TrackRef channel) const;

    void beginFaderGesture (duet::model::TrackRef channel);
    void dragFaderTo (duet::model::TrackRef channel, double db);
    void endFaderGesture (duet::model::TrackRef channel);
    void resetFader (duet::model::TrackRef channel);

    void beginPanGesture (duet::model::TrackRef channel);
    void dragPanTo (duet::model::TrackRef channel, double pan);
    void endPanGesture (duet::model::TrackRef channel);
    void resetPan (duet::model::TrackRef channel);
    void cancelGesture();

    void toggleMute (duet::model::TrackRef channel);
    void toggleSolo (duet::model::TrackRef track);

    [[nodiscard]] std::vector<RoutingDestination>
        routingDestinations (duet::model::TrackRef channel) const;
    void setOutput (duet::model::TrackRef track, duet::model::TrackRef destination);

    duet::model::PluginRef
        addBuiltin (duet::model::TrackRef channel, duet::model::BuiltinPlugin plugin, int position);
    [[nodiscard]] std::vector<duet::model::KnownPluginInfo>
        availableVst3For (duet::model::TrackRef channel) const;
    duet::model::PluginRef
        addVst3 (duet::model::TrackRef channel, std::string_view identifier, int position);
    void removePlugin (duet::model::PluginRef plugin);
    void reorderPlugin (duet::model::PluginRef plugin, int position);
    void toggleBypass (duet::model::PluginRef plugin);

    /** Limits 30 Hz sampling and repaint work to this strip interval. */
    void setVisibleRange (std::size_t firstStrip, std::size_t stripCount);

    /** Samples visible channels and advances their held display values. Returns
        the number of model meters read.
    */
    std::size_t sampleMeters (double nowSeconds);
    [[nodiscard]] const std::vector<duet::model::TrackRef>& lastSampledChannels() const;

    /** A deterministic seam for a measured peak; production supplies the same
        observation through Session's published meter reads.
    */
    void observeMeterPeakForTesting (duet::model::TrackRef channel,
                                     double peakDb,
                                     double nowSeconds);

    /** The fader's travel, which is the model's, because what a fader may be
        set to is a fact about the value and not about the strip drawing it. */
    static constexpr double faderMinimumDb = duet::model::faderMinimumDb;
    static constexpr double faderMaximumDb = duet::model::faderMaximumDb;

private:
    enum class GestureKind : std::uint8_t
    {
        none,
        fader,
        pan
    };

    struct Gesture
    {
        GestureKind kind = GestureKind::none;
        duet::model::TrackRef channel = duet::model::noTrack;
        double original = 0.0;
        double preview = 0.0;
    };

    struct MeterState
    {
        double displayedDb = duet::model::silentDb;
        double peakAtSeconds = 0.0;
        double advancedToSeconds = 0.0;
    };

    [[nodiscard]] bool hasChannel (duet::model::TrackRef channel) const;
    [[nodiscard]] std::optional<duet::model::PluginInfo>
        pluginInfo (duet::model::PluginRef plugin) const;
    void beginGesture (GestureKind kind, duet::model::TrackRef channel, double original);
    void previewGesture (GestureKind kind, duet::model::TrackRef channel, double value);
    void endGesture (GestureKind kind, duet::model::TrackRef channel);
    void observePeak (duet::model::TrackRef channel, double peakDb, double nowSeconds);

    duet::model::Session* session = nullptr;
    Gesture gesture;
    std::unordered_map<duet::model::TrackRef, MeterState> meters;
    mutable std::uint64_t cachedRevision = UINT64_MAX;
    mutable std::vector<MixerStrip> cachedStrips;
    std::size_t visibleFirst = 0;
    std::size_t visibleCount = static_cast<std::size_t> (-1);
    std::vector<duet::model::TrackRef> sampledChannels;
};
} // namespace duet::gui
