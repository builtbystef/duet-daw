#pragma once

#include <duet/model/Session.h>

#include <cstddef>
#include <cstdint>
#include <functional>
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

        Spec js437t's Suggestion manager is what implements this in the finished
        app; `ScriptedSuggestions` beneath is the development-only stand-in, so
        that every state of the card and the ghosts is reachable before the
        Collaborator's service exists.
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

        /** Drops the whole Suggestion, applying nothing. */
        virtual void reject (std::string_view id) = 0;

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

    /** Takes the whole Suggestion down, applying nothing. */
    void reject (std::string_view id);

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

    /** What marks a Suggestion the project has moved under. */
    static constexpr const char* staleLabel = "Stale";

    /** The two sides of the A/B, as the chip on an affected strip names them. */
    static constexpr const char* currentSideLabel = "A: CURRENT";
    static constexpr const char* proposedSideLabel = "B: PROPOSED";

private:
    [[nodiscard]] std::size_t indexOf (std::string_view id) const;
    void hearProposed();

    Source* source = nullptr;
    std::vector<SuggestionCardView> pending;

    /** Which Elements the producer has unticked, by Suggestion. Absent is
        ticked, so a Suggestion nobody has touched costs nothing to remember.
    */
    std::vector<std::pair<std::string, std::vector<std::size_t>>> unticked;

    std::optional<std::string> audition_;
    bool proposedHeard = false;
};

/** The development-only Suggestions that stand in for the Collaborator's.

    It speaks to no AI backend, opens no socket and reaches no network: it makes
    one Suggestion of three Elements over the project as it stands — two clip
    changes and a fader change — so that every state the card and the ghosts can
    be in is reachable by hand before spec js437t's Suggestion manager is wired
    to this interface. The mechanism it drives is the real one: an Audition is
    the model's apply-and-revert, and an acceptance is one Action on the
    producer's own undo history.

    Staleness here is the whole project moving rather than the named things a
    Suggestion touches — which is the manager's measurement, not a stand-in's —
    so a producer edit of any kind marks it, and its own acceptance does not.
*/
class ScriptedSuggestions final : public Suggestions::Source
{
public:
    ScriptedSuggestions() = default;

    /** The project the Suggestion is made over, or nothing when none is open.
        Whatever was pending goes with the project it was made against.
    */
    void setSession (duet::model::Session* openProject);

    /** Makes the Suggestion. False when there is no project to make one over.
        Only one stands at a time: making another replaces it.
    */
    bool fabricate();

    /** Told whenever a Suggestion is made, so that the card can be put in the
        conversation it belongs in.
    */
    void onSuggestionMade (std::function<void (std::string id, std::string summary)> notify);

    [[nodiscard]] std::vector<SuggestionCardView> pending() override;
    bool audition (std::string_view id, const std::vector<std::size_t>& elements) override;
    void stopAudition() override;
    bool accept (std::string_view id, const std::vector<std::size_t>& elements) override;
    void reject (std::string_view id) override;

    /** What the fabricated Suggestion asks the track's fader to read. */
    static constexpr double proposedLevelDb = -3.0;

    /** How far the fabricated Suggestion moves a clip, and how long the clips
        it makes are.
    */
    static constexpr double movedByBeats = 4.0;
    static constexpr double madeClipBeats = 4.0;

private:
    struct Element
    {
        SuggestionElementView view;
        duet::model::Suggestion changes { "" };
    };

    struct Made
    {
        std::string id;
        std::string summary;
        std::vector<Element> elements;
        std::uint64_t madeAtRevision = 0;
    };

    [[nodiscard]] duet::model::Suggestion applicable (const std::vector<std::size_t>& elements,
                                                      std::string_view name) const;

    /** Leaves a live Audition and carries the staleness baseline over it.

        An Audition moves the revision twice — once putting the suggested state
        in front of the producer and once taking it away — and neither is the
        project moving under the Suggestion, because the second undoes the
        first exactly. So the baseline moves by what the Audition spent, which
        leaves a Suggestion that was stale before it stale after it.
    */
    void endAudition();

    duet::model::Session* session = nullptr;
    std::optional<Made> made;
    std::function<void (std::string, std::string)> suggestionMade;
    std::uint64_t revisionEnteringAudition = 0;
    int fabricated = 0;
};
} // namespace duet::gui
