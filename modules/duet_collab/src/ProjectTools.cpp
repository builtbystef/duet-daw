#include <duet/collab/ProjectTools.h>

#include "AnalysisCall.h"

#include <duet/collab/Estimate.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <utility>

namespace duet::collab
{
namespace toolId
{
    namespace
    {
        constexpr std::string_view trackPrefix = "track-";
        constexpr std::string_view clipPrefix = "clip-";
        constexpr std::string_view pluginPrefix = "plugin-";
        constexpr std::string_view notePrefix = "note-";
        constexpr std::string_view masterName = "master";

        std::string with (std::string_view prefix, std::uint64_t ref)
        {
            return std::string { prefix } + std::to_string (ref);
        }

        std::optional<std::uint64_t> refIn (std::string_view id, std::string_view prefix)
        {
            if (! id.starts_with (prefix))
                return {};

            const auto digits = id.substr (prefix.size());
            std::uint64_t ref = 0;
            const auto* end = digits.data() + digits.size();
            const auto parsed = std::from_chars (digits.data(), end, ref);

            if (parsed.ec != std::errc {} || parsed.ptr != end)
                return {};

            return ref;
        }
    } // namespace

    std::string forTrack (model::TrackRef track)
    {
        if (track == model::masterChannel)
            return std::string { trackPrefix } + std::string { masterName };

        return with (trackPrefix, track);
    }

    std::string forClip (model::ClipRef clip) { return with (clipPrefix, clip); }

    std::string forPlugin (model::PluginRef plugin) { return with (pluginPrefix, plugin); }

    std::string forNote (model::NoteRef note) { return with (notePrefix, note); }

    std::optional<model::TrackRef> toTrack (std::string_view id)
    {
        if (id == std::string { trackPrefix } + std::string { masterName })
            return model::masterChannel;

        return refIn (id, trackPrefix);
    }

    std::optional<model::ClipRef> toClip (std::string_view id) { return refIn (id, clipPrefix); }

    std::optional<model::PluginRef> toPlugin (std::string_view id)
    {
        return refIn (id, pluginPrefix);
    }

    std::optional<model::NoteRef> toNote (std::string_view id) { return refIn (id, notePrefix); }
} // namespace toolId

namespace
{
    using model::Session;

    /** The rate a plugin's latency is stated in samples at.

        The project has no sample rate of its own — a rate belongs to the device
        or the render a project is played through — and a tool result that
        changed when an audio device opened would be a cache buster for a fact
        that did not move. So the contract's samples are counted at the one rate
        the model states its own numbers at, and every milestone-one built-in
        reports no latency at all whatever the rate is.
    */
    constexpr double latencySampleRate = 44100.0;

    RpcOutcome noSuchThing (std::string_view kind, const std::string& id)
    {
        return RpcOutcome::failure (rpcError::invalidParams,
                                    "this project has no " + std::string { kind } + " called "
                                        + id);
    }

    RpcOutcome missingArgument (std::string_view name)
    {
        return RpcOutcome::failure (rpcError::invalidParams,
                                    "this tool needs a " + std::string { name });
    }

    /** One of a call's arguments, and empty when it is not there or is not the
        text an id is. A number where an id belongs is a tool call this project
        cannot answer, and it goes down the same path as one naming a track that
        is not there.
    */
    std::string argument (const ToolCall& call, std::string_view name)
    {
        const auto found = call.arguments.find (name);

        if (found == call.arguments.end() || ! found->is_string())
            return {};

        return found->get<std::string>();
    }

    const char* nameOf (model::TrackKind kind)
    {
        switch (kind)
        {
            case model::TrackKind::audio:
                return "audio";
            case model::TrackKind::midi:
                return "midi";
            case model::TrackKind::group:
                return "group";
        }

        return "audio";
    }

    //==============================================================================
    /** A track as the Tool Vocabulary sees it: an arrangement track or the
        master, since buses are tracks and the master is one of them.

        The model keeps the master apart from the arrangement — it is not a
        track anything can be placed on — and the vocabulary deliberately does
        not, so this is where the two shapes become one.
    */
    struct ToolTrack
    {
        model::TrackRef ref = model::noTrack;
        std::string name;
        model::TrackKind kind = model::TrackKind::audio;
        bool isMaster = false;

        model::TrackRef output = model::masterChannel;
        double volumeDb = 0.0;
        double pan = 0.0;
        bool muted = false;
        bool soloed = false;

        std::vector<model::SendInfo> sends;
        std::vector<model::PluginInfo> plugins;
        std::vector<model::ClipInfo> clips;

        [[nodiscard]] const char* kindName() const { return isMaster ? "master" : nameOf (kind); }
    };

    ToolTrack toolTrackOf (const model::TrackInfo& track)
    {
        ToolTrack out;
        out.ref = track.track;
        out.name = track.name;
        out.kind = track.kind;

        // A track with no destination of its own ends up at the master, so the
        // vocabulary says the master rather than saying nothing.
        out.output = track.output == model::noTrack ? model::masterChannel : track.output;
        out.volumeDb = track.volumeDb;
        out.pan = track.pan;
        out.muted = track.muted;
        out.soloed = track.soloed;
        out.sends = track.sends;
        out.plugins = track.plugins;
        out.clips = track.clips;

        return out;
    }

    ToolTrack toolTrackOf (const model::MasterInfo& master)
    {
        ToolTrack out;
        out.ref = model::masterChannel;
        out.name = "Master";
        out.isMaster = true;
        out.volumeDb = master.volumeDb;
        out.pan = master.pan;
        out.muted = master.muted;
        out.plugins = master.plugins;

        return out;
    }

    /** Every track the vocabulary knows, the master last: reading order follows
        the signal, and a track added to the arrangement leaves the prefix of the
        list it was added to alone.
    */
    std::vector<ToolTrack> toolTracksOf (const Session& session)
    {
        std::vector<ToolTrack> out;

        for (const auto& track : session.tracks())
            out.push_back (toolTrackOf (track));

        out.push_back (toolTrackOf (session.master()));

        return out;
    }

    std::optional<ToolTrack> toolTrackFor (const Session& session, model::TrackRef ref)
    {
        if (ref == model::masterChannel)
            return toolTrackOf (session.master());

        const auto track = session.track (ref);

        if (track.track == model::noTrack)
            return {};

        return toolTrackOf (track);
    }

    //==============================================================================
    /** Every curve this track actually has, in the order a lane list reads best:
        the fader, then the pan, then each plugin's parameters in chain order.

        A curve exists when it has points on it, which is also what makes it
        worth telling the Collaborator about: a parameter every plugin owns and
        nobody has drawn is not automation.
    */
    std::vector<model::AutomationTarget> automatedTargetsOf (const Session& session,
                                                             const ToolTrack& track)
    {
        std::vector<model::AutomationTarget> out;

        const auto keepIfDrawn = [&] (const model::AutomationTarget& target)
        {
            if (! session.automationPoints (target).empty())
                out.push_back (target);
        };

        keepIfDrawn (model::AutomationTarget::trackVolumeOf (track.ref));
        keepIfDrawn (model::AutomationTarget::trackPanOf (track.ref));

        for (const auto& plugin : track.plugins)
            for (const auto& parameter : session.pluginParameters (plugin.plugin))
                keepIfDrawn (
                    model::AutomationTarget::parameterOf (plugin.plugin, parameter.parameterId));

        return out;
    }

    /** What a curve is called in a track list: the mixer's own words for its
        two, and "plugin: parameter" for everything else.
    */
    std::string nameOfTarget (const Session& session,
                              const ToolTrack& track,
                              const model::AutomationTarget& target)
    {
        if (target.kind == model::AutomationTarget::Kind::trackVolume)
            return "volume";

        if (target.kind == model::AutomationTarget::Kind::trackPan)
            return "pan";

        std::string pluginName;

        for (const auto& plugin : track.plugins)
            if (plugin.plugin == target.plugin)
                pluginName = plugin.name;

        for (const auto& parameter : session.pluginParameters (target.plugin))
            if (parameter.parameterId == target.parameterId)
                return pluginName + ": " + parameter.name;

        return pluginName + ": " + target.parameterId;
    }

    Json describe (const model::AutomationTarget& target)
    {
        Json out = Json::object();

        switch (target.kind)
        {
            case model::AutomationTarget::Kind::trackVolume:
                out["kind"] = "volume";
                break;
            case model::AutomationTarget::Kind::trackPan:
                out["kind"] = "pan";
                break;
            case model::AutomationTarget::Kind::pluginParameter:
                out["kind"] = "pluginParam";
                out["pluginId"] = toolId::forPlugin (target.plugin);
                out["paramId"] = target.parameterId;
                break;
        }

        return out;
    }

    //==============================================================================
    /** The instrument a MIDI track plays through, and nothing for a track that
        has none.
    */
    std::string instrumentOf (const ToolTrack& track)
    {
        for (const auto& plugin : track.plugins)
            if (plugin.builtin == model::BuiltinPlugin::synth
                || plugin.builtin == model::BuiltinPlugin::sampler)
                return plugin.name;

        return {};
    }

    Json describeMixer (const ToolTrack& track)
    {
        Json sends = Json::array();

        for (const auto& send : track.sends)
        {
            Json entry = Json::object();
            entry["busId"] = toolId::forTrack (send.bus);
            entry["levelDb"] = send.levelDb;
            sends.push_back (entry);
        }

        Json mixer = Json::object();
        mixer["volumeDb"] = track.volumeDb;
        mixer["pan"] = track.pan;
        mixer["mute"] = track.muted;
        mixer["solo"] = track.soloed;
        mixer["sends"] = sends;

        return mixer;
    }

    RpcOutcome listTracks (const Session& session)
    {
        Json tracks = Json::array();

        for (const auto& track : toolTracksOf (session))
        {
            Json entry = Json::object();
            entry["id"] = toolId::forTrack (track.ref);
            entry["name"] = track.name;
            entry["kind"] = track.kindName();

            if (const auto instrument = instrumentOf (track); ! instrument.empty())
                entry["instrument"] = instrument;

            // The master is the one track with nowhere to go, and the absence of
            // the field is what says so.
            if (! track.isMaster)
                entry["output"] = toolId::forTrack (track.output);

            entry["clipCount"] = static_cast<int> (track.clips.size());
            entry["hasMidi"] =
                std::any_of (track.clips.begin(),
                             track.clips.end(),
                             [] (const model::ClipInfo& clip) { return clip.holdsMidi; });

            Json pluginNames = Json::array();

            for (const auto& plugin : track.plugins)
                pluginNames.push_back (plugin.name);

            entry["pluginNames"] = pluginNames;

            Json automated = Json::array();

            for (const auto& target : automatedTargetsOf (session, track))
                automated.push_back (nameOfTarget (session, track, target));

            entry["automatedParameters"] = automated;

            // Last, because it is what a producer's next gesture moves.
            entry["mixer"] = describeMixer (track);

            tracks.push_back (entry);
        }

        Json result = Json::object();
        result["tracks"] = tracks;

        return RpcOutcome::success (result);
    }

    //==============================================================================
    RpcOutcome getArrangement (const Session& session)
    {
        Json result = Json::object();

        // Only when the project declares one: a key it does not name is a
        // question for the analysis layer, not an empty field here.
        if (const auto key = session.key(); ! key.empty())
            result["key"] = key;

        result["tempoBpm"] = session.tempoBpm();

        const auto signature = session.timeSignature();
        result["timeSignature"] =
            std::to_string (signature.numerator) + "/" + std::to_string (signature.denominator);
        result["barCount"] = analysisCall::barCountOf (session);

        Json sections = Json::array();

        for (const auto& section : session.sections())
        {
            Json entry = Json::object();
            entry["name"] = section.name;
            entry["startBar"] = section.startBar;
            entry["endBar"] = section.endBar;
            sections.push_back (entry);
        }

        result["sections"] = sections;

        Json placements = Json::array();

        for (const auto& track : session.tracks())
        {
            if (track.clips.empty())
                continue;

            Json clips = Json::array();

            for (const auto& clip : track.clips)
            {
                const auto startBar = session.barAtSeconds (clip.startSeconds);

                Json entry = Json::object();
                entry["clipId"] = toolId::forClip (clip.clip);
                entry["name"] = clip.name;
                entry["startBar"] = startBar;
                entry["lengthBars"] =
                    session.barAtSeconds (clip.startSeconds + clip.lengthSeconds) - startBar;
                entry["looped"] = clip.looped;
                clips.push_back (entry);
            }

            Json placement = Json::object();
            placement["trackId"] = toolId::forTrack (track.track);
            placement["clips"] = clips;
            placements.push_back (placement);
        }

        result["placements"] = placements;

        return RpcOutcome::success (result);
    }

    //==============================================================================
    Json describeNotes (const Session& session, model::ClipRef clip)
    {
        Json notes = Json::array();

        auto inOrder = session.notes (clip);

        // In time, and within one moment from the lowest pitch up, so that a
        // chord reads the way it is voiced and the same clip serialises the same
        // way however its notes were written.
        std::sort (inOrder.begin(),
                   inOrder.end(),
                   [] (const model::NoteInfo& first, const model::NoteInfo& second)
                   {
                       if (first.startBeats != second.startBeats)
                           return first.startBeats < second.startBeats;

                       return first.pitch < second.pitch;
                   });

        for (const auto& note : inOrder)
        {
            Json entry = Json::object();
            entry["noteId"] = toolId::forNote (note.note);
            entry["pitch"] = note.pitch;
            entry["startBeats"] = note.startBeats;
            entry["lengthBeats"] = note.lengthBeats;
            entry["velocity"] = note.velocity;
            notes.push_back (entry);
        }

        return notes;
    }

    RpcOutcome getMidi (const Session& session, const ToolTrack& track, const std::string& clipId)
    {
        std::vector<model::ClipInfo> wanted;

        if (clipId.empty())
        {
            for (const auto& clip : track.clips)
                if (clip.holdsMidi)
                    wanted.push_back (clip);
        }
        else
        {
            const auto ref = toolId::toClip (clipId);

            const auto found =
                std::find_if (track.clips.begin(),
                              track.clips.end(),
                              [&] (const model::ClipInfo& clip)
                              { return ref.has_value() && clip.clip == *ref && clip.holdsMidi; });

            if (found == track.clips.end())
                return noSuchThing ("MIDI clip on that track", clipId);

            wanted.push_back (*found);
        }

        Json clips = Json::array();

        for (const auto& clip : wanted)
        {
            Json entry = Json::object();
            entry["clipId"] = toolId::forClip (clip.clip);
            entry["notes"] = describeNotes (session, clip.clip);
            clips.push_back (entry);
        }

        Json result = Json::object();
        result["clips"] = clips;

        return RpcOutcome::success (result);
    }

    //==============================================================================
    RpcOutcome getAutomation (const Session& session, const ToolTrack& track)
    {
        Json lanes = Json::array();

        for (const auto& target : automatedTargetsOf (session, track))
        {
            Json points = Json::array();

            // The model answers in time order already, and in the units the
            // target reads back in, which is what a point on a fader curve and
            // the fader itself being written the same way is worth.
            for (const auto& point : session.automationPoints (target))
            {
                Json entry = Json::object();
                entry["timeBeats"] = session.beatsAtSeconds (point.timeSeconds);
                entry["value"] = point.value;
                points.push_back (entry);
            }

            Json lane = Json::object();
            lane["target"] = describe (target);
            lane["points"] = points;
            lanes.push_back (lane);
        }

        Json result = Json::object();
        result["lanes"] = lanes;

        return RpcOutcome::success (result);
    }

    //==============================================================================
    /** A scanned plugin's own display text, which says what its normalised
        number means in words the plugin chose.

        Duet owns the semantics of its built-ins and can hand their values over
        bare. It does not own a scanned plugin's, so the one thing that explains
        that plugin's number crosses the seam wrapped, and the Collaborator can
        tell the two apart without being told (ADR 0002). It is not written into
        the run's estimate ledger yet, which is the other half of this value
        (issue 97ynt7).
    */
    Json estimatedDisplayString (const std::string& text)
    {
        return wrapped (Estimate { text, "the plugin's own display text", 0.5 });
    }

    Json describeParameters (const Session& session, const model::PluginInfo& plugin)
    {
        Json parameters = Json::array();

        for (const auto& parameter : session.pluginParameters (plugin.plugin))
        {
            Json entry = Json::object();
            entry["paramId"] = parameter.parameterId;

            if (plugin.builtin.has_value())
            {
                entry["name"] = parameter.name;
                entry["value"] = parameter.value;
                entry["unit"] = parameter.unit;

                // The two ends belong beside the value because a write outside
                // them is held at them: a Suggestion that asked for more than
                // the plugin has would otherwise land quietly short of what it
                // asked for, and nothing would say so.
                entry["min"] = parameter.minValue;
                entry["max"] = parameter.maxValue;
            }
            else
            {
                entry["vendorName"] = parameter.name;
                entry["normalizedValue"] = parameter.value;
                entry["displayString"] = estimatedDisplayString (parameter.displayValue);
            }

            parameters.push_back (entry);
        }

        return parameters;
    }

    RpcOutcome getPluginChain (const Session& session, const ToolTrack& track)
    {
        Json plugins = Json::array();

        for (const auto& plugin : track.plugins)
        {
            Json entry = Json::object();
            entry["pluginId"] = toolId::forPlugin (plugin.plugin);
            entry["name"] = plugin.name;
            entry["format"] = plugin.builtin.has_value() ? "builtin" : "vst3";
            entry["latencySamples"] =
                static_cast<int> (std::llround (plugin.latencySeconds * latencySampleRate));
            entry["enabled"] = ! plugin.bypassed;
            entry["parameters"] = describeParameters (session, plugin);
            plugins.push_back (entry);
        }

        Json result = Json::object();
        result["plugins"] = plugins;

        return RpcOutcome::success (result);
    }
} // namespace

//==============================================================================
ProjectTools::ProjectTools (model::Session& projectSession, ProjectReadMarshal readMarshal)
    : session (projectSession), marshal (std::move (readMarshal))
{
}

RpcOutcome ProjectTools::read (const std::function<RpcOutcome (const model::Session&)>& body) const
{
    auto outcome = RpcOutcome::failure (rpcError::internalError, "the project was never read");

    marshal ([&] { outcome = body (session); });

    return outcome;
}

void ProjectTools::addTo (ToolRegistry& registry)
{
    registry.add ("list_tracks",
                  [this] (const ToolCall&)
                  { return read ([] (const Session& project) { return listTracks (project); }); });

    registry.add (
        "get_arrangement",
        [this] (const ToolCall&)
        { return read ([] (const Session& project) { return getArrangement (project); }); });

    /** The three tools that take a track id all resolve it the same way, and a
        track the project does not hold is an error the model can correct
        against rather than an empty success.
    */
    const auto onTrack = [this] (auto body)
    {
        return [this, body] (const ToolCall& call) -> RpcOutcome
        {
            const auto id = argument (call, "trackId");

            if (id.empty())
                return missingArgument ("trackId");

            return read (
                [&] (const Session& project) -> RpcOutcome
                {
                    const auto ref = toolId::toTrack (id);

                    if (! ref.has_value())
                        return noSuchThing ("track", id);

                    const auto track = toolTrackFor (project, *ref);

                    if (! track.has_value())
                        return noSuchThing ("track", id);

                    return body (project, *track, call);
                });
        };
    };

    registry.add ("get_midi",
                  onTrack ([] (const Session& project, const ToolTrack& track, const ToolCall& call)
                           { return getMidi (project, track, argument (call, "clipId")); }));

    registry.add ("get_automation",
                  onTrack ([] (const Session& project, const ToolTrack& track, const ToolCall&)
                           { return getAutomation (project, track); }));

    registry.add ("get_plugin_chain",
                  onTrack ([] (const Session& project, const ToolTrack& track, const ToolCall&)
                           { return getPluginChain (project, track); }));
}
} // namespace duet::collab
