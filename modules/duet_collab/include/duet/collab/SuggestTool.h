#pragma once

#include <duet/collab/Estimate.h>
#include <duet/collab/ProjectTools.h>
#include <duet/collab/ToolDispatch.h>

#include <duet/model/Session.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace duet::collab
{
/** One human-meaningful change: what it is, and the operations that make it.

    An element is the row the producer ticks or crosses on a Suggestion card,
    so it is also the cherry-pick unit, and that is why an element must be
    independently applicable: an operation may target what an earlier operation
    of the same element creates, and never what another element creates.
*/
struct SuggestionElement
{
    explicit SuggestionElement (std::string elementDescription);

    std::string description;

    /** The element's operations, in the order the call gave them and in the
        shape validation accepted them, which is what a read-back reads.
    */
    std::vector<Json> operations;

    /** The same operations as the model applies them, named for this element
        because accepting one element lands as an Action called by its own name.
    */
    model::Suggestion changes;
};

/** A Suggestion as the `suggest` tool made it: what the Collaborator called the
    whole change, the elements it is cherry-picked by, and the id the call was
    answered with.

    `duet::model::Suggestion` is the applicable form of the same thing — a flat
    ordered operation list the model auditions and accepts. This is the shape
    the seam speaks in, and it is the one that has a summary, elements, and an
    id, none of which the model layer knows about.
*/
struct Suggestion
{
    Suggestion (std::string suggestionId, std::string suggestionSummary);

    std::string id;
    std::string summary;
    std::vector<SuggestionElement> elements;

    /** Whether the run that made this Suggestion had been handed an estimate by
        the time it did.

        The estimate mark, read from that run's ledger when the Suggestion was
        made and never from anything the model said about itself (spec js437t).
        A Suggestion carries it for as long as it exists, because what a producer
        is being asked to accept is the thing the mark is about.
    */
    bool basedOnEstimates = false;

    /** Every element's operations as one list, named for the summary, which is
        what accepting the whole Suggestion applies as a single Action.
    */
    model::Suggestion changes;
};

/** The Collaborator's one write-tool, and the closure principle behind it.

    `suggest` takes a summary and an ordered list of elements, validates every
    operation against the live project before anything exists, and answers with
    the id of the Suggestion it made. Nothing about the project moves: a
    Suggestion is data until the producer accepts it, so a call leaves the
    project state and the producer's undo history exactly as it found them.

    The operation set mirrors what the Target Producer can do through the
    milestone-one interface and nothing else, which is what makes a Suggestion
    something the producer could have made by hand. Nothing in it creates audio
    content: an audio clip can be moved, trimmed, looped, duplicated and
    deleted, and never brought into being.

    A call carries ids the way the read tools write them — `track-3`, `clip-7`,
    `plugin-2`, `note-1`, `track-master` — or a placeholder, written with a
    leading `#`, that an earlier operation of the same element declared as its
    `ref`. A placeholder can never be a project id and a project id can never be
    a placeholder, so nothing has to be guessed at either end.

    Validation happens before anything is made, and a failure is an error the
    model can correct against: it names where it happened —
    `elements[0].operations[1]` — and what was wrong with it. A run may correct
    and call again; what it may not do is make a second Suggestion, because a
    Task Run produces at most one.

    A value is held to the range the thing it is written to has: the fader's
    travel for a level, −1 to +1 for a pan, the MIDI ranges for a note, and a
    plugin parameter's own two ends — in the real units where Duet owns the
    parameter's meaning and the plugin's own normalised 0..1 where it does not,
    so a number in the wrong one of those two domains is refused rather than
    quietly converted. A plugin the same element is adding is held to those same
    two ends: which parameters a built-in has and what each of them may be is a
    fact about the plugin and not about an instance of it, and so is the dry and
    wet level the engine gives every plugin Duet hosts. A scanned plugin's own
    parameter an element adds is held to 0..1, that being the whole of what Duet
    can say about a mapping it does not own, and only such a parameter's *ids*
    cannot be checked before the plugin exists, because they are the vendor's
    own.

    A Suggestion is stamped with the estimate mark of the run that made it, from
    that run's ledger. Given no ledger nothing is ever marked, which is what a
    Collaborator with nothing estimating wired to it should say.

    The session, the marshal and the ledger must all outlive this object, and
    this object must outlive the registry it was added to.
*/
class SuggestTool
{
public:
    SuggestTool (model::Session& projectSession,
                 ProjectReadMarshal readMarshal,
                 const EstimateLedger* estimateLedger = nullptr);
    ~SuggestTool();

    SuggestTool (const SuggestTool&) = delete;
    SuggestTool (SuggestTool&&) = delete;
    SuggestTool& operator= (const SuggestTool&) = delete;
    SuggestTool& operator= (SuggestTool&&) = delete;

    /** Adds `suggest` to a registry, replacing a tool of the same name. */
    void addTo (ToolRegistry& registry);

    /** Every Suggestion this tool has made, oldest first.

        Made on the service thread and read from wherever a surface reads it, so
        this is a copy taken under the tool's own lock rather than a reference
        into it.
    */
    [[nodiscard]] std::vector<Suggestion> suggestions() const;

    /** The Suggestion an id names, and nothing when no call made one. */
    [[nodiscard]] std::optional<Suggestion> suggestion (std::string_view id) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::collab
