#include <duet/collab/SuggestionManager.h>

#include <duet/collab/ProjectTools.h>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace duet::collab
{
namespace
{
    using model::Session;

    /** What the project itself is remembered under: what tempo and time
        signature belong to, neither of them being a thing with an id.
    */
    constexpr std::string_view projectKey = "project";

    /** What an automation curve is remembered under. A curve is named by what
        it drives rather than by an id of its own, so its key is written out of
        that: `automation:volume:track-3`, `automation:param:plugin-2:gain`.
    */
    constexpr std::string_view automationPrefix = "automation:";

    /** What nothing reads back as. A thing the producer has deleted has moved
        as surely as one they have dragged.
    */
    constexpr std::string_view gone = "gone";

    /** How a number reaches a descriptor: the same digits for the same value,
        so that two readings of an untouched project are the same text.
    */
    std::string number (double value, int places = 3)
    {
        std::ostringstream written;
        written.precision (places);
        written << std::fixed << value;

        return written.str();
    }

    std::string named (std::string_view what, std::string_view name)
    {
        return std::string { what } + " \"" + std::string { name } + "\"";
    }

    std::string kindOf (model::TrackKind kind)
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

    std::string pluginsOf (const std::vector<model::PluginInfo>& plugins)
    {
        std::string written = "plugins [";

        for (const auto& plugin : plugins)
        {
            if (written.back() != '[')
                written += ", ";

            written += named (toolId::forPlugin (plugin.plugin), plugin.name);
        }

        return written + "]";
    }

    std::string describeMaster (const Session& session)
    {
        const auto master = session.master();

        return named (toolId::forTrack (model::masterChannel), "Master") + ", "
               + number (master.volumeDb, 2) + " dB, pan " + number (master.pan, 2) + ", "
               + (master.muted ? "muted" : "unmuted") + ", " + pluginsOf (master.plugins);
    }

    std::string describeTrack (const Session& session, model::TrackRef ref)
    {
        if (ref == model::masterChannel)
            return describeMaster (session);

        const auto track = session.track (ref);

        if (track.track == model::noTrack)
            return std::string { gone };

        std::string written = named (toolId::forTrack (ref), track.name) + " " + kindOf (track.kind)
                              + ", out " + toolId::forTrack (track.output) + ", "
                              + number (track.volumeDb, 2) + " dB, pan " + number (track.pan, 2)
                              + ", " + (track.muted ? "muted" : "unmuted") + ", "
                              + (track.soloed ? "soloed" : "unsoloed") + ", colour "
                              + std::to_string (static_cast<int> (track.colour)) + ", "
                              + (track.recordArmed ? "armed" : "unarmed") + ", sends [";

        for (const auto& send : track.sends)
        {
            if (written.back() != '[')
                written += ", ";

            written += toolId::forTrack (send.bus) + " " + number (send.levelDb, 2) + " dB";
        }

        return written + "], " + pluginsOf (track.plugins);
    }

    /** Where a clip is, which is a question about every track. */
    std::string describeClip (const Session& session, model::ClipRef ref)
    {
        for (const auto& track : session.tracks())
            for (const auto& clip : track.clips)
            {
                if (clip.clip != ref)
                    continue;

                const auto startBar = session.barAtSeconds (clip.startSeconds);
                const auto endBar = session.barAtSeconds (clip.startSeconds + clip.lengthSeconds);

                return named (toolId::forClip (ref), clip.name) + " on "
                       + toolId::forTrack (track.track) + ", bar " + number (startBar) + " for "
                       + number (endBar - startBar) + " bars, offset "
                       + number (clip.contentOffsetSeconds) + " s, "
                       + (clip.looped ? "looped over " + number (clip.loopLengthBeats) + " beats"
                                      : "unlooped")
                       + ", " + (clip.holdsMidi ? "midi" : "audio " + clip.sourceReference);
            }

        return std::string { gone };
    }

    std::string describeNote (const Session& session, model::NoteRef ref)
    {
        for (const auto& track : session.tracks())
            for (const auto& clip : track.clips)
                for (const auto& note : session.notes (clip.clip))
                {
                    if (note.note != ref)
                        continue;

                    return toolId::forNote (ref) + " in " + toolId::forClip (clip.clip) + ", pitch "
                           + std::to_string (note.pitch) + ", beat " + number (note.startBeats)
                           + " for " + number (note.lengthBeats) + ", velocity "
                           + std::to_string (note.velocity);
                }

        return std::string { gone };
    }

    std::string describeParameters (const Session& session, model::PluginRef ref)
    {
        std::string written = "params [";

        for (const auto& parameter : session.pluginParameters (ref))
        {
            if (written.back() != '[')
                written += ", ";

            written += parameter.parameterId + "=" + number (parameter.value, 4);
        }

        return written + "]";
    }

    std::string describeOnePlugin (const Session& session,
                                   model::TrackRef track,
                                   int position,
                                   const model::PluginInfo& plugin)
    {
        return named (toolId::forPlugin (plugin.plugin), plugin.name) + " on "
               + toolId::forTrack (track) + " at " + std::to_string (position) + ", "
               + (plugin.bypassed ? "bypassed" : "active") + ", "
               + (plugin.missing ? "missing" : "present") + ", sidechain "
               + toolId::forTrack (plugin.sidechainSource) + ", "
               + describeParameters (session, plugin.plugin);
    }

    std::string describePlugin (const Session& session, model::PluginRef ref)
    {
        const auto master = session.master();

        for (std::size_t at = 0; at < master.plugins.size(); ++at)
            if (master.plugins.at (at).plugin == ref)
                return describeOnePlugin (
                    session, model::masterChannel, static_cast<int> (at), master.plugins.at (at));

        for (const auto& track : session.tracks())
            for (std::size_t at = 0; at < track.plugins.size(); ++at)
                if (track.plugins.at (at).plugin == ref)
                    return describeOnePlugin (
                        session, track.track, static_cast<int> (at), track.plugins.at (at));

        return std::string { gone };
    }

    std::string describeProject (const Session& session)
    {
        const auto signature = session.timeSignature();

        return "tempo " + number (session.tempoBpm(), 2) + " bpm, "
               + std::to_string (signature.numerator) + "/"
               + std::to_string (signature.denominator);
    }

    /** The curve a key names, or nothing when the key is not one of a curve. */
    std::optional<model::AutomationTarget> curveOf (std::string_view key)
    {
        if (! key.starts_with (automationPrefix))
            return {};

        auto rest = key.substr (automationPrefix.size());
        const auto kindEnd = rest.find (':');

        if (kindEnd == std::string_view::npos)
            return {};

        const auto kind = rest.substr (0, kindEnd);
        rest = rest.substr (kindEnd + 1);

        if (kind == "volume" || kind == "pan")
        {
            const auto track = toolId::toTrack (rest);

            if (! track.has_value())
                return {};

            return kind == "volume" ? model::AutomationTarget::trackVolumeOf (*track)
                                    : model::AutomationTarget::trackPanOf (*track);
        }

        const auto parameterAt = rest.find (':');

        if (parameterAt == std::string_view::npos)
            return {};

        const auto plugin = toolId::toPlugin (rest.substr (0, parameterAt));

        if (! plugin.has_value())
            return {};

        return model::AutomationTarget::parameterOf (*plugin,
                                                     std::string { rest.substr (parameterAt + 1) });
    }

    std::string describeCurve (const Session& session, const model::AutomationTarget& target)
    {
        std::string written = "points [";

        for (const auto& point : session.automationPoints (target))
        {
            if (written.back() != '[')
                written += ", ";

            written += number (point.timeSeconds) + " s = " + number (point.value, 4);
        }

        return written + "]";
    }

    /** What the project says about one remembered thing, right now. */
    std::string describe (const Session& session, const std::string& key)
    {
        if (key == projectKey)
            return describeProject (session);

        if (const auto curve = curveOf (key))
            return describeCurve (session, *curve);

        if (const auto track = toolId::toTrack (key))
            return describeTrack (session, *track);

        if (const auto clip = toolId::toClip (key))
            return describeClip (session, *clip);

        if (const auto plugin = toolId::toPlugin (key))
            return describePlugin (session, *plugin);

        if (const auto note = toolId::toNote (key))
            return describeNote (session, *note);

        return std::string { gone };
    }

    /** Whether a string is an id of the project rather than a placeholder, a
        name, or anything else a call may write.
    */
    bool isProjectId (std::string_view text)
    {
        return toolId::toTrack (text).has_value() || toolId::toClip (text).has_value()
               || toolId::toPlugin (text).has_value() || toolId::toNote (text).has_value();
    }

    /** Every id anywhere inside one operation, whatever field it was written
        in: an operation names what it touches, and a walk over the values it
        was accepted in cannot miss one the way a list of field names can.
    */
    void idsIn (const Json& value, std::set<std::string>& found)
    {
        std::vector<const Json*> unread { &value };

        while (! unread.empty())
        {
            const auto* next = unread.back();
            unread.pop_back();

            if (next->is_string())
            {
                auto text = next->get<std::string>();

                if (isProjectId (text))
                    found.insert (std::move (text));

                continue;
            }

            if (next->is_object() || next->is_array())
                for (const auto& under : *next)
                    unread.push_back (&under);
        }
    }

    /** The curve an automation operation draws on, as a key, and nothing when
        it draws on something the same Element is still to create.
    */
    std::optional<std::string> curveKeyOf (const Json& operation)
    {
        const auto target = operation.find ("target");

        if (target == operation.end() || ! target->is_object())
            return {};

        const auto kind = target->find ("kind");

        if (kind == target->end() || ! kind->is_string())
            return {};

        const auto written = kind->get<std::string>();

        if (written == "volume" || written == "pan")
        {
            const auto track = operation.find ("trackId");

            if (track == operation.end() || ! track->is_string()
                || ! toolId::toTrack (track->get<std::string>()).has_value())
                return {};

            return std::string { automationPrefix } + written + ":" + track->get<std::string>();
        }

        const auto plugin = target->find ("pluginId");
        const auto parameter = target->find ("paramId");

        if (plugin == target->end() || ! plugin->is_string() || parameter == target->end()
            || ! parameter->is_string()
            || ! toolId::toPlugin (plugin->get<std::string>()).has_value())
            return {};

        return std::string { automationPrefix } + "param:" + plugin->get<std::string>() + ":"
               + parameter->get<std::string>();
    }

    /** Everything one Suggestion's operations name. */
    std::set<std::string> referencesOf (const Suggestion& made)
    {
        std::set<std::string> keys;

        for (const auto& element : made.elements)
            for (const auto& operation : element.operations)
            {
                idsIn (operation, keys);

                const auto name = operation.find ("op");

                if (name != operation.end() && name->is_string()
                    && name->get<std::string>().starts_with ("project."))
                    keys.insert (std::string { projectKey });

                if (const auto curve = curveKeyOf (operation))
                    keys.insert (*curve);
            }

        return keys;
    }
} // namespace

//==============================================================================
class SuggestionManager::Impl
{
public:
    Impl (model::Session& projectSession, RunLauncher launchRun)
        : session (projectSession), launch (std::move (launchRun))
    {
    }

    ~Impl() = default;

    Impl (const Impl&) = delete;
    Impl (Impl&&) = delete;
    Impl& operator= (const Impl&) = delete;
    Impl& operator= (Impl&&) = delete;

    RunStart ask (std::string request)
    {
        auto started = launch (request);

        if (started.started)
            waiting[started.runId] = { std::move (request), {} };

        return started;
    }

    bool suggested (const std::string& runId, Suggestion made)
    {
        const auto run = waiting.find (runId);

        if (run == waiting.end())
            return false;

        Held held;
        held.info.made = std::move (made);
        held.info.request = run->second.request;
        held.info.revises = run->second.revises;
        held.info.elements.assign (held.info.made.elements.size(), ElementState::pending);

        waiting.erase (run);
        rebaseline (held);
        kept.push_back (std::move (held));

        return true;
    }

    [[nodiscard]] std::vector<SuggestionInfo> suggestions()
    {
        sweep();

        std::vector<SuggestionInfo> all;
        all.reserve (kept.size());

        for (const auto& held : kept)
            all.push_back (held.info);

        return all;
    }

    [[nodiscard]] std::optional<SuggestionInfo> suggestion (std::string_view id)
    {
        sweep();

        for (const auto& held : kept)
            if (held.info.made.id == id)
                return held.info;

        return {};
    }

    bool acceptElement (std::string_view id, std::size_t element)
    {
        auto* held = withPendingElement (id, element);

        if (held == nullptr)
            return false;

        apply (*held, held->info.made.elements.at (element).changes);
        held->info.elements.at (element) = ElementState::accepted;
        resolve (*held);

        return true;
    }

    bool rejectElement (std::string_view id, std::size_t element)
    {
        auto* held = withPendingElement (id, element);

        if (held == nullptr)
            return false;

        held->info.elements.at (element) = ElementState::rejected;
        resolve (*held);

        return true;
    }

    bool accept (std::string_view id)
    {
        auto* held = pending (id);

        if (held == nullptr)
            return false;

        apply (*held, everythingLeftOf (*held));

        for (auto& state : held->info.elements)
            if (state == ElementState::pending)
                state = ElementState::accepted;

        resolve (*held);

        return true;
    }

    bool reject (std::string_view id)
    {
        auto* held = pending (id);

        if (held == nullptr)
            return false;

        stopAuditionOf (*held);

        for (auto& state : held->info.elements)
            if (state == ElementState::pending)
                state = ElementState::rejected;

        resolve (*held);

        return true;
    }

    RunStart reply (std::string_view id, const std::string& what)
    {
        sweep();
        auto* held = heldSuggestion (id);

        if (held == nullptr)
            return RunStart::rejected (rpcError::invalidParams, "there is no such Suggestion");

        if (held->info.state != SuggestionState::pending
            && held->info.state != SuggestionState::rejected)
            return RunStart::rejected (rpcError::invalidParams,
                                       "this Suggestion has been resolved already");

        auto started = launch (revisionPrompt (*held, what));

        if (! started.started)
            return started;

        waiting[started.runId] = { held->info.request, held->info.made.id };

        // A rejected Suggestion stays rejected: the producer has already said
        // what became of it, and the reply is about what comes next.
        if (held->info.state == SuggestionState::pending)
            supersede (*held);

        return started;
    }

    RunStart redo (std::string_view id)
    {
        auto* held = pending (id);

        if (held == nullptr)
            return RunStart::rejected (rpcError::invalidParams,
                                       "there is no such pending Suggestion");

        auto started = launch (redoPrompt (*held));

        if (! started.started)
            return started;

        waiting[started.runId] = { held->info.request, held->info.made.id };
        supersede (*held);

        return started;
    }

    bool audition (std::string_view id)
    {
        auto* held = pending (id);

        if (held == nullptr)
            return false;

        stopAudition();

        // The model reads the operations it is auditioning for as long as the
        // Audition lasts, so what it is given has to outlive the call.
        live = std::make_unique<model::Suggestion> (everythingLeftOf (*held));

        if (! session.auditionSuggestion (*live))
        {
            live.reset();

            return false;
        }

        liveFor = held->info.made.id;

        return true;
    }

    void stopAudition()
    {
        if (live == nullptr)
            return;

        session.stopAudition();
        live.reset();
        liveFor.clear();
    }

    [[nodiscard]] std::optional<std::string> auditioning() const
    {
        if (live == nullptr)
            return {};

        return liveFor;
    }

private:
    /** What one started run is answering, until it answers. */
    struct Run
    {
        std::string request;
        std::string revises;
    };

    /** One Suggestion, and what the project said about everything it names at
        the moment it was made: what staleness is measured against.
    */
    struct Held
    {
        SuggestionInfo info;
        std::map<std::string, std::string> baseline;
    };

    /** The Suggestion an id names while it is still the producer's to resolve,
        and nothing for one that is not there or has reached an ending.
    */
    [[nodiscard]] Held* pending (std::string_view id)
    {
        sweep();

        for (auto& held : kept)
            if (held.info.made.id == id)
                return held.info.state == SuggestionState::pending ? &held : nullptr;

        return nullptr;
    }

    /** The Suggestion holding an Element that is still the producer's to
        resolve, and nothing when there is no such Element to resolve.
    */
    [[nodiscard]] Held* withPendingElement (std::string_view id, std::size_t element)
    {
        auto* held = pending (id);

        if (held == nullptr || element >= held->info.elements.size()
            || held->info.elements.at (element) != ElementState::pending)
            return nullptr;

        return held;
    }

    /** Every Element of a Suggestion the producer has not resolved, as one
        operation list named for the whole: what accepting it now would do.
    */
    [[nodiscard]] static model::Suggestion everythingLeftOf (const Held& held)
    {
        model::Suggestion changes { held.info.made.summary };

        for (std::size_t at = 0; at < held.info.elements.size(); ++at)
            if (held.info.elements.at (at) == ElementState::pending)
                changes.append (held.info.made.elements.at (at).changes);

        return changes;
    }

    /** Ends an Audition of this Suggestion, and leaves another one's alone. */
    void stopAuditionOf (const Held& held)
    {
        if (liveFor == held.info.made.id)
            stopAudition();
    }

    /** The Suggestion an id names, whatever state it is in. */
    [[nodiscard]] Held* heldSuggestion (std::string_view id)
    {
        for (auto& held : kept)
            if (held.info.made.id == id)
                return &held;

        return nullptr;
    }

    /** Replaces a pending Suggestion with the one about to be asked for.

        Nothing of it was applied, so every Element still pending is one the
        producer never took, and the Suggestion itself is neither accepted nor
        rejected but answered by another.
    */
    void supersede (Held& held)
    {
        stopAuditionOf (held);

        for (auto& state : held.info.elements)
            if (state == ElementState::pending)
                state = ElementState::rejected;

        held.info.state = SuggestionState::superseded;
    }

    /** What the original request reads as when it is asked again.

        The request itself comes first and what has happened since comes last,
        which is the prompt-cache discipline the Collaborator spec asks of
        everything this side sends: a second asking invalidates the tail of what
        a provider has cached rather than its middle.
    */
    [[nodiscard]] static std::string against (const Held& held)
    {
        return held.info.request + "\n\nThat was answered with the Suggestion \""
               + held.info.made.summary + "\", which is not what the project needs now.";
    }

    [[nodiscard]] static std::string revisionPrompt (const Held& held, const std::string& what)
    {
        return against (held) + " What the producer said about it:\n" + what
               + "\n\nAnswer the request again, taking that into account.";
    }

    /** The same request, and everything the producer has changed under it. */
    [[nodiscard]] std::string redoPrompt (const Held& held)
    {
        std::string changed;

        for (const auto& [key, was] : held.baseline)
        {
            const auto now = describe (session, key);

            if (now == was)
                continue;

            changed.append ("\n- ").append (key);
            changed.append ("\n  was: ").append (was);
            changed.append ("\n  is now: ").append (now);
        }

        if (changed.empty())
            return against (held)
                   + "\n\nAnswer the request again, against the project as it "
                     "now stands.";

        return against (held) + " Since it was made, the producer has changed the project:"
               + changed + "\n\nAnswer the request again, against the project as it now stands.";
    }

    static void resolve (Held& held)
    {
        const auto is = [&held] (ElementState wanted)
        {
            return std::any_of (held.info.elements.begin(),
                                held.info.elements.end(),
                                [wanted] (auto state) { return state == wanted; });
        };

        if (is (ElementState::pending))
            return;

        held.info.state =
            is (ElementState::accepted) ? SuggestionState::accepted : SuggestionState::rejected;
    }

    /** Reads what the project says now about everything this Suggestion names,
        and calls that what it was made against.
    */
    void rebaseline (Held& held)
    {
        held.baseline.clear();

        for (const auto& key : referencesOf (held.info.made))
            held.baseline[key] = describe (session, key);

        held.info.stale = false;
    }

    /** Applies operations as one Action, and takes what they did as this
        Suggestion's own doing rather than as the project moving under it.
    */
    void apply (Held& held, const model::Suggestion& changes)
    {
        stopAudition();
        session.acceptSuggestion (changes);
        sweep();
        rebaseline (held);
    }

    /** Compares every pending Suggestion with what it was made against, unless
        nothing has happened since the last look.

        A live Audition is the project holding a Suggestion's own changes rather
        than the producer's, so nothing is compared while one is on; leaving it
        puts the project back exactly, and the next look sees what it saw.
    */
    void sweep()
    {
        if (session.isAuditioning() || session.revision() == lastRevision)
            return;

        lastRevision = session.revision();

        for (auto& held : kept)
        {
            if (held.info.state != SuggestionState::pending)
                continue;

            held.info.stale =
                std::any_of (held.baseline.begin(),
                             held.baseline.end(),
                             [this] (const auto& entry)
                             { return describe (session, entry.first) != entry.second; });
        }
    }

    model::Session& session;
    RunLauncher launch;

    std::map<std::string, Run> waiting;
    std::vector<Held> kept;
    std::uint64_t lastRevision = 0;

    /** What the model is auditioning, held here because it reads it for as long
        as the Audition lasts, and which Suggestion it came from.
    */
    std::unique_ptr<model::Suggestion> live;
    std::string liveFor;
};

//==============================================================================
SuggestionManager::SuggestionManager (model::Session& projectSession, RunLauncher launchRun)
    : impl (std::make_unique<Impl> (projectSession, std::move (launchRun)))
{
}

SuggestionManager::~SuggestionManager() = default;

RunStart SuggestionManager::ask (std::string request) { return impl->ask (std::move (request)); }

bool SuggestionManager::suggested (const std::string& runId, Suggestion made)
{
    return impl->suggested (runId, std::move (made));
}

std::vector<SuggestionInfo> SuggestionManager::suggestions() const { return impl->suggestions(); }

std::optional<SuggestionInfo> SuggestionManager::suggestion (std::string_view id) const
{
    return impl->suggestion (id);
}

bool SuggestionManager::acceptElement (std::string_view id, std::size_t element)
{
    return impl->acceptElement (id, element);
}

bool SuggestionManager::rejectElement (std::string_view id, std::size_t element)
{
    return impl->rejectElement (id, element);
}

bool SuggestionManager::accept (std::string_view id) { return impl->accept (id); }

bool SuggestionManager::reject (std::string_view id) { return impl->reject (id); }

RunStart SuggestionManager::reply (std::string_view id, const std::string& what)
{
    return impl->reply (id, what);
}

RunStart SuggestionManager::redo (std::string_view id) { return impl->redo (id); }

bool SuggestionManager::audition (std::string_view id) { return impl->audition (id); }

void SuggestionManager::stopAudition() { impl->stopAudition(); }

std::optional<std::string> SuggestionManager::auditioning() const { return impl->auditioning(); }
} // namespace duet::collab
