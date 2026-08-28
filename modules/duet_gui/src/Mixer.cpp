#include <duet/gui/Mixer.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace duet::gui
{
namespace
{
    constexpr double peakHoldSeconds = 1.0;
    constexpr double meterFallDbPerSecond = 24.0;

    double faderValue (double db)
    {
        if (db <= Mixer::faderMinimumDb)
            return duet::model::silentDb;
        return std::clamp (db, Mixer::faderMinimumDb, Mixer::faderMaximumDb);
    }
} // namespace

void Mixer::setSession (duet::model::Session* openProject)
{
    cancelGesture();
    session = openProject;
    meters.clear();
    cachedRevision = UINT64_MAX;
    cachedStrips.clear();
    sampledChannels.clear();
}

void Mixer::setSuggestions (Suggestions* pendingSuggestions) { suggestions = pendingSuggestions; }

std::optional<GhostFaderDrawing> Mixer::ghostFader (duet::model::TrackRef channel) const
{
    if (suggestions == nullptr)
        return {};

    for (const auto& card : suggestions->cards())
        for (std::size_t element = 0; element < card.elements.size(); ++element)
            for (const auto& ghost : card.elements[element].faders)
                if (ghost.channel == channel)
                    return GhostFaderDrawing { ghost.db,
                                               suggestions->intensityOf (card.id, element),
                                               suggestions->isAuditioning (card.id) };

    return {};
}

AuditionChip Mixer::auditionChip (duet::model::TrackRef channel) const
{
    if (suggestions == nullptr)
        return {};

    const auto live = suggestions->auditioning();

    if (! live.has_value())
        return {};

    const auto* card = suggestions->card (*live);

    if (card == nullptr)
        return {};

    // Only the strips the Audition would change say anything: a chip on a strip
    // the Suggestion never mentions would be asking the producer to compare two
    // identical things.
    for (const auto& element : card->elements)
        for (const auto& ghost : element.faders)
            if (ghost.channel == channel)
                return { true, suggestions->hearingProposed() };

    return {};
}

std::vector<MixerStrip> Mixer::strips() const
{
    if (session == nullptr)
        return {};

    if (cachedRevision != session->revision())
    {
        cachedStrips.clear();
        const auto tracks = session->tracks();
        cachedStrips.reserve (tracks.size() + 1);
        for (const auto& track : tracks)
            cachedStrips.push_back ({ track.track,
                                      track.name,
                                      track.colour,
                                      track.kind,
                                      track.volumeDb,
                                      track.pan,
                                      duet::model::silentDb,
                                      track.muted,
                                      track.soloed,
                                      true,
                                      true,
                                      track.output,
                                      track.plugins });

        const auto master = session->master();
        cachedStrips.push_back ({ duet::model::masterChannel,
                                  "Master",
                                  std::nullopt,
                                  duet::model::TrackKind::group,
                                  master.volumeDb,
                                  master.pan,
                                  duet::model::silentDb,
                                  master.muted,
                                  false,
                                  false,
                                  false,
                                  duet::model::noTrack,
                                  master.plugins });
        cachedRevision = session->revision();
    }

    auto result = cachedStrips;
    for (auto& item : result)
    {
        if (const auto meter = meters.find (item.channel); meter != meters.end())
            item.meterDb = meter->second.displayedDb;
        if (gesture.channel == item.channel)
        {
            if (gesture.kind == GestureKind::fader)
                item.volumeDb = gesture.preview;
            else if (gesture.kind == GestureKind::pan)
                item.pan = gesture.preview;
        }
    }
    return result;
}

MixerStrip Mixer::strip (duet::model::TrackRef channel) const
{
    auto all = strips();
    const auto found = std::find_if (
        all.begin(), all.end(), [channel] (const auto& item) { return item.channel == channel; });
    return found != all.end() ? *found : MixerStrip {};
}

bool Mixer::hasChannel (duet::model::TrackRef channel) const
{
    return channel == duet::model::masterChannel
           || (session != nullptr && session->track (channel).track != duet::model::noTrack);
}

void Mixer::beginGesture (GestureKind kind, duet::model::TrackRef channel, double original)
{
    cancelGesture();
    if (session != nullptr && hasChannel (channel))
        gesture = { kind, channel, original, original };
}

void Mixer::beginFaderGesture (duet::model::TrackRef channel)
{
    beginGesture (GestureKind::fader, channel, strip (channel).volumeDb);
}

void Mixer::beginPanGesture (duet::model::TrackRef channel)
{
    beginGesture (GestureKind::pan, channel, strip (channel).pan);
}

void Mixer::previewGesture (GestureKind kind, duet::model::TrackRef channel, double value)
{
    if (session == nullptr || gesture.kind != kind || gesture.channel != channel)
        return;

    gesture.preview = value;
    if (kind == GestureKind::fader)
        session->previewVolumeDb (channel, value);
    else
        session->previewPan (channel, value);
}

void Mixer::dragFaderTo (duet::model::TrackRef channel, double db)
{
    previewGesture (GestureKind::fader, channel, faderValue (db));
}

void Mixer::dragPanTo (duet::model::TrackRef channel, double pan)
{
    previewGesture (GestureKind::pan, channel, std::clamp (pan, -1.0, 1.0));
}

void Mixer::endGesture (GestureKind kind, duet::model::TrackRef channel)
{
    if (session == nullptr || gesture.kind != kind || gesture.channel != channel)
        return;

    const auto completed = gesture;
    gesture = {};

    if (kind == GestureKind::fader)
        session->previewVolumeDb (channel, completed.original);
    else
        session->previewPan (channel, completed.original);

    if (std::abs (completed.preview - completed.original) < 0.000001)
        return;

    session->performAction (kind == GestureKind::fader ? "Set Track Fader" : "Set Track Pan",
                            [&] (auto& ops)
                            {
                                if (kind == GestureKind::fader)
                                    ops.setTrackVolumeDb (channel, completed.preview);
                                else
                                    ops.setTrackPan (channel, completed.preview);
                            });
}

void Mixer::endFaderGesture (duet::model::TrackRef channel)
{
    endGesture (GestureKind::fader, channel);
}

void Mixer::endPanGesture (duet::model::TrackRef channel)
{
    endGesture (GestureKind::pan, channel);
}

void Mixer::cancelGesture()
{
    if (session != nullptr)
    {
        if (gesture.kind == GestureKind::fader)
            session->previewVolumeDb (gesture.channel, gesture.original);
        else if (gesture.kind == GestureKind::pan)
            session->previewPan (gesture.channel, gesture.original);
    }
    gesture = {};
}

void Mixer::resetFader (duet::model::TrackRef channel)
{
    if (session == nullptr || ! hasChannel (channel)
        || std::abs (strip (channel).volumeDb) < 0.000001)
        return;
    session->performAction ("Reset Track Fader",
                            [&] (auto& ops) { ops.setTrackVolumeDb (channel, 0.0); });
}

void Mixer::resetPan (duet::model::TrackRef channel)
{
    if (session == nullptr || ! hasChannel (channel) || std::abs (strip (channel).pan) < 0.000001)
        return;
    session->performAction ("Centre Track Pan",
                            [&] (auto& ops) { ops.setTrackPan (channel, 0.0); });
}

void Mixer::toggleMute (duet::model::TrackRef channel)
{
    if (session == nullptr || ! hasChannel (channel))
        return;
    const auto muted = strip (channel).muted;
    session->performAction (muted ? "Unmute Track" : "Mute Track",
                            [&] (auto& ops) { ops.setTrackMute (channel, ! muted); });
}

void Mixer::toggleSolo (duet::model::TrackRef track)
{
    if (session == nullptr || track == duet::model::masterChannel)
        return;
    const auto soloed = session->track (track).soloed;
    session->performAction (soloed ? "Unsolo Track" : "Solo Track",
                            [&] (auto& ops) { ops.setTrackSolo (track, ! soloed); });
}

std::vector<RoutingDestination> Mixer::routingDestinations (duet::model::TrackRef channel) const
{
    std::vector<RoutingDestination> result { { duet::model::noTrack, "Main Output" } };
    if (session == nullptr || channel == duet::model::masterChannel)
        return result;

    const auto tracks = session->tracks();
    for (const auto& candidate : tracks)
    {
        if (candidate.kind != duet::model::TrackKind::group || candidate.track == channel)
            continue;

        auto destination = candidate.output;
        bool cycles = false;
        while (destination != duet::model::noTrack)
        {
            if (destination == channel)
            {
                cycles = true;
                break;
            }
            destination = session->track (destination).output;
        }
        if (! cycles)
            result.push_back ({ candidate.track, candidate.name });
    }
    return result;
}

void Mixer::setOutput (duet::model::TrackRef track, duet::model::TrackRef destination)
{
    if (session == nullptr || track == duet::model::masterChannel
        || session->track (track).output == destination)
        return;

    const auto choices = routingDestinations (track);
    if (std::none_of (choices.begin(),
                      choices.end(),
                      [destination] (const auto& choice) { return choice.channel == destination; }))
        return;

    session->performAction ("Set Track Output",
                            [&] (auto& ops) { ops.setTrackOutput (track, destination); });
}

duet::model::PluginRef Mixer::addBuiltin (duet::model::TrackRef channel,
                                          duet::model::BuiltinPlugin plugin,
                                          int position)
{
    if (session == nullptr || ! hasChannel (channel))
        return duet::model::noPlugin;
    const auto instrument = plugin == duet::model::BuiltinPlugin::synth
                            || plugin == duet::model::BuiltinPlugin::sampler;
    if (instrument
        && (channel == duet::model::masterChannel
            || session->track (channel).kind != duet::model::TrackKind::midi))
        return duet::model::noPlugin;
    duet::model::PluginRef result = duet::model::noPlugin;
    session->performAction (
        "Insert Plugin", [&] (auto& ops) { result = ops.addPlugin (channel, plugin, position); });
    return result;
}

std::vector<duet::model::KnownPluginInfo>
    Mixer::availableVst3For (duet::model::TrackRef channel) const
{
    if (session == nullptr || ! hasChannel (channel))
        return {};
    auto known = session->knownVst3Plugins();
    const auto acceptsInstrument = channel != duet::model::masterChannel
                                   && session->track (channel).kind == duet::model::TrackKind::midi;
    std::erase_if (known,
                   [acceptsInstrument] (const auto& plugin) {
                       return ! plugin.isAvailable || (plugin.isInstrument && ! acceptsInstrument);
                   });
    return known;
}

duet::model::PluginRef
    Mixer::addVst3 (duet::model::TrackRef channel, std::string_view identifier, int position)
{
    if (session == nullptr || ! hasChannel (channel))
        return duet::model::noPlugin;
    const auto known = session->knownVst3Plugins();
    const auto found =
        std::ranges::find (known, identifier, &duet::model::KnownPluginInfo::identifier);
    if (found == known.end()
        || (found->isInstrument
            && (channel == duet::model::masterChannel
                || session->track (channel).kind != duet::model::TrackKind::midi)))
        return duet::model::noPlugin;
    duet::model::PluginRef result = duet::model::noPlugin;
    session->performAction ("Insert Plugin",
                            [&] (auto& ops)
                            { result = ops.addPlugin (channel, identifier, position); });
    return result;
}

void Mixer::removePlugin (duet::model::PluginRef plugin)
{
    if (session != nullptr && pluginInfo (plugin).has_value())
        session->performAction ("Remove Plugin", [&] (auto& ops) { ops.removePlugin (plugin); });
}

void Mixer::reorderPlugin (duet::model::PluginRef plugin, int position)
{
    if (session == nullptr)
        return;
    for (const auto& channel : strips())
    {
        const auto found =
            std::ranges::find (channel.plugins, plugin, &duet::model::PluginInfo::plugin);
        if (found == channel.plugins.end())
            continue;
        const auto current = static_cast<int> (std::distance (channel.plugins.begin(), found));
        const auto clamped =
            std::clamp (position, 0, static_cast<int> (channel.plugins.size()) - 1);
        if (current == clamped)
            return;
        session->performAction ("Reorder Plugin",
                                [&] (auto& ops) { ops.reorderPlugin (plugin, clamped); });
        return;
    }
}

std::optional<duet::model::PluginInfo> Mixer::pluginInfo (duet::model::PluginRef plugin) const
{
    if (session == nullptr)
        return std::nullopt;
    for (const auto& item : strips())
        for (const auto& candidate : item.plugins)
            if (candidate.plugin == plugin)
                return candidate;
    return std::nullopt;
}

void Mixer::toggleBypass (duet::model::PluginRef plugin)
{
    if (session == nullptr)
        return;
    const auto info = pluginInfo (plugin);
    if (! info.has_value())
        return;
    session->performAction (info->bypassed ? "Enable Plugin" : "Bypass Plugin",
                            [&] (auto& ops) { ops.setPluginBypassed (plugin, ! info->bypassed); });
}

void Mixer::setVisibleRange (std::size_t firstStrip, std::size_t stripCount)
{
    visibleFirst = firstStrip;
    visibleCount = stripCount;
}

void Mixer::observePeak (duet::model::TrackRef channel, double peakDb, double nowSeconds)
{
    auto& state = meters[channel];
    if (peakDb > state.displayedDb)
    {
        state.displayedDb = peakDb;
        state.peakAtSeconds = nowSeconds;
    }
    state.advancedToSeconds = nowSeconds;
}

std::size_t Mixer::sampleMeters (double nowSeconds)
{
    sampledChannels.clear();
    if (session == nullptr)
        return 0;

    const auto all = strips();
    const auto end = std::min (all.size(), visibleFirst + visibleCount);
    for (auto index = visibleFirst; index < end; ++index)
    {
        const auto channel = all[index].channel;
        sampledChannels.push_back (channel);
        const auto peak = channel == duet::model::masterChannel ? session->outputPeakDb()
                                                                : session->trackPeakDb (channel);
        auto& state = meters[channel];
        const auto previouslyAdvancedTo = state.advancedToSeconds;
        if (peak > state.displayedDb)
        {
            state.displayedDb = peak;
            state.peakAtSeconds = nowSeconds;
        }
        const auto fallFrom =
            std::max (previouslyAdvancedTo, state.peakAtSeconds + peakHoldSeconds);
        if (nowSeconds > fallFrom)
            state.displayedDb =
                std::max (duet::model::silentDb,
                          state.displayedDb - meterFallDbPerSecond * (nowSeconds - fallFrom));
        state.advancedToSeconds = nowSeconds;
    }
    return sampledChannels.size();
}

const std::vector<duet::model::TrackRef>& Mixer::lastSampledChannels() const
{
    return sampledChannels;
}

void Mixer::observeMeterPeakForTesting (duet::model::TrackRef channel,
                                        double peakDb,
                                        double nowSeconds)
{
    observePeak (channel, peakDb, nowSeconds);
}
} // namespace duet::gui
