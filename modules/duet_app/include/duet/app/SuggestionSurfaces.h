#pragma once

#include <duet/collab/SuggestionManager.h>

#include <duet/gui/Suggestions.h>
#include <duet/model/Session.h>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace duet::app
{
/** The Suggestion manager, as the three surfaces that show a Suggestion read
    it.

    The card in the conversation, the ghosts on the timeline and the ghost marks
    in the mixer all read one `duet::gui::Suggestions`, and what that reads is
    this: the manager holds the Duet Loop and this says it in the engine-free
    shape the interface speaks. It lives in `duet_app` because it is the only
    place that may name both — the interface links no engine and the manager
    links no JUCE.

    **Ghosts are read off the operations.** Where a Suggestion's clips would
    land and what its faders would read are worked out from the `suggest` call
    the Collaborator made, against the project as it stands: an operation that
    moves a clip is drawn where the clip would be, one that makes a clip is
    drawn where it would appear, and one that sets a level is drawn beside the
    fader it would move. An operation that draws nothing — a note edit, a
    plugin, a deletion — is still applied by an acceptance; it just has no
    picture of its own, and the Element's own words are what the producer reads
    it as.

    **Elements the producer has resolved are gone from the card.** A card row is
    an Element still pending, so the indices the interface counts in are the
    pending ones and this is what maps them back onto the manager's.

    Every member is called on the message thread. The manager and the session
    are read and never owned, and none of either is a surface with no ghosts on
    it.
*/
class SuggestionSurfaces final : public duet::gui::Suggestions::Source
{
public:
    /** Told whenever a gesture on a card has resolved a Suggestion or asked for
        another one, which is when the conversation and its History have to be
        read again.
    */
    using Changed = std::function<void()>;

    explicit SuggestionSurfaces (Changed onChange = {});
    ~SuggestionSurfaces() override;

    SuggestionSurfaces (const SuggestionSurfaces&) = delete;
    SuggestionSurfaces (SuggestionSurfaces&&) = delete;
    SuggestionSurfaces& operator= (const SuggestionSurfaces&) = delete;
    SuggestionSurfaces& operator= (SuggestionSurfaces&&) = delete;

    /** The manager these surfaces read, and the project its ghosts are placed
        against. Both or neither: a manager holds Suggestions about one project.
    */
    void setManager (duet::collab::SuggestionManager* suggestionManager,
                     duet::model::Session* openProject);

    //==============================================================================
    [[nodiscard]] std::vector<duet::gui::SuggestionCardView> pending() override;
    bool audition (std::string_view id, const std::vector<std::size_t>& elements) override;
    void stopAudition() override;
    bool accept (std::string_view id, const std::vector<std::size_t>& elements) override;
    void reject (std::string_view id, const std::string& reason) override;
    bool redo (std::string_view id) override;

private:
    /** The manager's own numbering of the Elements a card names, the card
        counting only the ones still pending. Empty when one of them is not
        there, which is a gesture on a card that has moved on.
    */
    [[nodiscard]] std::vector<std::size_t>
        managerElements (std::string_view id, const std::vector<std::size_t>& cardElements) const;

    void announce() const;

    duet::collab::SuggestionManager* manager = nullptr;
    duet::model::Session* session = nullptr;
    Changed changed;
};
} // namespace duet::app
