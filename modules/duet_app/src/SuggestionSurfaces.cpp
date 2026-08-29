#include <duet/app/SuggestionSurfaces.h>

#include <duet/collab/ProjectTools.h>

#include <optional>
#include <utility>

namespace duet::app
{
namespace
{
    using duet::collab::Json;
    using duet::gui::GhostClip;
    using duet::gui::GhostFader;

    /** What one operation says its `op` is. */
    [[nodiscard]] std::string opName (const Json& operation)
    {
        return operation.is_object() ? operation.value ("op", std::string {}) : std::string {};
    }

    /** A number an operation carries, and nothing when it carries none. A
        placeholder or an id in a numeric field is not a number, and the tool
        that accepted the operation is what guarantees neither is there.
    */
    [[nodiscard]] std::optional<double> number (const Json& operation, const char* field)
    {
        if (! operation.contains (field) || ! operation.at (field).is_number())
            return {};

        return operation.at (field).get<double>();
    }

    /** The track an id field names, and nothing when the field is absent or
        holds a placeholder rather than a project id.
    */
    [[nodiscard]] std::optional<duet::model::TrackRef> track (const Json& operation,
                                                              const char* field)
    {
        if (! operation.contains (field) || ! operation.at (field).is_string())
            return {};

        return duet::collab::toolId::toTrack (operation.at (field).get<std::string>());
    }

    /** The clip an operation names, and where it stands now: what a ghost of a
        move, a trim or a copy is drawn from.
    */
    struct FoundClip
    {
        duet::model::TrackRef track = duet::model::noTrack;
        duet::model::ClipInfo info;
    };

    [[nodiscard]] std::optional<FoundClip> clipOf (const duet::model::Session& session,
                                                   const Json& operation)
    {
        if (! operation.contains ("clipId") || ! operation.at ("clipId").is_string())
            return {};

        const auto ref = duet::collab::toolId::toClip (operation.at ("clipId").get<std::string>());

        if (! ref.has_value())
            return {};

        for (const auto& holder : session.tracks())
            for (const auto& clip : holder.clips)
                if (clip.clip == *ref)
                    return FoundClip { holder.track, clip };

        return {};
    }

    /** How long a stretch of bars is in seconds, measured where it starts, so a
        length written in bars is the length the call asked for wherever the
        tempo map puts it.
    */
    [[nodiscard]] double
        secondsOfBars (const duet::model::Session& session, double fromBar, double bars)
    {
        return session.secondsAtBar (fromBar + bars) - session.secondsAtBar (fromBar);
    }

    /** What one operation would look like where it would land, added to the
        Element's picture. An operation with no picture of its own adds nothing:
        it is applied by an acceptance all the same, and the Element's words are
        what the producer reads it as.
    */
    void draw (const duet::model::Session& session,
               const Json& operation,
               duet::gui::SuggestionElementView& element)
    {
        const auto name = opName (operation);

        if (name == "mixer.set")
        {
            const auto channel = track (operation, "trackId");
            const auto db = number (operation, "volumeDb");

            if (channel.has_value() && db.has_value())
                element.faders.push_back (GhostFader { *channel, *db });

            return;
        }

        if (name == "clip.createMidi")
        {
            const auto holder = track (operation, "trackId");
            const auto startBar = number (operation, "startBar");
            const auto lengthBars = number (operation, "lengthBars");

            if (! holder.has_value() || ! startBar.has_value() || ! lengthBars.has_value())
                return;

            element.clips.push_back (GhostClip { *holder,
                                                 operation.value ("name", std::string {}),
                                                 session.secondsAtBar (*startBar),
                                                 secondsOfBars (session, *startBar, *lengthBars),
                                                 true });

            return;
        }

        if (name == "clip.move" || name == "clip.trim" || name == "clip.duplicate")
        {
            const auto found = clipOf (session, operation);

            if (! found.has_value())
                return;

            const auto atBar = number (operation, name == "clip.duplicate" ? "atBar" : "startBar");

            if (! atBar.has_value())
                return;

            // A trim is the one of the three that says how long the clip then
            // is; a move and a copy carry the length they came with.
            const auto lengthBars = number (operation, "lengthBars");
            const auto length = lengthBars.has_value()
                                    ? secondsOfBars (session, *atBar, *lengthBars)
                                    : found->info.lengthSeconds;

            const auto onto = track (operation, name == "clip.duplicate" ? "toTrackId" : "trackId");

            element.clips.push_back (GhostClip { onto.value_or (found->track),
                                                 found->info.name,
                                                 session.secondsAtBar (*atBar),
                                                 length,
                                                 found->info.holdsMidi });
        }
    }
} // namespace

//==============================================================================
SuggestionSurfaces::SuggestionSurfaces (Changed onChange) : changed (std::move (onChange)) {}

SuggestionSurfaces::~SuggestionSurfaces() = default;

void SuggestionSurfaces::setManager (duet::collab::SuggestionManager* suggestionManager,
                                     duet::model::Session* openProject)
{
    manager = suggestionManager;
    session = openProject;
}

//==============================================================================
std::vector<duet::gui::SuggestionCardView> SuggestionSurfaces::pending()
{
    std::vector<duet::gui::SuggestionCardView> cards;

    if (manager == nullptr || session == nullptr)
        return cards;

    for (const auto& held : manager->suggestions())
    {
        if (held.state != duet::collab::SuggestionState::pending)
            continue;

        duet::gui::SuggestionCardView card;
        card.id = held.made.id;
        card.summary = held.made.summary;
        card.stale = held.stale;

        for (std::size_t at = 0; at < held.elements.size(); ++at)
        {
            if (held.elements.at (at) != duet::collab::ElementState::pending)
                continue;

            duet::gui::SuggestionElementView row;
            row.description = held.made.elements.at (at).description;

            for (const auto& operation : held.made.elements.at (at).operations)
                draw (*session, operation, row);

            card.elements.push_back (std::move (row));
        }

        cards.push_back (std::move (card));
    }

    return cards;
}

bool SuggestionSurfaces::audition (std::string_view id, const std::vector<std::size_t>& elements)
{
    const auto chosen = managerElements (id, elements);

    return ! chosen.empty() && manager->audition (id, chosen);
}

void SuggestionSurfaces::stopAudition()
{
    if (manager != nullptr)
        manager->stopAudition();
}

bool SuggestionSurfaces::accept (std::string_view id, const std::vector<std::size_t>& elements)
{
    const auto chosen = managerElements (id, elements);

    if (chosen.empty() || ! manager->accept (id, chosen))
        return false;

    announce();

    return true;
}

void SuggestionSurfaces::reject (std::string_view id, const std::string& reason)
{
    if (manager == nullptr)
        return;

    // Saying why is asking for a better one, which is a reply: the Suggestion
    // is superseded by the revision rather than merely dropped, and what the
    // producer typed is what the revision run is asked in (spec js437t).
    // Rejecting without a word is the ending it reads as.
    const auto asked = ! reason.empty() && manager->reply (id, reason).started;

    if (! asked && ! manager->reject (id))
        return;

    announce();
}

bool SuggestionSurfaces::redo (std::string_view id)
{
    if (manager == nullptr || ! manager->redo (id).started)
        return false;

    announce();

    return true;
}

//==============================================================================
std::vector<std::size_t>
    SuggestionSurfaces::managerElements (std::string_view id,
                                         const std::vector<std::size_t>& cardElements) const
{
    std::vector<std::size_t> chosen;

    if (manager == nullptr)
        return chosen;

    const auto held = manager->suggestion (id);

    if (! held.has_value())
        return chosen;

    std::vector<std::size_t> stillPending;

    for (std::size_t at = 0; at < held->elements.size(); ++at)
        if (held->elements.at (at) == duet::collab::ElementState::pending)
            stillPending.push_back (at);

    for (const auto row : cardElements)
    {
        if (row >= stillPending.size())
            return {};

        chosen.push_back (stillPending.at (row));
    }

    return chosen;
}

void SuggestionSurfaces::announce() const
{
    if (changed)
        changed();
}
} // namespace duet::app
