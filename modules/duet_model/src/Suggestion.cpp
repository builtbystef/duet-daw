#include "SessionImpl.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace duet::model
{
struct Suggestion::Impl
{
    struct Reference
    {
        std::uint64_t value = 0;
        bool isPlaceholder = false;
    };

    struct Operation
    {
        enum class Kind : std::uint8_t
        {
            createTrack,
            duplicateTrack,
            removeTrack,
            renameTrack,
            moveTrack,
            setTrackOutput,
            insertAudioClip,
            insertMidiClip,
            moveClip,
            moveClipToTrack,
            trimClip,
            trimClipEdges,
            deleteClip,
            setClipLoop,
            duplicateClip,
            addNote,
            removeNote,
            moveNote,
            resizeNote,
            setNoteVelocity,
            setTrackVolumeDb,
            setTrackPan,
            setTrackMute,
            setTrackSolo,
            setTrackColour,
            setSend,
            addBuiltinPlugin,
            addExternalPlugin,
            removePlugin,
            reorderPlugin,
            setPluginParameter,
            setPluginSidechainSource,
            setAutomationPoints,
            removeAutomationPoints,
            setTempo,
            setTimeSignature
        };

        Kind kind = Kind::createTrack;
        Reference first;
        Reference second;
        SuggestionRef result;
        TrackKind trackKind = TrackKind::audio;
        TrackColour colour = TrackColour::orange;
        std::optional<BuiltinPlugin> builtin;
        AutomationTarget::Kind automationKind = AutomationTarget::Kind::trackVolume;
        std::string text;
        std::filesystem::path path;
        std::vector<AutomationPoint> points;
        double a = 0.0;
        double b = 0.0;
        int firstInt = 0;
        int secondInt = 0;
        bool flag = false;
    };

    explicit Impl (std::string suggestionName) : name (std::move (suggestionName)) {}

    SuggestionRef nextPlaceholder() { return { nextPlaceholderValue++ }; }

    static Reference referenceOf (const SuggestionTarget& target)
    {
        if (const auto* placeholder = std::get_if<SuggestionRef> (&target))
            return { placeholder->value, true };

        return { std::get<std::uint64_t> (target), false };
    }

    Operation& append (Operation::Kind kind)
    {
        operations.push_back ({});
        operations.back().kind = kind;
        return operations.back();
    }

    std::string name;
    std::vector<Operation> operations;
    std::uint64_t nextPlaceholderValue = 1;
};

SuggestionAutomationTarget SuggestionAutomationTarget::trackVolumeOf (SuggestionTarget track)
{
    return { AutomationTarget::Kind::trackVolume, track, {} };
}

SuggestionAutomationTarget SuggestionAutomationTarget::trackPanOf (SuggestionTarget track)
{
    return { AutomationTarget::Kind::trackPan, track, {} };
}

SuggestionAutomationTarget SuggestionAutomationTarget::parameterOf (SuggestionTarget plugin,
                                                                    std::string_view parameterId)
{
    return { AutomationTarget::Kind::pluginParameter, plugin, std::string { parameterId } };
}

Suggestion::Suggestion (std::string name) : impl (std::make_unique<Impl> (std::move (name))) {}
Suggestion::~Suggestion() = default;
Suggestion::Suggestion (const Suggestion& other) : impl (std::make_unique<Impl> (*other.impl)) {}

Suggestion& Suggestion::operator= (const Suggestion& other)
{
    if (this != &other)
        impl = std::make_unique<Impl> (*other.impl);

    return *this;
}

Suggestion::Suggestion (Suggestion&&) noexcept = default;
Suggestion& Suggestion::operator= (Suggestion&&) noexcept = default;

const std::string& Suggestion::name() const { return impl->name; }

SuggestionRef Suggestion::createTrack (TrackKind kind,
                                       std::string_view name,
                                       std::optional<BuiltinPlugin> instrument)
{
    auto& operation = impl->append (Impl::Operation::Kind::createTrack);
    operation.result = impl->nextPlaceholder();
    operation.trackKind = kind;
    operation.builtin = instrument;
    operation.text = name;
    return operation.result;
}

SuggestionRef Suggestion::duplicateTrack (SuggestionTarget track)
{
    auto& operation = impl->append (Impl::Operation::Kind::duplicateTrack);
    operation.first = Impl::referenceOf (track);
    operation.result = impl->nextPlaceholder();
    return operation.result;
}

void Suggestion::removeTrack (SuggestionTarget track)
{
    impl->append (Impl::Operation::Kind::removeTrack).first = Impl::referenceOf (track);
}

void Suggestion::renameTrack (SuggestionTarget track, std::string_view newName)
{
    auto& operation = impl->append (Impl::Operation::Kind::renameTrack);
    operation.first = Impl::referenceOf (track);
    operation.text = newName;
}

void Suggestion::moveTrack (SuggestionTarget track, int newIndex)
{
    auto& operation = impl->append (Impl::Operation::Kind::moveTrack);
    operation.first = Impl::referenceOf (track);
    operation.firstInt = newIndex;
}

void Suggestion::setTrackOutput (SuggestionTarget track, SuggestionTarget bus)
{
    auto& operation = impl->append (Impl::Operation::Kind::setTrackOutput);
    operation.first = Impl::referenceOf (track);
    operation.second = Impl::referenceOf (bus);
}

SuggestionRef Suggestion::insertAudioClip (SuggestionTarget track,
                                           std::string_view name,
                                           std::filesystem::path sourceFile,
                                           double startSeconds,
                                           double lengthSeconds)
{
    auto& operation = impl->append (Impl::Operation::Kind::insertAudioClip);
    operation.first = Impl::referenceOf (track);
    operation.result = impl->nextPlaceholder();
    operation.text = name;
    operation.path = std::move (sourceFile);
    operation.a = startSeconds;
    operation.b = lengthSeconds;
    return operation.result;
}

SuggestionRef Suggestion::insertMidiClip (SuggestionTarget track,
                                          std::string_view name,
                                          double startSeconds,
                                          double lengthSeconds)
{
    auto& operation = impl->append (Impl::Operation::Kind::insertMidiClip);
    operation.first = Impl::referenceOf (track);
    operation.result = impl->nextPlaceholder();
    operation.text = name;
    operation.a = startSeconds;
    operation.b = lengthSeconds;
    return operation.result;
}

void Suggestion::moveClip (SuggestionTarget clip, double newStartSeconds)
{
    auto& operation = impl->append (Impl::Operation::Kind::moveClip);
    operation.first = Impl::referenceOf (clip);
    operation.a = newStartSeconds;
}

void Suggestion::moveClip (SuggestionTarget clip, SuggestionTarget toTrack, double newStartSeconds)
{
    auto& operation = impl->append (Impl::Operation::Kind::moveClipToTrack);
    operation.first = Impl::referenceOf (clip);
    operation.second = Impl::referenceOf (toTrack);
    operation.a = newStartSeconds;
}

void Suggestion::trimClip (SuggestionTarget clip, double newLengthSeconds)
{
    auto& operation = impl->append (Impl::Operation::Kind::trimClip);
    operation.first = Impl::referenceOf (clip);
    operation.a = newLengthSeconds;
}

void Suggestion::trimClip (SuggestionTarget clip, double newStartSeconds, double newLengthSeconds)
{
    auto& operation = impl->append (Impl::Operation::Kind::trimClipEdges);
    operation.first = Impl::referenceOf (clip);
    operation.a = newStartSeconds;
    operation.b = newLengthSeconds;
}

void Suggestion::deleteClip (SuggestionTarget clip)
{
    impl->append (Impl::Operation::Kind::deleteClip).first = Impl::referenceOf (clip);
}

void Suggestion::setClipLoop (SuggestionTarget clip, bool looped, double loopLengthBeats)
{
    auto& operation = impl->append (Impl::Operation::Kind::setClipLoop);
    operation.first = Impl::referenceOf (clip);
    operation.flag = looped;
    operation.a = loopLengthBeats;
}

SuggestionRef
    Suggestion::duplicateClip (SuggestionTarget clip, SuggestionTarget toTrack, double startSeconds)
{
    auto& operation = impl->append (Impl::Operation::Kind::duplicateClip);
    operation.first = Impl::referenceOf (clip);
    operation.second = Impl::referenceOf (toTrack);
    operation.result = impl->nextPlaceholder();
    operation.a = startSeconds;
    return operation.result;
}

SuggestionRef Suggestion::addNote (SuggestionTarget clip,
                                   int pitch,
                                   double startBeats,
                                   double lengthBeats,
                                   int velocity)
{
    auto& operation = impl->append (Impl::Operation::Kind::addNote);
    operation.first = Impl::referenceOf (clip);
    operation.result = impl->nextPlaceholder();
    operation.firstInt = pitch;
    operation.secondInt = velocity;
    operation.a = startBeats;
    operation.b = lengthBeats;
    return operation.result;
}

void Suggestion::removeNote (SuggestionTarget note)
{
    impl->append (Impl::Operation::Kind::removeNote).first = Impl::referenceOf (note);
}

void Suggestion::moveNote (SuggestionTarget note, int newPitch, double newStartBeats)
{
    auto& operation = impl->append (Impl::Operation::Kind::moveNote);
    operation.first = Impl::referenceOf (note);
    operation.firstInt = newPitch;
    operation.a = newStartBeats;
}

void Suggestion::resizeNote (SuggestionTarget note, double newLengthBeats)
{
    auto& operation = impl->append (Impl::Operation::Kind::resizeNote);
    operation.first = Impl::referenceOf (note);
    operation.a = newLengthBeats;
}

void Suggestion::setNoteVelocity (SuggestionTarget note, int velocity)
{
    auto& operation = impl->append (Impl::Operation::Kind::setNoteVelocity);
    operation.first = Impl::referenceOf (note);
    operation.firstInt = velocity;
}

void Suggestion::setTrackVolumeDb (SuggestionTarget track, double db)
{
    auto& operation = impl->append (Impl::Operation::Kind::setTrackVolumeDb);
    operation.first = Impl::referenceOf (track);
    operation.a = db;
}

void Suggestion::setTrackPan (SuggestionTarget track, double pan)
{
    auto& operation = impl->append (Impl::Operation::Kind::setTrackPan);
    operation.first = Impl::referenceOf (track);
    operation.a = pan;
}

void Suggestion::setTrackMute (SuggestionTarget track, bool muted)
{
    auto& operation = impl->append (Impl::Operation::Kind::setTrackMute);
    operation.first = Impl::referenceOf (track);
    operation.flag = muted;
}

void Suggestion::setTrackSolo (SuggestionTarget track, bool soloed)
{
    auto& operation = impl->append (Impl::Operation::Kind::setTrackSolo);
    operation.first = Impl::referenceOf (track);
    operation.flag = soloed;
}

void Suggestion::setTrackColour (SuggestionTarget track, TrackColour colour)
{
    auto& operation = impl->append (Impl::Operation::Kind::setTrackColour);
    operation.first = Impl::referenceOf (track);
    operation.colour = colour;
}

void Suggestion::setSend (SuggestionTarget track, SuggestionTarget bus, double levelDb)
{
    auto& operation = impl->append (Impl::Operation::Kind::setSend);
    operation.first = Impl::referenceOf (track);
    operation.second = Impl::referenceOf (bus);
    operation.a = levelDb;
}

SuggestionRef Suggestion::addPlugin (SuggestionTarget track, BuiltinPlugin plugin, int position)
{
    auto& operation = impl->append (Impl::Operation::Kind::addBuiltinPlugin);
    operation.first = Impl::referenceOf (track);
    operation.result = impl->nextPlaceholder();
    operation.builtin = plugin;
    operation.firstInt = position;
    return operation.result;
}

SuggestionRef Suggestion::addPlugin (SuggestionTarget track,
                                     std::string_view knownPluginIdentifier,
                                     int position)
{
    auto& operation = impl->append (Impl::Operation::Kind::addExternalPlugin);
    operation.first = Impl::referenceOf (track);
    operation.result = impl->nextPlaceholder();
    operation.text = knownPluginIdentifier;
    operation.firstInt = position;
    return operation.result;
}

void Suggestion::removePlugin (SuggestionTarget plugin)
{
    impl->append (Impl::Operation::Kind::removePlugin).first = Impl::referenceOf (plugin);
}

void Suggestion::reorderPlugin (SuggestionTarget plugin, int newPosition)
{
    auto& operation = impl->append (Impl::Operation::Kind::reorderPlugin);
    operation.first = Impl::referenceOf (plugin);
    operation.firstInt = newPosition;
}

void Suggestion::setPluginParameter (SuggestionTarget plugin,
                                     std::string_view parameterId,
                                     double value)
{
    auto& operation = impl->append (Impl::Operation::Kind::setPluginParameter);
    operation.first = Impl::referenceOf (plugin);
    operation.text = parameterId;
    operation.a = value;
}

void Suggestion::setPluginSidechainSource (SuggestionTarget plugin, SuggestionTarget source)
{
    auto& operation = impl->append (Impl::Operation::Kind::setPluginSidechainSource);
    operation.first = Impl::referenceOf (plugin);
    operation.second = Impl::referenceOf (source);
}

void Suggestion::setAutomationPoints (SuggestionAutomationTarget target,
                                      std::vector<AutomationPoint> points)
{
    auto& operation = impl->append (Impl::Operation::Kind::setAutomationPoints);
    operation.first = Impl::referenceOf (target.item);
    operation.automationKind = target.kind;
    operation.text = std::move (target.parameterId);
    operation.points = std::move (points);
}

void Suggestion::removeAutomationPoints (SuggestionAutomationTarget target,
                                         double fromSeconds,
                                         double toSeconds)
{
    auto& operation = impl->append (Impl::Operation::Kind::removeAutomationPoints);
    operation.first = Impl::referenceOf (target.item);
    operation.automationKind = target.kind;
    operation.text = std::move (target.parameterId);
    operation.a = fromSeconds;
    operation.b = toSeconds;
}

void Suggestion::append (const Suggestion& other)
{
    // A list appended to itself would be read while it grew, and doubling a
    // Suggestion is not something anything asks for.
    if (&other == this)
        return;

    // Placeholders are positions in a resolution map built afresh at apply
    // time, so two lists written apart both start at one. Shifting the appended
    // list past everything this one has ever handed out is what keeps the two
    // sets of creations distinct.
    const auto offset = impl->nextPlaceholderValue - 1;

    const auto renumber = [offset] (Impl::Reference& reference)
    {
        if (reference.isPlaceholder)
            reference.value += offset;
    };

    for (auto operation : other.impl->operations)
    {
        renumber (operation.first);
        renumber (operation.second);

        if (operation.result.value != 0)
            operation.result.value += offset;

        impl->operations.push_back (std::move (operation));
    }

    impl->nextPlaceholderValue += other.impl->nextPlaceholderValue - 1;
}

void Suggestion::setTempo (double bpm) { impl->append (Impl::Operation::Kind::setTempo).a = bpm; }

void Suggestion::setTimeSignature (int numerator, int denominator)
{
    auto& operation = impl->append (Impl::Operation::Kind::setTimeSignature);
    operation.firstInt = numerator;
    operation.secondInt = denominator;
}

namespace
{
    void syncProperties (juce::ValueTree target, const juce::ValueTree& source)
    {
        for (int property = target.getNumProperties(); --property >= 0;)
        {
            const auto name = target.getPropertyName (property);

            if (! source.hasProperty (name))
                target.removeProperty (name, nullptr);
        }

        for (int property = 0; property < source.getNumProperties(); ++property)
        {
            const auto name = source.getPropertyName (property);
            target.setProperty (name, source[name], nullptr);
        }
    }

    bool isProjectChild (const juce::ValueTree& child, bool isRoot)
    {
        return ! isRoot || ! child.hasType (te::IDs::TRANSPORT);
    }

    auto findMatchingChild (std::vector<juce::ValueTree>& available, const juce::ValueTree& source)
    {
        return std::find_if (available.begin(),
                             available.end(),
                             [&source] (const juce::ValueTree& candidate)
                             {
                                 if (! candidate.hasType (source.getType()))
                                     return false;

                                 return ! source.hasProperty (te::IDs::id)
                                        || candidate[te::IDs::id].toString()
                                               == source[te::IDs::id].toString();
                             });
    }

    void putChildrenInOrder (juce::ValueTree target, const std::vector<juce::ValueTree>& wanted)
    {
        for (std::size_t index = 0; index < wanted.size(); ++index)
        {
            const auto at = target.indexOf (wanted[index]);
            const auto wantedIndex = static_cast<int> (index);

            if (at != wantedIndex)
                target.moveChild (at, wantedIndex, nullptr);
        }
    }

    /** Makes a live tree state exactly what a detached snapshot states, with no
        UndoManager, while retaining the live nodes for items present in both.

        Retaining those nodes matters twice: engine objects listen to them, and a
        session-only NoteRef is a handle onto one. Replacing the whole tree would
        briefly register two engine items with one ID and would invalidate every
        existing note handle.
    */
    void syncTree (juce::ValueTree target, const juce::ValueTree& source)
    {
        struct Pending
        {
            juce::ValueTree target;
            juce::ValueTree source;
            bool isRoot = false;
        };

        std::vector<Pending> remaining { { target, source, true } };

        while (! remaining.empty())
        {
            auto next = remaining.back();
            remaining.pop_back();

            // Suggestion operations live below the EDIT root. Its properties are
            // engine bookkeeping, and copying a shadow Edit's bookkeeping back
            // would itself look like a project edit.
            if (! next.isRoot)
                syncProperties (next.target, next.source);

            std::vector<juce::ValueTree> available;
            for (const auto& child : next.target)
                if (isProjectChild (child, next.isRoot))
                    available.push_back (child);

            std::vector<juce::ValueTree> wanted;

            for (const auto& sourceChild : next.source)
            {
                if (! isProjectChild (sourceChild, next.isRoot))
                    continue;

                const auto found = findMatchingChild (available, sourceChild);

                if (found == available.end())
                {
                    auto added = sourceChild.createCopy();
                    next.target.appendChild (added, nullptr);
                    wanted.push_back (added);
                    continue;
                }

                auto retained = *found;
                available.erase (found);
                remaining.push_back ({ retained, sourceChild, false });
                wanted.push_back (retained);
            }

            for (const auto& removed : available)
                next.target.removeChild (removed, nullptr);

            // Root-child order states nothing, and its TRANSPORT child is omitted
            // from wanted, so only order the descendants where order has meaning.
            if (! next.isRoot)
                putChildrenInOrder (next.target, wanted);
        }
    }

    void replaceProjectState (te::Edit& edit, const juce::ValueTree& replacement)
    {
        // Directly appending a detached item does not pass through createNewItemID,
        // so advance the live allocator beyond every ID about to arrive. Otherwise
        // accepting immediately after an Audition can reuse an ID while the graph
        // still retains the transient item that had it.
        const auto replacementIDs = te::EditItemID::findAllIDs (replacement);

        if (! replacementIDs.empty())
        {
            const auto largest = std::max_element (replacementIDs.begin(), replacementIDs.end());

            while (edit.createNewItemID().getRawID() <= largest->getRawID())
            {
            }
        }

        // Some engine listeners answer a direct no-undo ValueTree change with a
        // bookkeeping write through the Edit's UndoManager. Isolate those answers
        // in a fresh transaction, then undo that transaction only. JUCE restores a
        // pre-existing redo future in this path; the direct nullptr writes remain.
        auto& undoManager = edit.getUndoManager();
        undoManager.beginNewTransaction();

        // The transport is not project editing state: Audition must not stop,
        // reposition, or otherwise write it. syncTree keeps the live TRANSPORT node
        // itself while making every project node match the suggested snapshot.
        syncTree (edit.state, replacement);
        edit.dispatchPendingUpdatesSynchronously();
        undoManager.undoCurrentTransactionOnly();
    }
} // namespace

void Session::applySuggestion (const Suggestion& suggestion)
{
    std::unordered_map<std::uint64_t, std::uint64_t> placeholders;

    const auto resolve = [&placeholders] (const Suggestion::Impl::Reference& reference)
    {
        if (! reference.isPlaceholder)
            return reference.value;

        const auto found = placeholders.find (reference.value);
        return found != placeholders.end() ? found->second : std::uint64_t { 0 };
    };

    const auto automationTarget = [&resolve] (const Suggestion::Impl::Operation& operation)
    {
        const auto item = resolve (operation.first);

        switch (operation.automationKind)
        {
            case AutomationTarget::Kind::trackVolume:
                return AutomationTarget::trackVolumeOf (item);
            case AutomationTarget::Kind::trackPan:
                return AutomationTarget::trackPanOf (item);
            case AutomationTarget::Kind::pluginParameter:
                return AutomationTarget::parameterOf (item, operation.text);
        }

        return AutomationTarget {};
    };

    performAction (
        suggestion.name(),
        [&] (EditOps& ops)
        {
            for (const auto& operation : suggestion.impl->operations)
            {
                const auto first = resolve (operation.first);
                const auto second = resolve (operation.second);

                switch (operation.kind)
                {
                    case Suggestion::Impl::Operation::Kind::createTrack:
                        placeholders[operation.result.value] = ops.createTrack (
                            operation.trackKind, operation.text, operation.builtin);
                        break;
                    case Suggestion::Impl::Operation::Kind::duplicateTrack:
                        placeholders[operation.result.value] = ops.duplicateTrack (first);
                        break;
                    case Suggestion::Impl::Operation::Kind::removeTrack:
                        ops.removeTrack (first);
                        break;
                    case Suggestion::Impl::Operation::Kind::renameTrack:
                        ops.renameTrack (first, operation.text);
                        break;
                    case Suggestion::Impl::Operation::Kind::moveTrack:
                        ops.moveTrack (first, operation.firstInt);
                        break;
                    case Suggestion::Impl::Operation::Kind::setTrackOutput:
                        ops.setTrackOutput (first, second);
                        break;
                    case Suggestion::Impl::Operation::Kind::insertAudioClip:
                        placeholders[operation.result.value] = ops.insertAudioClip (
                            first, operation.text, operation.path, operation.a, operation.b);
                        break;
                    case Suggestion::Impl::Operation::Kind::insertMidiClip:
                        placeholders[operation.result.value] =
                            ops.insertMidiClip (first, operation.text, operation.a, operation.b);
                        break;
                    case Suggestion::Impl::Operation::Kind::moveClip:
                        ops.moveClip (first, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::moveClipToTrack:
                        ops.moveClip (first, second, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::trimClip:
                        ops.trimClip (first, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::trimClipEdges:
                        ops.trimClip (first, operation.a, operation.b);
                        break;
                    case Suggestion::Impl::Operation::Kind::deleteClip:
                        ops.deleteClip (first);
                        break;
                    case Suggestion::Impl::Operation::Kind::setClipLoop:
                        ops.setClipLoop (first, operation.flag, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::duplicateClip:
                        placeholders[operation.result.value] =
                            ops.duplicateClip (first, second, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::addNote:
                        placeholders[operation.result.value] = ops.addNote (first,
                                                                            operation.firstInt,
                                                                            operation.a,
                                                                            operation.b,
                                                                            operation.secondInt);
                        break;
                    case Suggestion::Impl::Operation::Kind::removeNote:
                        ops.removeNote (first);
                        break;
                    case Suggestion::Impl::Operation::Kind::moveNote:
                        ops.moveNote (first, operation.firstInt, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::resizeNote:
                        ops.resizeNote (first, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::setNoteVelocity:
                        ops.setNoteVelocity (first, operation.firstInt);
                        break;
                    case Suggestion::Impl::Operation::Kind::setTrackVolumeDb:
                        ops.setTrackVolumeDb (first, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::setTrackPan:
                        ops.setTrackPan (first, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::setTrackMute:
                        ops.setTrackMute (first, operation.flag);
                        break;
                    case Suggestion::Impl::Operation::Kind::setTrackSolo:
                        ops.setTrackSolo (first, operation.flag);
                        break;
                    case Suggestion::Impl::Operation::Kind::setTrackColour:
                        ops.setTrackColour (first, operation.colour);
                        break;
                    case Suggestion::Impl::Operation::Kind::setSend:
                        ops.setSend (first, second, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::addBuiltinPlugin:
                        placeholders[operation.result.value] =
                            ops.addPlugin (first, *operation.builtin, operation.firstInt);
                        break;
                    case Suggestion::Impl::Operation::Kind::addExternalPlugin:
                        placeholders[operation.result.value] =
                            ops.addPlugin (first, operation.text, operation.firstInt);
                        break;
                    case Suggestion::Impl::Operation::Kind::removePlugin:
                        ops.removePlugin (first);
                        break;
                    case Suggestion::Impl::Operation::Kind::reorderPlugin:
                        ops.reorderPlugin (first, operation.firstInt);
                        break;
                    case Suggestion::Impl::Operation::Kind::setPluginParameter:
                        ops.setPluginParameter (first, operation.text, operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::setPluginSidechainSource:
                        ops.setPluginSidechainSource (first, second);
                        break;
                    case Suggestion::Impl::Operation::Kind::setAutomationPoints:
                        ops.setAutomationPoints (automationTarget (operation), operation.points);
                        break;
                    case Suggestion::Impl::Operation::Kind::removeAutomationPoints:
                        ops.removeAutomationPoints (
                            automationTarget (operation), operation.a, operation.b);
                        break;
                    case Suggestion::Impl::Operation::Kind::setTempo:
                        ops.setTempo (operation.a);
                        break;
                    case Suggestion::Impl::Operation::Kind::setTimeSignature:
                        ops.setTimeSignature (operation.firstInt, operation.secondInt);
                        break;
                }
            }
        });
}

bool Session::auditionSuggestion (const Suggestion& suggestion)
{
    if (isAuditioning())
        return false;

    if (impl->suggestionEdit == nullptr)
    {
        impl->suggestionEdit = te::loadEditFromState (impl->engine, impl->edit->state.createCopy());

        if (impl->suggestionEdit == nullptr)
            return false;

        auto editFile = toJuceFile (impl->editFile);
        impl->suggestionEdit->editFileRetriever = [editFile] { return editFile; };
    }
    else
    {
        // Keep the detached Edit, especially its monotonically advancing item-ID
        // allocator, but begin from today's canonical project state.
        replaceProjectState (*impl->suggestionEdit, impl->edit->state);
    }

    // NoteRef is session-only. Carry each existing handle to the corresponding
    // note in the detached Edit, by its ordinal in the same durable clip.
    impl->suggestionNotesByRef.clear();
    impl->nextSuggestionNoteRef = impl->nextNoteRef;

    for (const auto& [ref, handle] : impl->notesByRef)
    {
        auto* sourceClip = impl->midiClipFor (handle.clip);
        auto* shadowClip = dynamic_cast<te::MidiClip*> (
            te::findClipForID (*impl->suggestionEdit, toItemID (handle.clip)));

        if (sourceClip == nullptr || shadowClip == nullptr)
            continue;

        const auto& sourceNotes = sourceClip->getSequence().getNotes();
        const auto& shadowNotes = shadowClip->getSequence().getNotes();
        int ordinal = -1;

        for (int index = 0; index < sourceNotes.size(); ++index)
            if (sourceNotes[index]->state == handle.state)
            {
                ordinal = index;
                break;
            }

        if (ordinal >= 0 && ordinal < shadowNotes.size())
            impl->suggestionNotesByRef.emplace (
                ref, Impl::NoteHandle { handle.clip, shadowNotes[ordinal]->state });
    }

    // EditOps always targets impl->edit. Temporarily put the detached Edit and
    // its session-only note handles there, with project change reporting muted;
    // this materializes the operation data without a second Engine or a second
    // owner of the app-global settings store.
    std::swap (impl->edit, impl->suggestionEdit);
    std::swap (impl->notesByRef, impl->suggestionNotesByRef);
    std::swap (impl->nextNoteRef, impl->nextSuggestionNoteRef);
    auto projectChanged = std::move (impl->projectChanged);

    const auto restoreLiveEdit = [&]
    {
        impl->projectChanged = std::move (projectChanged);
        std::swap (impl->nextNoteRef, impl->nextSuggestionNoteRef);
        std::swap (impl->notesByRef, impl->suggestionNotesByRef);
        std::swap (impl->edit, impl->suggestionEdit);
    };

    try
    {
        startUndoHistory();
        applySuggestion (suggestion);
    }
    catch (...)
    {
        restoreLiveEdit();
        throw;
    }

    restoreLiveEdit();

    impl->stateBeforeAudition = impl->edit->state.createCopy();
    replaceProjectState (*impl->edit, impl->suggestionEdit->state);
    impl->refreshParametersFromState();
    impl->applyLoopRange();
    impl->auditionedSuggestion = &suggestion;
    return true;
}

void Session::stopAudition()
{
    if (! isAuditioning())
        return;

    replaceProjectState (*impl->edit, impl->stateBeforeAudition);
    impl->refreshParametersFromState();
    impl->applyLoopRange();
    impl->stateBeforeAudition = {};
    impl->auditionedSuggestion = nullptr;
}

bool Session::isAuditioning() const { return impl->auditionedSuggestion != nullptr; }

bool Session::acceptSuggestion (const Suggestion& suggestion)
{
    if (isAuditioning())
        stopAudition();

    applySuggestion (suggestion);
    return true;
}

void Session::rejectSuggestion (const Suggestion& suggestion)
{
    if (impl->auditionedSuggestion == &suggestion)
        stopAudition();
}
} // namespace duet::model
