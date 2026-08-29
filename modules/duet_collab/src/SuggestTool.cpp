#include <duet/collab/SuggestTool.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace duet::collab
{
SuggestionElement::SuggestionElement (std::string elementDescription)
    : description (std::move (elementDescription)), changes (description)
{
}

Suggestion::Suggestion (std::string suggestionId, std::string suggestionSummary)
    : id (std::move (suggestionId)), summary (std::move (suggestionSummary)), changes (summary)
{
}

namespace
{
    using model::Session;

    /** What marks a placeholder: an id an earlier operation of the same element
        declared, rather than one the project already holds.

        A project id never begins with it and a placeholder always does, so
        neither end of the seam has to guess which of the two it is looking at.
    */
    constexpr char placeholderMark = '#';

    /** How far outside a range a value may sit and still be its end.

        A number that crossed the seam as text is the end of the range it was
        read from, and a rejection turning on its last bit would be one nobody
        could correct.
    */
    constexpr double rangeTolerance = 1.0e-9;

    /** The two ends of a pan control, which are the model's own. */
    constexpr double panLeft = -1.0;
    constexpr double panRight = 1.0;

    /** What a MIDI note may be: the range the format itself has. A velocity of
        zero is a note-off rather than a note, so a note starts at one.
    */
    constexpr int lowestPitch = 0;
    constexpr int highestPitch = 127;
    constexpr int quietestVelocity = 1;
    constexpr int loudestVelocity = 127;

    /** The two whole numbers that are only bounded by what an int holds: a
        chain position, which the model holds to the chain's own ends, and the
        two halves of a time signature.
    */
    constexpr int largestPosition = std::numeric_limits<int>::max();
    constexpr int largestCount = std::numeric_limits<int>::max();

    /** What a scanned plugin's parameter is measured in: nothing Duet owns, so
        the vocabulary carries the plugin's own normalised value untouched.
    */
    constexpr double normalisedLowest = 0.0;
    constexpr double normalisedHighest = 1.0;

    //==============================================================================
    /** What went wrong with one operation, and nothing when nothing did. */
    using Problem = std::optional<std::string>;

    /** Where an operation is written, so that an error is corrected against the
        call that carried it rather than searched for.
    */
    std::string positionOf (std::size_t element, std::size_t operation)
    {
        return "elements[" + std::to_string (element) + "].operations[" + std::to_string (operation)
               + "]";
    }

    Problem needs (std::string_view field)
    {
        return "this operation needs a " + std::string { field };
    }

    Problem noSuchThing (std::string_view kind, const std::string& id)
    {
        return "this project has no " + std::string { kind } + " called " + id;
    }

    /** A number as an error should print it: what was written, without the
        trailing zeros a fixed-point conversion adds to it.
    */
    std::string numberText (double value)
    {
        auto text = std::to_string (value);

        if (text.find ('.') == std::string::npos)
            return text;

        text.erase (text.find_last_not_of ('0') + 1);

        if (text.back() == '.')
            text.pop_back();

        return text;
    }

    Problem outsideRange (std::string_view field, double value, double lowest, double highest)
    {
        return std::string { field } + " is " + numberText (value) + ", and it must be between "
               + numberText (lowest) + " and " + numberText (highest);
    }

    //==============================================================================
    /** One of an operation's fields, read as the kind of thing it has to be.

        A field that is absent and one that is the wrong kind both read as
        nothing, so a malformed operation and an incomplete one take the same
        path: the caller says what it needed and the error says it did not get
        it.
    */
    std::optional<std::string> text (const Json& operation, std::string_view name)
    {
        const auto found = operation.find (name);

        if (found == operation.end() || ! found->is_string())
            return {};

        return found->get<std::string>();
    }

    std::optional<double> number (const Json& operation, std::string_view name)
    {
        const auto found = operation.find (name);

        if (found == operation.end() || ! found->is_number())
            return {};

        const auto value = found->get<double>();

        return std::isfinite (value) ? std::optional { value } : std::nullopt;
    }

    std::optional<int> whole (const Json& operation, std::string_view name)
    {
        const auto value = number (operation, name);

        if (! value.has_value() || *value != std::floor (*value))
            return {};

        return static_cast<int> (*value);
    }

    std::optional<bool> flag (const Json& operation, std::string_view name)
    {
        const auto found = operation.find (name);

        if (found == operation.end() || ! found->is_boolean())
            return {};

        return found->get<bool>();
    }

    const Json* list (const Json& operation, std::string_view name)
    {
        const auto found = operation.find (name);

        if (found == operation.end() || ! found->is_array())
            return nullptr;

        return &(*found);
    }

    const Json* object (const Json& operation, std::string_view name)
    {
        const auto found = operation.find (name);

        if (found == operation.end() || ! found->is_object())
            return nullptr;

        return &(*found);
    }

    //==============================================================================
    /** What kind of thing an id names. A placeholder carries its own, because
        nothing about it can be read off the project.
    */
    enum class Kind : std::uint8_t
    {
        track,
        clip,
        plugin,
        note
    };

    const char* nameOf (Kind kind)
    {
        switch (kind)
        {
            case Kind::track:
                return "track";
            case Kind::clip:
                return "clip";
            case Kind::plugin:
                return "plugin";
            case Kind::note:
                return "note";
        }

        return "thing";
    }

    /** An id resolved: something the project holds, or something an earlier
        operation of the same element makes.
    */
    struct Resolved
    {
        model::SuggestionTarget target = std::uint64_t { 0 };
        Kind kind = Kind::track;
        bool isPlaceholder = false;

        /** Which built-in a plugin an earlier operation adds is, and nothing
            for an external one — the only thing about a plugin that does not
            exist yet that says what parameters it is going to have.
        */
        std::optional<model::BuiltinPlugin> builtin;

        /** The project's own reference, and nothing for a placeholder, whose
            reference does not exist yet and cannot be read about.
        */
        [[nodiscard]] std::optional<std::uint64_t> inProject() const
        {
            if (isPlaceholder)
                return {};

            return std::get<std::uint64_t> (target);
        }
    };

    /** What a note is at right now, so that a change to part of it keeps the
        rest: the model moves a note to a pitch and a beat together, and a call
        may name only one of them.
    */
    struct NoteState
    {
        int pitch = 0;
        double startBeats = 0.0;
    };

    //==============================================================================
    /** The project, in the shapes the write vocabulary asks of it. */
    class Project
    {
    public:
        explicit Project (const Session& projectSession) : session (projectSession) {}

        ~Project() = default;

        Project (const Project&) = delete;
        Project (Project&&) = delete;
        Project& operator= (const Project&) = delete;
        Project& operator= (Project&&) = delete;

        [[nodiscard]] bool hasTrack (model::TrackRef track) const
        {
            return track == model::masterChannel || session.track (track).track != model::noTrack;
        }

        [[nodiscard]] std::optional<model::ClipInfo> clip (model::ClipRef ref) const
        {
            for (const auto& track : session.tracks())
                for (const auto& held : track.clips)
                    if (held.clip == ref)
                        return held;

            return {};
        }

        [[nodiscard]] std::optional<model::PluginInfo> plugin (model::PluginRef ref) const
        {
            for (const auto& track : session.tracks())
                for (const auto& held : track.plugins)
                    if (held.plugin == ref)
                        return held;

            for (const auto& held : session.master().plugins)
                if (held.plugin == ref)
                    return held;

            return {};
        }

        /** The track a clip sits on. */
        [[nodiscard]] std::optional<model::TrackRef> trackOfClip (model::ClipRef ref) const
        {
            for (const auto& track : session.tracks())
                for (const auto& held : track.clips)
                    if (held.clip == ref)
                        return track.track;

            return {};
        }

        /** The track a plugin's chain belongs to. */
        [[nodiscard]] std::optional<model::TrackRef> trackOf (model::PluginRef ref) const
        {
            for (const auto& track : session.tracks())
                for (const auto& held : track.plugins)
                    if (held.plugin == ref)
                        return track.track;

            for (const auto& held : session.master().plugins)
                if (held.plugin == ref)
                    return model::masterChannel;

            return {};
        }

        [[nodiscard]] std::optional<model::NoteInfo> note (model::ClipRef clip,
                                                           model::NoteRef ref) const
        {
            for (const auto& held : session.notes (clip))
                if (held.note == ref)
                    return held;

            return {};
        }

        [[nodiscard]] std::optional<model::PluginParameterInfo>
            parameter (model::PluginRef plugin, const std::string& parameterId) const
        {
            for (const auto& held : session.pluginParameters (plugin))
                if (held.parameterId == parameterId)
                    return held;

            return {};
        }

        /** One of a built-in's parameters, before the project holds one of it.

            Duet ships the built-ins, so this is a fact about the plugin and not
            about any instance of it, which is what makes it answerable for a
            plugin an element is only adding.
        */
        [[nodiscard]] std::optional<model::PluginParameterInfo>
            builtinParameter (model::BuiltinPlugin plugin, const std::string& parameterId) const
        {
            for (const auto& held : parametersOf (plugin))
                if (held.parameterId == parameterId)
                    return held;

            return {};
        }

        [[nodiscard]] bool knowsExternal (const std::string& identifier) const
        {
            const auto known = session.knownVst3Plugins();

            return std::ranges::any_of (known,
                                        [&] (const model::KnownPluginInfo& plugin)
                                        { return plugin.identifier == identifier; });
        }

        [[nodiscard]] const Session& read() const { return session; }

    private:
        /** What a built-in has, asked of the model once for each built-in a
            call names: the model makes a plugin to answer it, and one element
            may set several parameters of the same one.
        */
        [[nodiscard]] const std::vector<model::PluginParameterInfo>&
            parametersOf (model::BuiltinPlugin plugin) const
        {
            const auto found = builtins.find (plugin);

            if (found != builtins.end())
                return found->second;

            return builtins.emplace (plugin, session.builtinPluginParameters (plugin))
                .first->second;
        }

        const Session& session;

        mutable std::map<model::BuiltinPlugin, std::vector<model::PluginParameterInfo>> builtins;
    };

    //==============================================================================
    /** One element on its way into the model's own operation list.

        Checking and building are one walk: an operation that cannot be checked
        cannot be built either, so every check happens where its value is wanted
        and every error names the field that was wrong.

        Placeholders are scoped to the element, because an element is the
        cherry-pick unit and has to apply on its own. `madeElsewhere` is what
        lets the refusal say which of the two went wrong: a name another element
        declared, or a name nothing declared at all.
    */
    class Element
    {
    public:
        Element (const Project& projectRead,
                 model::Suggestion& target,
                 const std::set<std::string>& refsOfEarlierElements)
            : project (projectRead), out (target), madeElsewhere (refsOfEarlierElements)
        {
        }

        ~Element() = default;

        Element (const Element&) = delete;
        Element (Element&&) = delete;
        Element& operator= (const Element&) = delete;
        Element& operator= (Element&&) = delete;

        [[nodiscard]] const Project& reads() const { return project; }
        [[nodiscard]] model::Suggestion& changes() { return out; }

        /** Resolves one of an operation's id fields. */
        Problem take (const Json& operation, std::string_view field, Kind kind, Resolved& resolved)
        {
            const auto id = text (operation, field);

            if (! id.has_value())
                return needs (field);

            return resolve (*id, kind, resolved);
        }

        /** Resolves an id that is written somewhere other than a named field of
            the operation — inside one of its entries, or in an id list.
        */
        Problem resolve (const std::string& id, Kind kind, Resolved& resolved)
        {
            if (! id.empty() && id.front() == placeholderMark)
            {
                const auto found = declared.find (id);

                if (found == declared.end())
                    return madeElsewhere.contains (id)
                               ? Problem { id
                                           + " is made by another element, and an element has to be "
                                             "applicable on its own" }
                               : Problem { "nothing in this element declares " + id };

                if (found->second.kind != kind)
                    return id + " is a " + nameOf (found->second.kind) + ", not a " + nameOf (kind);

                resolved = found->second;

                return {};
            }

            const auto ref = referenceIn (id, kind);

            if (! ref.has_value())
                return noSuchThing (nameOf (kind), id);

            resolved = { *ref, kind, false };

            return {};
        }

        /** Records what an operation makes, under the name the call gave it. */
        Problem declare (const Json& operation,
                         Kind kind,
                         const model::SuggestionRef& made,
                         std::optional<model::BuiltinPlugin> builtin = {})
        {
            return declareAs (text (operation, "ref"), kind, made, builtin);
        }

        Problem declareAs (const std::optional<std::string>& name,
                           Kind kind,
                           const model::SuggestionRef& made,
                           std::optional<model::BuiltinPlugin> builtin = {})
        {
            if (! name.has_value())
                return {};

            if (name->empty() || name->front() != placeholderMark)
                return "a ref begins with " + std::string { 1, placeholderMark } + ", and " + *name
                       + " does not";

            if (declared.contains (*name))
                return "this element already declares " + *name;

            declared[*name] = { made, kind, true, builtin };

            return {};
        }

        [[nodiscard]] const std::map<std::string, Resolved>& refs() const { return declared; }

        /** What a note is at, for a change that names only part of it. */
        [[nodiscard]] std::optional<NoteState> noteState (const std::string& id) const
        {
            const auto found = states.find (id);

            return found == states.end() ? std::nullopt : std::optional { found->second };
        }

        void rememberNote (const std::string& id, NoteState state) { states[id] = state; }

    private:
        [[nodiscard]] std::optional<std::uint64_t> referenceIn (const std::string& id,
                                                                Kind kind) const
        {
            switch (kind)
            {
                case Kind::track:
                {
                    const auto ref = toolId::toTrack (id);

                    return ref.has_value() && project.hasTrack (*ref) ? ref : std::nullopt;
                }
                case Kind::clip:
                {
                    const auto ref = toolId::toClip (id);

                    return ref.has_value() && project.clip (*ref).has_value() ? ref : std::nullopt;
                }
                case Kind::plugin:
                {
                    const auto ref = toolId::toPlugin (id);

                    return ref.has_value() && project.plugin (*ref).has_value() ? ref
                                                                                : std::nullopt;
                }
                case Kind::note:
                    // A note is only ever resolved against the clip that holds
                    // it, which is what takeNote does; there is no clip here.
                    return {};
            }

            return {};
        }

        const Project& project;
        model::Suggestion& out;
        const std::set<std::string>& madeElsewhere;

        std::map<std::string, Resolved> declared;
        std::map<std::string, NoteState> states;
    };

    //==============================================================================
    // What every operation is made of: values held to their ranges, times turned
    // from the vocabulary's bars and beats into the seconds an edit takes, and
    // the ids of the two things a note operation names.

    bool outside (double value, double lowest, double highest)
    {
        return value < lowest - rangeTolerance || value > highest + rangeTolerance;
    }

    /** One of an operation's numeric fields, held to a range. */
    Problem takeNumber (const Json& operation,
                        std::string_view field,
                        double lowest,
                        double highest,
                        double& taken)
    {
        const auto value = number (operation, field);

        if (! value.has_value())
            return needs (field);

        if (outside (*value, lowest, highest))
            return outsideRange (field, *value, lowest, highest);

        taken = *value;

        return {};
    }

    Problem takeWhole (const Json& operation,
                       std::string_view field,
                       int lowest,
                       int highest,
                       int& taken)
    {
        const auto value = whole (operation, field);

        if (! value.has_value())
            return needs (field);

        if (*value < lowest || *value > highest)
            return outsideRange (field, *value, lowest, highest);

        taken = *value;

        return {};
    }

    /** A bar position, as the seconds the model edits in.

        Bars count from one, the way the producer counts them and the way the
        read tools report them.
    */
    double secondsAtBar (const Element& element, double bar)
    {
        return element.reads().read().secondsAtBar (bar);
    }

    /** How long a stretch of bars is in beats, measured where it starts, so a
        loop written in bars stays the loop the call asked for.
    */
    double beatsInBars (const Element& element, double fromBar, double bars)
    {
        const auto& session = element.reads().read();

        return session.beatsAtSeconds (secondsAtBar (element, fromBar + bars))
               - session.beatsAtSeconds (secondsAtBar (element, fromBar));
    }

    //==============================================================================
    /** One note of an `addNotes` or `createMidi` list, added to its clip. */
    Problem addNote (Element& element, const Json& note, const Resolved& clip)
    {
        int pitch = 0;
        int velocity = 0;
        double startBeats = 0.0;
        double lengthBeats = 0.0;

        if (auto problem = takeWhole (note, "pitch", lowestPitch, highestPitch, pitch))
            return problem;

        if (auto problem = takeNumber (note, "startBeats", 0.0, HUGE_VAL, startBeats))
            return problem;

        if (auto problem = takeNumber (note, "lengthBeats", rangeTolerance, HUGE_VAL, lengthBeats))
            return problem;

        if (auto problem =
                takeWhole (note, "velocity", quietestVelocity, loudestVelocity, velocity))
            return problem;

        const auto made =
            element.changes().addNote (clip.target, pitch, startBeats, lengthBeats, velocity);
        const auto name = text (note, "ref");

        if (auto problem = element.declareAs (name, Kind::note, made))
            return problem;

        if (name.has_value())
            element.rememberNote (*name, { pitch, startBeats });

        return {};
    }

    /** A note id, which is only ever resolved against the clip that holds it:
        a note the project holds has to be in the clip the operation names, and
        a note an earlier operation makes has to have been made in it.
    */
    Problem
        takeNote (Element& element, const std::string& id, const Resolved& clip, Resolved& resolved)
    {
        if (! id.empty() && id.front() == placeholderMark)
            return element.resolve (id, Kind::note, resolved);

        const auto ref = toolId::toNote (id);
        const auto clipRef = clip.inProject();

        if (! ref.has_value() || ! clipRef.has_value())
            return noSuchThing ("note", id);

        const auto note = element.reads().note (*clipRef, *ref);

        if (! note.has_value())
            return noSuchThing ("note", id);

        resolved = { *ref, Kind::note, false };
        element.rememberNote (id, { note->pitch, note->startBeats });

        return {};
    }

    //==============================================================================
    /** Which built-in a call is asking for, by the names the vocabulary uses. */
    std::optional<model::BuiltinPlugin> builtinCalled (const std::string& name)
    {
        if (name == "eq")
            return model::BuiltinPlugin::eq;
        if (name == "compressor")
            return model::BuiltinPlugin::compressor;
        if (name == "reverb")
            return model::BuiltinPlugin::reverb;
        if (name == "synth")
            return model::BuiltinPlugin::synth;
        if (name == "sampler")
            return model::BuiltinPlugin::sampler;

        return {};
    }

    std::optional<model::TrackKind> trackKindCalled (const std::string& name)
    {
        if (name == "midi")
            return model::TrackKind::midi;
        if (name == "audio")
            return model::TrackKind::audio;
        if (name == "group")
            return model::TrackKind::group;

        return {};
    }

    /** The two ends a parameter's value has to be inside, and nothing when the
        parameter is one nothing can be asked about.

        Duet owns what a built-in's numbers mean and holds them to the plugin's
        own range, in the real units the read side reported — for a plugin an
        earlier operation of the element adds just as much as for one the
        project already holds, since which parameters a built-in has is a fact
        about the plugin and not about an instance of it. It does not own a
        scanned plugin's mapping, so an external parameter is the normalised
        0..1 the plugin itself speaks, and a number in the wrong one of those
        two domains is refused rather than quietly converted.
    */
    std::optional<std::pair<double, double>>
        parameterRange (const Element& element, const Resolved& plugin, const std::string& paramId)
    {
        const auto ref = plugin.inProject();

        if (! ref.has_value())
        {
            // An external plugin the element adds: 0..1 is the whole of what
            // Duet can say about a mapping it does not own, and it is enough to
            // refuse a number written in some other domain.
            if (! plugin.builtin.has_value())
                return std::pair { normalisedLowest, normalisedHighest };

            const auto stated = element.reads().builtinParameter (*plugin.builtin, paramId);

            return stated.has_value()
                       ? std::optional { std::pair { stated->minValue, stated->maxValue } }
                       : std::nullopt;
        }

        const auto parameter = element.reads().parameter (*ref, paramId);

        if (! parameter.has_value())
            return {};

        const auto held = element.reads().plugin (*ref);

        if (held.has_value() && ! held->builtin.has_value())
            return std::pair { normalisedLowest, normalisedHighest };

        return std::pair { parameter->minValue, parameter->maxValue };
    }

    /** Whether a plugin declares a parameter at all.

        An external plugin an element adds is the one thing that cannot be
        asked: Duet does not ship it, so what it has is the vendor's own answer
        and there is nothing to ask until the plugin exists.
    */
    bool hasParameter (const Element& element, const Resolved& plugin, const std::string& paramId)
    {
        const auto ref = plugin.inProject();

        if (! ref.has_value())
            return ! plugin.builtin.has_value()
                   || element.reads().builtinParameter (*plugin.builtin, paramId).has_value();

        return element.reads().parameter (*ref, paramId).has_value();
    }

    /** The curve an automation operation is drawn on, and the two ends its
        values are held to.
    */
    Problem takeAutomationTarget (Element& element,
                                  const Json& operation,
                                  const Resolved& track,
                                  model::SuggestionAutomationTarget& target,
                                  std::optional<std::pair<double, double>>& range)
    {
        const auto* written = object (operation, "target");

        if (written == nullptr)
            return needs ("target");

        const auto kind = text (*written, "kind");

        if (! kind.has_value())
            return needs ("target.kind");

        if (*kind == "volume")
        {
            target = model::SuggestionAutomationTarget::trackVolumeOf (track.target);
            range = std::pair { model::faderMinimumDb, model::faderMaximumDb };

            return {};
        }

        if (*kind == "pan")
        {
            target = model::SuggestionAutomationTarget::trackPanOf (track.target);
            range = std::pair { panLeft, panRight };

            return {};
        }

        if (*kind != "pluginParam")
            return "no automation target is called " + *kind;

        Resolved plugin;

        if (auto problem = element.take (*written, "pluginId", Kind::plugin, plugin))
            return problem;

        const auto paramId = text (*written, "paramId");

        if (! paramId.has_value())
            return needs ("target.paramId");

        if (! hasParameter (element, plugin, *paramId))
            return "this plugin has no parameter called " + *paramId;

        target = model::SuggestionAutomationTarget::parameterOf (plugin.target, *paramId);
        range = parameterRange (element, plugin, *paramId);

        return {};
    }

    //==============================================================================
    // The vocabulary itself: one handler per operation, each mirroring exactly
    // one thing the Target Producer can do through the milestone-one interface.

    Problem midiAddNotes (Element& element, const Json& operation)
    {
        Resolved clip;

        if (auto problem = element.take (operation, "clipId", Kind::clip, clip))
            return problem;

        const auto* notes = list (operation, "notes");

        if (notes == nullptr || notes->empty())
            return needs ("notes");

        for (const auto& note : *notes)
            if (auto problem = addNote (element, note, clip))
                return problem;

        return {};
    }

    Problem midiRemoveNotes (Element& element, const Json& operation)
    {
        Resolved clip;

        if (auto problem = element.take (operation, "clipId", Kind::clip, clip))
            return problem;

        const auto* ids = list (operation, "noteIds");

        if (ids == nullptr || ids->empty())
            return needs ("noteIds");

        for (const auto& id : *ids)
        {
            if (! id.is_string())
                return needs ("noteIds of ids");

            Resolved note;

            if (auto problem = takeNote (element, id.get<std::string>(), clip, note))
                return problem;

            element.changes().removeNote (note.target);
        }

        return {};
    }

    /** Where a note is after a change: what the change names, over what the
        note was, because the model moves a note to a pitch and a beat together
        while a change may name only one of the two.
    */
    Problem movedNote (Element& element, const Json& change, const std::string& id, bool& moved)
    {
        auto now = element.noteState (id).value_or (NoteState {});

        if (change.contains ("pitch"))
        {
            if (auto problem = takeWhole (change, "pitch", lowestPitch, highestPitch, now.pitch))
                return problem;

            moved = true;
        }

        if (change.contains ("startBeats"))
        {
            if (auto problem = takeNumber (change, "startBeats", 0.0, HUGE_VAL, now.startBeats))
                return problem;

            moved = true;
        }

        if (moved)
            element.rememberNote (id, now);

        return {};
    }

    /** One entry of a `modifyNotes` list: whichever of a note's four things it
        names, and a refusal when it names none of them.
    */
    Problem changeNote (Element& element, const Json& change, const Resolved& clip)
    {
        const auto id = text (change, "noteId");

        if (! id.has_value())
            return needs ("noteId");

        Resolved note;

        if (auto problem = takeNote (element, *id, clip, note))
            return problem;

        auto moved = false;

        if (auto problem = movedNote (element, change, *id, moved))
            return problem;

        auto changed = moved;

        if (moved)
        {
            const auto now = element.noteState (*id).value_or (NoteState {});
            element.changes().moveNote (note.target, now.pitch, now.startBeats);
        }

        if (change.contains ("lengthBeats"))
        {
            double lengthBeats = 0.0;

            if (auto problem =
                    takeNumber (change, "lengthBeats", rangeTolerance, HUGE_VAL, lengthBeats))
                return problem;

            element.changes().resizeNote (note.target, lengthBeats);
            changed = true;
        }

        if (change.contains ("velocity"))
        {
            int velocity = 0;

            if (auto problem =
                    takeWhole (change, "velocity", quietestVelocity, loudestVelocity, velocity))
                return problem;

            element.changes().setNoteVelocity (note.target, velocity);
            changed = true;
        }

        return changed ? Problem {} : Problem { "this change changes nothing about " + *id };
    }

    Problem midiModifyNotes (Element& element, const Json& operation)
    {
        Resolved clip;

        if (auto problem = element.take (operation, "clipId", Kind::clip, clip))
            return problem;

        const auto* changes = list (operation, "changes");

        if (changes == nullptr || changes->empty())
            return needs ("changes");

        for (const auto& change : *changes)
            if (auto problem = changeNote (element, change, clip))
                return problem;

        return {};
    }

    Problem clipCreateMidi (Element& element, const Json& operation)
    {
        Resolved track;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        if (const auto ref = track.inProject())
            if (*ref != model::masterChannel
                && element.reads().read().track (*ref).kind != model::TrackKind::midi)
                return "a MIDI clip goes on a MIDI track, and this one is not";

        double startBar = 1.0;
        double lengthBars = 0.0;

        if (auto problem = takeNumber (operation, "startBar", 1.0, HUGE_VAL, startBar))
            return problem;

        if (auto problem =
                takeNumber (operation, "lengthBars", rangeTolerance, HUGE_VAL, lengthBars))
            return problem;

        const auto start = secondsAtBar (element, startBar);
        const auto made = element.changes().insertMidiClip (
            track.target,
            text (operation, "name").value_or (std::string {}),
            start,
            secondsAtBar (element, startBar + lengthBars) - start);

        if (auto problem = element.declare (operation, Kind::clip, made))
            return problem;

        const Resolved clip { made, Kind::clip, true };

        if (const auto* notes = list (operation, "notes"))
            for (const auto& note : *notes)
                if (auto problem = addNote (element, note, clip))
                    return problem;

        return {};
    }

    Problem clipDelete (Element& element, const Json& operation)
    {
        Resolved clip;

        if (auto problem = element.take (operation, "clipId", Kind::clip, clip))
            return problem;

        element.changes().deleteClip (clip.target);

        return {};
    }

    Problem clipMove (Element& element, const Json& operation)
    {
        Resolved clip;

        if (auto problem = element.take (operation, "clipId", Kind::clip, clip))
            return problem;

        double startBar = 1.0;

        if (auto problem = takeNumber (operation, "startBar", 1.0, HUGE_VAL, startBar))
            return problem;

        const auto start = secondsAtBar (element, startBar);

        if (! operation.contains ("trackId"))
        {
            element.changes().moveClip (clip.target, start);

            return {};
        }

        Resolved track;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        element.changes().moveClip (clip.target, track.target, start);

        return {};
    }

    Problem clipTrim (Element& element, const Json& operation)
    {
        Resolved clip;

        if (auto problem = element.take (operation, "clipId", Kind::clip, clip))
            return problem;

        double startBar = 1.0;
        double lengthBars = 0.0;

        if (auto problem = takeNumber (operation, "startBar", 1.0, HUGE_VAL, startBar))
            return problem;

        if (auto problem =
                takeNumber (operation, "lengthBars", rangeTolerance, HUGE_VAL, lengthBars))
            return problem;

        const auto start = secondsAtBar (element, startBar);

        element.changes().trimClip (
            clip.target, start, secondsAtBar (element, startBar + lengthBars) - start);

        return {};
    }

    Problem clipSetLoop (Element& element, const Json& operation)
    {
        Resolved clip;

        if (auto problem = element.take (operation, "clipId", Kind::clip, clip))
            return problem;

        const auto looped = flag (operation, "looped");

        if (! looped.has_value())
            return needs ("looped");

        if (! *looped)
        {
            element.changes().setClipLoop (clip.target, false, 0.0);

            return {};
        }

        double loopLengthBars = 0.0;

        if (auto problem =
                takeNumber (operation, "loopLengthBars", rangeTolerance, HUGE_VAL, loopLengthBars))
            return problem;

        element.changes().setClipLoop (
            clip.target, true, beatsInBars (element, 1.0, loopLengthBars));

        return {};
    }

    Problem clipDuplicate (Element& element, const Json& operation)
    {
        Resolved clip;

        if (auto problem = element.take (operation, "clipId", Kind::clip, clip))
            return problem;

        double atBar = 1.0;

        if (auto problem = takeNumber (operation, "atBar", 1.0, HUGE_VAL, atBar))
            return problem;

        Resolved track;

        if (operation.contains ("toTrackId"))
        {
            if (auto problem = element.take (operation, "toTrackId", Kind::track, track))
                return problem;
        }
        else
        {
            const auto ref = clip.inProject();
            const auto onto = ref.has_value() ? element.reads().trackOfClip (*ref) : std::nullopt;

            if (! onto.has_value())
                return "a copy of a clip this element makes needs a toTrackId";

            track = { *onto, Kind::track, false };
        }

        const auto made = element.changes().duplicateClip (
            clip.target, track.target, secondsAtBar (element, atBar));

        return element.declare (operation, Kind::clip, made);
    }

    Problem trackCreate (Element& element, const Json& operation)
    {
        const auto kind = text (operation, "kind");

        if (! kind.has_value())
            return needs ("kind");

        const auto trackKind = trackKindCalled (*kind);

        if (! trackKind.has_value())
            return "no kind of track is called " + *kind;

        const auto name = text (operation, "name");

        if (! name.has_value())
            return needs ("name");

        std::optional<model::BuiltinPlugin> instrument;

        if (const auto called = text (operation, "instrument"))
        {
            instrument = builtinCalled (*called);

            if (! instrument.has_value()
                || (*instrument != model::BuiltinPlugin::synth
                    && *instrument != model::BuiltinPlugin::sampler))
                return "no built-in instrument is called " + *called;

            if (*trackKind != model::TrackKind::midi)
                return "an instrument goes on a MIDI track, and this one is a " + *kind;
        }

        return element.declare (
            operation, Kind::track, element.changes().createTrack (*trackKind, *name, instrument));
    }

    Problem trackRename (Element& element, const Json& operation)
    {
        Resolved track;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        const auto name = text (operation, "name");

        if (! name.has_value())
            return needs ("name");

        element.changes().renameTrack (track.target, *name);

        return {};
    }

    Problem trackDelete (Element& element, const Json& operation)
    {
        Resolved track;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        if (track.inProject() == model::masterChannel)
            return "the master is not a track anything can delete";

        element.changes().removeTrack (track.target);

        return {};
    }

    Problem trackSetOutput (Element& element, const Json& operation)
    {
        Resolved track;
        Resolved bus;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        if (auto problem = element.take (operation, "busId", Kind::track, bus))
            return problem;

        if (track.inProject().has_value() && track.inProject() == bus.inProject())
            return "a track cannot be routed into itself";

        // The read side says the master where a track has no bus of its own, so
        // the write side takes the master back to meaning exactly that.
        element.changes().setTrackOutput (track.target,
                                          bus.inProject() == model::masterChannel
                                              ? model::SuggestionTarget { model::noTrack }
                                              : bus.target);

        return {};
    }

    Problem mixerSet (Element& element, const Json& operation)
    {
        Resolved track;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        auto set = false;

        if (operation.contains ("volumeDb"))
        {
            double volumeDb = 0.0;

            if (auto problem = takeNumber (
                    operation, "volumeDb", model::faderMinimumDb, model::faderMaximumDb, volumeDb))
                return problem;

            element.changes().setTrackVolumeDb (track.target, volumeDb);
            set = true;
        }

        if (operation.contains ("pan"))
        {
            double pan = 0.0;

            if (auto problem = takeNumber (operation, "pan", panLeft, panRight, pan))
                return problem;

            element.changes().setTrackPan (track.target, pan);
            set = true;
        }

        if (operation.contains ("mute"))
        {
            const auto muted = flag (operation, "mute");

            if (! muted.has_value())
                return needs ("mute that is true or false");

            element.changes().setTrackMute (track.target, *muted);
            set = true;
        }

        if (operation.contains ("solo"))
        {
            const auto soloed = flag (operation, "solo");

            if (! soloed.has_value())
                return needs ("solo that is true or false");

            if (track.inProject() == model::masterChannel)
                return "the master is what everything is soloed against, and cannot be soloed";

            element.changes().setTrackSolo (track.target, *soloed);
            set = true;
        }

        if (! set)
            return "this operation sets nothing: it needs a volumeDb, a pan, a mute or a solo";

        return {};
    }

    Problem mixerSetSend (Element& element, const Json& operation)
    {
        Resolved track;
        Resolved bus;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        if (auto problem = element.take (operation, "busId", Kind::track, bus))
            return problem;

        if (track.inProject().has_value() && track.inProject() == bus.inProject())
            return "a track cannot send to itself";

        double levelDb = 0.0;

        if (auto problem = takeNumber (
                operation, "levelDb", model::faderMinimumDb, model::faderMaximumDb, levelDb))
            return problem;

        element.changes().setSend (track.target, bus.target, levelDb);

        return {};
    }

    Problem pluginAdd (Element& element, const Json& operation)
    {
        Resolved track;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        int position = 0;

        if (auto problem = takeWhole (operation, "position", 0, largestPosition, position))
            return problem;

        const auto* which = object (operation, "plugin");

        if (which == nullptr)
            return needs ("plugin");

        if (const auto builtin = text (*which, "builtin"))
        {
            const auto plugin = builtinCalled (*builtin);

            if (! plugin.has_value())
                return "no built-in plugin is called " + *builtin;

            return element.declare (operation,
                                    Kind::plugin,
                                    element.changes().addPlugin (track.target, *plugin, position),
                                    plugin);
        }

        const auto external = text (*which, "external");

        if (! external.has_value())
            return needs ("plugin that names a builtin or an external");

        if (! element.reads().knowsExternal (*external))
            return "this machine has no plugin called " + *external;

        return element.declare (operation,
                                Kind::plugin,
                                element.changes().addPlugin (track.target, *external, position));
    }

    Problem pluginRemove (Element& element, const Json& operation)
    {
        Resolved plugin;

        if (auto problem = element.take (operation, "pluginId", Kind::plugin, plugin))
            return problem;

        element.changes().removePlugin (plugin.target);

        return {};
    }

    Problem pluginReorder (Element& element, const Json& operation)
    {
        Resolved track;
        Resolved plugin;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        if (auto problem = element.take (operation, "pluginId", Kind::plugin, plugin))
            return problem;

        if (const auto ref = plugin.inProject())
            if (const auto onto = track.inProject())
                if (element.reads().trackOf (*ref) != onto)
                    return "this plugin is not in that track's chain";

        int position = 0;

        if (auto problem = takeWhole (operation, "position", 0, largestPosition, position))
            return problem;

        element.changes().reorderPlugin (plugin.target, position);

        return {};
    }

    Problem pluginSetParam (Element& element, const Json& operation)
    {
        Resolved plugin;

        if (auto problem = element.take (operation, "pluginId", Kind::plugin, plugin))
            return problem;

        const auto paramId = text (operation, "paramId");

        if (! paramId.has_value())
            return needs ("paramId");

        if (! hasParameter (element, plugin, *paramId))
            return "this plugin has no parameter called " + *paramId;

        const auto range = parameterRange (element, plugin, *paramId);
        double value = 0.0;

        if (auto problem = takeNumber (operation,
                                       "value",
                                       range.has_value() ? range->first : -HUGE_VAL,
                                       range.has_value() ? range->second : HUGE_VAL,
                                       value))
            return problem;

        element.changes().setPluginParameter (plugin.target, *paramId, value);

        return {};
    }

    Problem pluginSetSidechainSource (Element& element, const Json& operation)
    {
        Resolved plugin;
        Resolved source;

        if (auto problem = element.take (operation, "pluginId", Kind::plugin, plugin))
            return problem;

        if (auto problem = element.take (operation, "sourceTrackId", Kind::track, source))
            return problem;

        element.changes().setPluginSidechainSource (plugin.target, source.target);

        return {};
    }

    Problem automationSetPoints (Element& element, const Json& operation)
    {
        Resolved track;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        model::SuggestionAutomationTarget target;
        std::optional<std::pair<double, double>> range;

        if (auto problem = takeAutomationTarget (element, operation, track, target, range))
            return problem;

        const auto* written = list (operation, "points");

        if (written == nullptr || written->empty())
            return needs ("points");

        std::vector<model::AutomationPoint> points;

        for (const auto& point : *written)
        {
            double timeBeats = 0.0;
            double value = 0.0;

            if (auto problem = takeNumber (point, "timeBeats", 0.0, HUGE_VAL, timeBeats))
                return problem;

            if (auto problem = takeNumber (point,
                                           "value",
                                           range.has_value() ? range->first : -HUGE_VAL,
                                           range.has_value() ? range->second : HUGE_VAL,
                                           value))
                return problem;

            points.push_back ({ element.reads().read().secondsAtBeats (timeBeats), value, 0.0 });
        }

        element.changes().setAutomationPoints (target, std::move (points));

        return {};
    }

    Problem automationRemovePoints (Element& element, const Json& operation)
    {
        Resolved track;

        if (auto problem = element.take (operation, "trackId", Kind::track, track))
            return problem;

        model::SuggestionAutomationTarget target;
        std::optional<std::pair<double, double>> range;

        if (auto problem = takeAutomationTarget (element, operation, track, target, range))
            return problem;

        const auto* written = list (operation, "range");

        if (written == nullptr || written->size() != 2 || ! written->front().is_number()
            || ! written->back().is_number())
            return needs ("range of two times in beats");

        const auto from = written->front().get<double>();
        const auto to = written->back().get<double>();

        if (! std::isfinite (from) || ! std::isfinite (to) || from < 0.0 || to < from)
            return "a range runs from a beat to a later one, and this one does not";

        const auto& session = element.reads().read();

        element.changes().removeAutomationPoints (
            target, session.secondsAtBeats (from), session.secondsAtBeats (to));

        return {};
    }

    Problem projectSetTempo (Element& element, const Json& operation)
    {
        double bpm = 0.0;

        if (auto problem = takeNumber (operation, "bpm", rangeTolerance, HUGE_VAL, bpm))
            return problem;

        element.changes().setTempo (bpm);

        return {};
    }

    Problem projectSetTimeSignature (Element& element, const Json& operation)
    {
        int numerator = 0;
        int denominator = 0;

        if (auto problem = takeWhole (operation, "numerator", 1, largestCount, numerator))
            return problem;

        if (auto problem = takeWhole (operation, "denominator", 1, largestCount, denominator))
            return problem;

        element.changes().setTimeSignature (numerator, denominator);

        return {};
    }

    //==============================================================================
    /** The closed set the edit vocabulary is.

        Every entry mirrors one thing the Target Producer can do through the
        milestone-one interface, which is the closure principle: a Suggestion is
        something the producer could have made by hand. Nothing here brings
        audio into being — an audio clip can be moved, trimmed, looped,
        duplicated and deleted, and never created — and a name that is not in
        this list is refused rather than guessed at.
    */
    Problem applyOperation (Element& element, const Json& operation)
    {
        const auto name = text (operation, "op");

        if (! name.has_value())
            return "an operation says which one it is in its op";

        if (*name == "midi.addNotes")
            return midiAddNotes (element, operation);
        if (*name == "midi.removeNotes")
            return midiRemoveNotes (element, operation);
        if (*name == "midi.modifyNotes")
            return midiModifyNotes (element, operation);
        if (*name == "clip.createMidi")
            return clipCreateMidi (element, operation);
        if (*name == "clip.delete")
            return clipDelete (element, operation);
        if (*name == "clip.move")
            return clipMove (element, operation);
        if (*name == "clip.trim")
            return clipTrim (element, operation);
        if (*name == "clip.setLoop")
            return clipSetLoop (element, operation);
        if (*name == "clip.duplicate")
            return clipDuplicate (element, operation);
        if (*name == "track.create")
            return trackCreate (element, operation);
        if (*name == "track.rename")
            return trackRename (element, operation);
        if (*name == "track.delete")
            return trackDelete (element, operation);
        if (*name == "track.setOutput")
            return trackSetOutput (element, operation);
        if (*name == "mixer.set")
            return mixerSet (element, operation);
        if (*name == "mixer.setSend")
            return mixerSetSend (element, operation);
        if (*name == "plugin.add")
            return pluginAdd (element, operation);
        if (*name == "plugin.remove")
            return pluginRemove (element, operation);
        if (*name == "plugin.reorder")
            return pluginReorder (element, operation);
        if (*name == "plugin.setParam")
            return pluginSetParam (element, operation);
        if (*name == "plugin.setSidechainSource")
            return pluginSetSidechainSource (element, operation);
        if (*name == "automation.setPoints")
            return automationSetPoints (element, operation);
        if (*name == "automation.removePoints")
            return automationRemovePoints (element, operation);
        if (*name == "project.setTempo")
            return projectSetTempo (element, operation);
        if (*name == "project.setTimeSignature")
            return projectSetTimeSignature (element, operation);

        return "no operation is called " + *name;
    }

    //==============================================================================
    /** Turns one element of a call into the model's own operation list, or says
        why it is not one.
    */
    RpcOutcome buildElement (const Project& project,
                             const Json& entry,
                             std::size_t index,
                             const std::set<std::string>& madeElsewhere,
                             SuggestionElement& made)
    {
        const auto* operations = list (entry, "operations");

        if (operations == nullptr || operations->empty())
            return RpcOutcome::failure (rpcError::invalidParams,
                                        "elements[" + std::to_string (index)
                                            + "] needs operations");

        Element element { project, made.changes, madeElsewhere };

        for (std::size_t at = 0; at < operations->size(); ++at)
        {
            const auto& operation = operations->at (at);

            if (! operation.is_object())
                return RpcOutcome::failure (rpcError::invalidParams,
                                            positionOf (index, at) + " is not an operation");

            if (const auto problem = applyOperation (element, operation))
            {
                const auto called = text (operation, "op");

                return RpcOutcome::failure (
                    rpcError::invalidParams,
                    positionOf (index, at)
                        + (called.has_value() ? " (" + *called + ")" : std::string {}) + ": "
                        + *problem);
            }

            made.operations.push_back (operation);
        }

        return RpcOutcome::success (Json::object());
    }

    //==============================================================================
    /** Turns one call into a Suggestion, or says why it is not one.

        Everything is checked before anything is kept: an element that fails its
        last operation leaves nothing behind, so a run can correct the call and
        make it again without having half a Suggestion in the way.
    */
    RpcOutcome build (const Project& project, const Json& elements, Suggestion& suggestion)
    {
        // What the elements before this one declared, which is what makes an
        // element that leans on another element's creation refusable by name.
        std::set<std::string> madeElsewhere;

        for (std::size_t index = 0; index < elements.size(); ++index)
        {
            const auto& entry = elements.at (index);
            const auto description = entry.is_object() ? text (entry, "description") : std::nullopt;

            if (! description.has_value())
                return RpcOutcome::failure (rpcError::invalidParams,
                                            "elements[" + std::to_string (index)
                                                + "] needs a description");

            SuggestionElement made { *description };
            const auto outcome = buildElement (project, entry, index, madeElsewhere, made);

            if (! outcome.succeeded)
                return outcome;

            // The same walk again, into the list that accepting the whole
            // Suggestion applies: an element's placeholders are its own, so the
            // two lists resolve them separately and neither can see the other's.
            Element together { project, suggestion.changes, madeElsewhere };

            for (const auto& operation : made.operations)
                applyOperation (together, operation);

            for (const auto& declared : together.refs())
                madeElsewhere.insert (declared.first);

            suggestion.elements.push_back (std::move (made));
        }

        return RpcOutcome::success (Json::object());
    }
} // namespace

//==============================================================================
class SuggestTool::Impl
{
public:
    Impl (model::Session& projectSession,
          ProjectReadMarshal readMarshal,
          const EstimateLedger* estimateLedger)
        : session (projectSession), marshal (std::move (readMarshal)), ledger (estimateLedger)
    {
    }

    RpcOutcome suggest (const ToolCall& call)
    {
        const auto summary = text (call.arguments, "summary");

        if (! summary.has_value())
            return RpcOutcome::failure (rpcError::invalidParams, "a Suggestion needs a summary");

        const auto* elements = list (call.arguments, "elements");

        if (elements == nullptr || elements->empty())
            return RpcOutcome::failure (rpcError::invalidParams, "a Suggestion needs elements");

        {
            const std::lock_guard lock (mutex);

            if (runsThatSuggested.contains (call.runId))
                return RpcOutcome::failure (rpcError::suggestionAlreadyMade,
                                            "this task run has already made a Suggestion, and a "
                                            "run makes at most one");
        }

        Suggestion suggestion { std::string {}, *summary };

        // The mark is the run's ledger read here, at the moment the Suggestion
        // is made, and not the model's account of what it leant on.
        suggestion.basedOnEstimates = ledger != nullptr && ledger->basedOnEstimates (call.runId);

        auto outcome = RpcOutcome::failure (rpcError::internalError, "the project was never read");

        marshal (
            [&]
            {
                const Project project { session };
                outcome = build (project, *elements, suggestion);
            });

        if (! outcome.succeeded)
            return outcome;

        Json result = Json::object();

        {
            const std::lock_guard lock (mutex);

            suggestion.id = "suggestion-" + std::to_string (++suggestionsMade);
            result["suggestionId"] = suggestion.id;
            runsThatSuggested.insert (call.runId);
            made.push_back (std::move (suggestion));
        }

        return RpcOutcome::success (result);
    }

    [[nodiscard]] std::vector<Suggestion> suggestions() const
    {
        const std::lock_guard lock (mutex);

        return made;
    }

    [[nodiscard]] std::optional<Suggestion> suggestion (std::string_view id) const
    {
        const std::lock_guard lock (mutex);

        for (const auto& suggestion : made)
            if (suggestion.id == id)
                return suggestion;

        return {};
    }

private:
    model::Session& session;
    ProjectReadMarshal marshal;
    const EstimateLedger* ledger = nullptr;

    mutable std::mutex mutex;
    std::vector<Suggestion> made;
    std::set<std::string> runsThatSuggested;

    /** How many Suggestions have been made, which is what the next one is
        named after: a Suggestion has an id once it exists, so a refused call
        leaves no gap in the count and a trace reads as it happened.
    */
    std::uint64_t suggestionsMade = 0;
};

//==============================================================================
SuggestTool::SuggestTool (model::Session& projectSession,
                          ProjectReadMarshal readMarshal,
                          const EstimateLedger* estimateLedger)
    : impl (std::make_unique<Impl> (projectSession, std::move (readMarshal), estimateLedger))
{
}

SuggestTool::~SuggestTool() = default;

void SuggestTool::addTo (ToolRegistry& registry)
{
    registry.add ("suggest", [this] (const ToolCall& call) { return impl->suggest (call); });
}

std::vector<Suggestion> SuggestTool::suggestions() const { return impl->suggestions(); }

std::optional<Suggestion> SuggestTool::suggestion (std::string_view id) const
{
    return impl->suggestion (id);
}
} // namespace duet::collab
