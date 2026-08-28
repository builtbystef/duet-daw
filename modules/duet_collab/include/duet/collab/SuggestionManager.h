#pragma once

#include <duet/collab/SuggestTool.h>
#include <duet/collab/TaskRun.h>

#include <duet/model/Session.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace duet::collab
{
/** Where a Suggestion is in its life.

    Pending is the only state anything is applied from, and the three endings
    are endings: a Suggestion that has reached one never leaves it again.
*/
enum class SuggestionState : std::uint8_t
{
    pending,
    accepted,
    rejected,
    superseded
};

/** Where one Element of a Suggestion is.

    An Element is the cherry-pick unit, so it has a state of its own, and a
    Suggestion is resolved exactly when none of its Elements is pending.
*/
enum class ElementState : std::uint8_t
{
    pending,
    accepted,
    rejected
};

/** What a Suggestion the manager holds looks like from outside it. */
struct SuggestionInfo
{
    /** The Suggestion itself, as the `suggest` tool made it. */
    Suggestion made { {}, {} };

    /** What the producer asked that produced it: what a revision and a redo
        against current state each ask again.
    */
    std::string request;

    SuggestionState state = SuggestionState::pending;

    /** True while the project has moved under this Suggestion: something an
        Element's operations name is not what it was when the Suggestion was
        made. A stale Suggestion is auditionable like any other and merges by
        itself as little as any other — the producer's own edits win, so what
        goes is the Suggestion's claim to still fit, not its usefulness.
    */
    bool stale = false;

    /** One state per Element, in the Suggestion's own order. */
    std::vector<ElementState> elements;

    /** The Suggestion this one was asked to improve on, and empty for a first
        one.
    */
    std::string revises;
};

/** The Duet Loop: what makes a Suggestion a conversation rather than a patch.

    The manager holds every Suggestion of an app session, each pending until the
    producer resolves it, and holds the requests behind them, because a revision
    and a redo are the same request asked again. Several Suggestions are pending
    at once and independently: resolving one says nothing about another.

    **Resolving.** Accepting an Element applies exactly that Element's
    operations, as one Action named for the Element; rejecting one drops them.
    Accepting or rejecting the whole Suggestion resolves every Element still
    pending, and an acceptance of several Elements is still one Action, named
    for the summary. A Suggestion is resolved when no Element of it is pending,
    and it is accepted when any Element of it was and rejected when none was. An
    ending is final: nothing resolves a Suggestion twice.

    **Stale.** Every id a pending Suggestion's operations name is remembered as
    what the project said about it when the Suggestion was made, and a
    Suggestion whose project has since said something else is stale. It is
    still auditionable and it is still acceptable; what it is not is applied by
    anything but an explicit acceptance. A change the producer undoes is not a
    change, so an undo back to what the Suggestion was made against takes the
    staleness with it. A Suggestion's own accepted Element is not a change out
    from under it, and does not make it stale; another Suggestion's acceptance
    is the producer editing, and does.

    **Asking again.** Redoing a pending Suggestion resolves it as superseded and
    starts one Task Run carrying the original request and what has changed under
    it since. Replying supersedes a pending Suggestion the same way and leaves a
    rejected one rejected, and either way starts one run carrying the original
    request and what the producer said. A run that is refused changes nothing:
    the Suggestion stays where it was.

    **Nothing is written down.** Suggestions, the requests behind them and the
    replies to them live in this object for the app session and die with it.
    Nothing here reaches the project folder, and a save while a Suggestion is
    pending writes a file that knows nothing about it.

    Every member is called on the message thread, which is the sole writer of
    the project model and therefore the one thread that may apply anything. The
    session must outlive this object.
*/
class SuggestionManager
{
public:
    /** Starts one Task Run for a prompt this manager composed, and answers what
        came of asking.

        The manager composes prompts and knows nothing about opening context:
        what the producer had selected and where the playhead was belong to the
        moment a run starts, so whoever supplies this is who supplies those.
    */
    using RunLauncher = std::function<RunStart (const std::string& prompt)>;

    SuggestionManager (model::Session& projectSession, RunLauncher launchRun);
    ~SuggestionManager();

    SuggestionManager (const SuggestionManager&) = delete;
    SuggestionManager (SuggestionManager&&) = delete;
    SuggestionManager& operator= (const SuggestionManager&) = delete;
    SuggestionManager& operator= (SuggestionManager&&) = delete;

    //==============================================================================
    /** Asks the Collaborator something, and remembers what was asked. */
    RunStart ask (std::string request);

    /** Holds what a run's `suggest` call made, pending.

        False for a run this manager did not start, because a Suggestion with no
        request behind it is one that can be neither revised nor redone.
    */
    bool suggested (const std::string& runId, Suggestion made);

    //==============================================================================
    /** Every Suggestion of this app session, oldest first.

        Reading is when the project is compared with what the pending
        Suggestions were made against, so what comes back has today's staleness
        on it rather than the staleness of the last time anything was resolved.
    */
    [[nodiscard]] std::vector<SuggestionInfo> suggestions() const;

    /** The one an id names, and nothing when no Suggestion has that id. */
    [[nodiscard]] std::optional<SuggestionInfo> suggestion (std::string_view id) const;

    //==============================================================================
    /** Applies one Element's operations as one Action named for the Element.

        False for a Suggestion that is not pending, an Element that is not, and
        an Element that is not there.
    */
    bool acceptElement (std::string_view id, std::size_t element);

    /** Drops one Element's operations, applying nothing. */
    bool rejectElement (std::string_view id, std::size_t element);

    /** Applies every Element still pending, as one Action named for the
        summary.
    */
    bool accept (std::string_view id);

    /** Resolves every Element still pending, applying nothing. */
    bool reject (std::string_view id);

    //==============================================================================
    /** Replies to a Suggestion, and asks for a better one.

        A pending Suggestion is superseded by the reply and a rejected one stays
        rejected. What the producer typed is what the revision run is asked in,
        beside the original request. Refused for a Suggestion that is accepted
        or already superseded, and for one that is not there.
    */
    RunStart reply (std::string_view id, const std::string& what);

    /** Asks the same question again, against the project as it now stands.

        The pending Suggestion is superseded, and the run carries the original
        request and every change the project has made under it since.
    */
    RunStart redo (std::string_view id);

    //==============================================================================
    /** Auditions every Element of a Suggestion that is still pending: what
        accepting it now would do, made audible without doing it.

        Any other Audition is stopped first, there being one at a time. False
        when the Suggestion is not pending, or has no Element left to hear.
    */
    bool audition (std::string_view id);

    /** Ends a live Audition. Does nothing when none is live. */
    void stopAudition();

    /** The Suggestion being auditioned, and nothing when none is. */
    [[nodiscard]] std::optional<std::string> auditioning() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::collab
