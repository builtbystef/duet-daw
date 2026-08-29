#pragma once

#include <duet/model/Session.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace duet::gui
{
/** A clip a Suggestion would put on the timeline, where its operations put it.

    A ghost is a drawing and not a clip: nothing in the project answers to it,
    and it is gone the moment the Suggestion it belongs to is resolved.
*/
struct GhostClip
{
    duet::model::TrackRef track = duet::model::noTrack;
    std::string name;
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;
    bool holdsMidi = false;
};

/** A mixer value a Suggestion would set: what the fader would read, drawn
    beside where it reads now.
*/
struct GhostFader
{
    duet::model::TrackRef channel = duet::model::noTrack;
    double db = 0.0;
};

/** One row of a Suggestion card: one human-meaningful change, in the words the
    Collaborator gave it, and what it would look like where it would land.

    An Element is the cherry-pick unit, which is why the ghosts hang off it: an
    element the producer unchecks takes its own ghosts down to the excluded
    intensity with it.
*/
struct SuggestionElementView
{
    std::string description;
    std::vector<GhostClip> clips;
    std::vector<GhostFader> faders;
};

/** A pending Suggestion, as the card in the conversation and the ghosts on the
    surfaces read it.
*/
struct SuggestionCardView
{
    std::string id;
    std::string summary;

    /** True when the project has moved under this Suggestion. It is marked as
        such and it stays auditionable.
    */
    bool stale = false;

    std::vector<SuggestionElementView> elements;
};

/** The Suggestions the producer has not resolved, and what the surfaces draw of
    them.

    This is the rendering half of the Duet Loop: which Suggestions are pending,
    which of their Elements the producer has ticked, which one is being
    auditioned and which side of the A/B is heard. The mechanism underneath —
    what applying an Element does, what makes a Suggestion stale, what an
    acceptance lands as — is not this object's, and reaches it through `Source`.

    Every member is called on the message thread, which is the sole writer of
    the project model.
*/
class Suggestions
{
public:
    Suggestions() = default;

    //==============================================================================
    /** Where the pending Suggestions come from and what resolving one does.

        Spec js437t's Suggestion manager is what implements this, through the
        adapter that puts it on these surfaces: this module links no engine and
        cannot name the manager, so the app is where the two meet.
    */
    class Source
    {
    public:
        virtual ~Source() = default;

        Source (const Source& other) = delete;
        Source& operator= (const Source& other) = delete;

        /** Every Suggestion the producer has not resolved, oldest first. */
        [[nodiscard]] virtual std::vector<SuggestionCardView> pending() = 0;

        /** Hears what accepting exactly these Elements now would do, without
            doing it. Any other Audition ends first, there being one at a time.
        */
        virtual bool audition (std::string_view id, const std::vector<std::size_t>& elements) = 0;

        /** Ends a live Audition, leaving what was heard before it. */
        virtual void stopAudition() = 0;

        /** Applies exactly these Elements, as one Action. The Elements that are
            left stay pending.
        */
        virtual bool accept (std::string_view id, const std::vector<std::size_t>& elements) = 0;

        /** Drops the whole Suggestion, applying nothing.

            `reason` is what the producer typed about it, and empty when they
            typed nothing. It is first-class input to the revision the
            rejection asks for (spec js437t), so it crosses here rather than
            being lost between the card and whatever answers it.
        */
        virtual void reject (std::string_view id, const std::string& reason) = 0;

        /** Asks the same question again, against the project as it now stands:
            what the redo control on a stale Suggestion does. The Suggestion
            goes and the answer arrives as a card of its own.
        */
        virtual bool redo (std::string_view id) = 0;

    protected:
        Source() = default;
    };

    /** The source is read and never owned, and none is a surface with no ghosts
        on it.
    */
    void setSource (Source* newSource);

    /** Takes what is pending now. The producer's ticks survive it, because a
        checkbox is the producer's answer and not the Suggestion's.
    */
    void refresh();

    //==============================================================================
    /** Every pending Suggestion, oldest first. */
    [[nodiscard]] const std::vector<SuggestionCardView>& cards() const { return pending; }

    /** The one an id names, and nothing when none is pending under it. */
    [[nodiscard]] const SuggestionCardView* card (std::string_view id) const;

    //==============================================================================
    /** Cherry-pick: whether the producer has this Element ticked. Everything a
        Suggestion arrives with is ticked.
    */
    [[nodiscard]] bool isChecked (std::string_view id, std::size_t element) const;
    void setChecked (std::string_view id, std::size_t element, bool checked);

    /** The Elements the producer has left ticked, in the Suggestion's order:
        what Audition hears and what Accept applies.
    */
    [[nodiscard]] std::vector<std::size_t> checkedElements (std::string_view id) const;

    //==============================================================================
    /** Hears the ticked Elements of a Suggestion. False when there is nothing
        ticked to hear.
    */
    bool audition (std::string_view id);

    /** Leaves the Audition: the look and the sound go back to what they were
        before it was entered.
    */
    void stopAudition();

    /** The Suggestion being auditioned, and nothing when none is. */
    [[nodiscard]] std::optional<std::string> auditioning() const { return audition_; }

    [[nodiscard]] bool isAuditioning (std::string_view id) const;

    /** Which side of the A/B is heard: the project as it stands, or the
        Suggestion applied over it. Entering an Audition lands on B.
    */
    [[nodiscard]] bool hearingProposed() const { return proposedHeard; }

    /** Swaps the heard side. The transport is not asked to stop for it. */
    void toggleAB();

    //==============================================================================
    /** Applies the ticked Elements as one Action and takes their ghosts down.
        The Elements the producer left unticked stay as they were.
    */
    bool accept (std::string_view id);

    /** Takes the whole Suggestion down, applying nothing, and carries the
        producer's reason to whatever asks for a better one.
    */
    void reject (std::string_view id, const std::string& reason = {});

    /** Asks for this Suggestion again against the project as it now stands, and
        takes it down. False when the source would not ask.
    */
    bool redo (std::string_view id);

    //==============================================================================
    /** How strongly one Element is drawn, in its card row and in its ghosts: as
        it is, or at the excluded intensity when the producer has unticked it.
    */
    [[nodiscard]] double intensityOf (std::string_view id, std::size_t element) const;

    /** How much of the reserved teal a Suggestion's ghosts are filled with:
        their pending weight, or the auditioning one while it is being heard.
    */
    [[nodiscard]] double fillAlphaOf (std::string_view id) const;

    //==============================================================================
    /** The ghost treatment, as the visual reference settled it (spec 535bbo):
        a teal wash under a dashed teal border, three rings of soft glow around
        it, and a ✦ before the name.
    */
    static constexpr double pendingFillAlpha = 0.12;
    static constexpr int glowRings = 3;

    /** And what the wash carries while the Suggestion is being auditioned:
        nothing.

        An Audition puts the suggested state into the project, so by the time a
        ghost is heard the clip it describes is already under it, drawn in its
        own colour and carrying its own name. A wash over that muddies both and
        prints the name twice. What marks it as the Suggestion's while it is
        heard is the solid border, the glow and the badge — the reference's
        heavier fill was drawn for a ghost standing on its own, which an
        auditioned one never is.
    */
    static constexpr double auditionFillAlpha = 0.0;

    /** What an Element the producer has unticked is drawn at, wherever it is
        drawn.
    */
    static constexpr double excludedIntensity = 0.35;

    /** The words on the card. The Audition button says "Audition" because that
        is what the glossary calls it.
    */
    static constexpr const char* auditionLabel = "Audition";
    static constexpr const char* acceptLabel = "Accept";
    static constexpr const char* rejectLabel = "Reject";

    /** What marks a Suggestion the project has moved under, and what the
        control beside the mark offers to do about it.
    */
    static constexpr const char* staleLabel = "Stale";
    static constexpr const char* redoLabel = "Redo against current state";

    /** What the box the producer types a rejection reason into says when it is
        empty. Saying why is the producer's to offer, not the panel's to
        demand: Reject with nothing typed rejects.
    */
    static constexpr const char* rejectReasonHint = "Why not? (optional)";

    /** The two sides of the A/B, as the chip on an affected strip names them. */
    static constexpr const char* currentSideLabel = "A: CURRENT";
    static constexpr const char* proposedSideLabel = "B: PROPOSED";

private:
    [[nodiscard]] std::size_t indexOf (std::string_view id) const;
    void hearProposed();

    /** Drops the ticks kept for a Suggestion that has gone. */
    void forget (std::string_view id);

    Source* source = nullptr;
    std::vector<SuggestionCardView> pending;

    /** Which Elements the producer has unticked, by Suggestion. Absent is
        ticked, so a Suggestion nobody has touched costs nothing to remember.
    */
    std::vector<std::pair<std::string, std::vector<std::size_t>>> unticked;

    std::optional<std::string> audition_;
    bool proposedHeard = false;
};
} // namespace duet::gui
