#include <duet/gui/Suggestions.h>

#include <algorithm>
#include <utility>

namespace duet::gui
{
namespace
{
    constexpr std::size_t notFound = static_cast<std::size_t> (-1);
} // namespace

void Suggestions::setSource (Source* newSource)
{
    source = newSource;
    pending.clear();
    unticked.clear();
    audition_.reset();
    proposedHeard = false;
    refresh();
}

void Suggestions::refresh()
{
    pending = source != nullptr ? source->pending() : std::vector<SuggestionCardView> {};

    // A tick belongs to the producer rather than to the Suggestion, so what a
    // refresh drops is only what it can no longer be about.
    std::erase_if (unticked,
                   [this] (const auto& entry)
                   {
                       return std::none_of (pending.begin(),
                                            pending.end(),
                                            [&entry] (const auto& card)
                                            { return card.id == entry.first; });
                   });

    if (audition_.has_value() && card (*audition_) == nullptr)
    {
        if (source != nullptr && proposedHeard)
            source->stopAudition();

        audition_.reset();
        proposedHeard = false;
    }
}

const SuggestionCardView* Suggestions::card (std::string_view id) const
{
    const auto index = indexOf (id);

    return index == notFound ? nullptr : &pending[index];
}

std::size_t Suggestions::indexOf (std::string_view id) const
{
    for (std::size_t index = 0; index < pending.size(); ++index)
        if (pending[index].id == id)
            return index;

    return notFound;
}

//==============================================================================
bool Suggestions::isChecked (std::string_view id, std::size_t element) const
{
    for (const auto& [suggestion, excluded] : unticked)
        if (suggestion == id)
            return std::find (excluded.begin(), excluded.end(), element) == excluded.end();

    return true;
}

void Suggestions::setChecked (std::string_view id, std::size_t element, bool checked)
{
    if (isChecked (id, element) == checked)
        return;

    auto entry = std::find_if (unticked.begin(),
                               unticked.end(),
                               [id] (const auto& candidate) { return candidate.first == id; });

    if (entry == unticked.end())
    {
        unticked.emplace_back (std::string { id }, std::vector<std::size_t> {});
        entry = std::prev (unticked.end());
    }

    if (checked)
        std::erase (entry->second, element);
    else
        entry->second.push_back (element);

    // What is heard has to be what is ticked, so a tick changed mid-Audition
    // re-applies rather than waiting for the producer to leave and come back.
    if (isAuditioning (id) && proposedHeard)
        hearProposed();
}

std::vector<std::size_t> Suggestions::checkedElements (std::string_view id) const
{
    std::vector<std::size_t> ticked;
    const auto* found = card (id);

    if (found == nullptr)
        return ticked;

    for (std::size_t element = 0; element < found->elements.size(); ++element)
        if (isChecked (id, element))
            ticked.push_back (element);

    return ticked;
}

//==============================================================================
bool Suggestions::audition (std::string_view id)
{
    if (card (id) == nullptr || checkedElements (id).empty())
        return false;

    if (audition_.has_value() && proposedHeard && source != nullptr)
        source->stopAudition();

    audition_ = std::string { id };
    proposedHeard = true;
    hearProposed();

    return true;
}

void Suggestions::stopAudition()
{
    if (! audition_.has_value())
        return;

    if (source != nullptr && proposedHeard)
        source->stopAudition();

    audition_.reset();
    proposedHeard = false;
}

bool Suggestions::isAuditioning (std::string_view id) const
{
    return audition_.has_value() && *audition_ == id;
}

void Suggestions::toggleAB()
{
    if (! audition_.has_value())
        return;

    if (proposedHeard)
    {
        if (source != nullptr)
            source->stopAudition();

        proposedHeard = false;
        return;
    }

    proposedHeard = true;
    hearProposed();
}

void Suggestions::hearProposed()
{
    if (source == nullptr || ! audition_.has_value())
        return;

    // Nothing ticked is nothing to hear. What was heard goes and the Audition
    // stays open, so that ticking something back puts the sound on again.
    if (const auto ticked = checkedElements (*audition_); ticked.empty())
        source->stopAudition();
    else
        source->audition (*audition_, ticked);
}

//==============================================================================
bool Suggestions::accept (std::string_view id)
{
    const auto ticked = checkedElements (id);

    if (ticked.empty() || source == nullptr)
        return false;

    if (isAuditioning (id))
        stopAudition();

    if (! source->accept (id, ticked))
        return false;

    // The Elements that were applied leave the Suggestion, and the ones the
    // producer said no to keep their order behind them: their ticks move down
    // with them so that what is left is the card as it was.
    for (auto& [suggestion, excluded] : unticked)
        if (suggestion == id)
        {
            std::vector<std::size_t> moved;

            for (const auto element : excluded)
            {
                const auto applied = static_cast<std::size_t> (
                    std::count_if (ticked.begin(),
                                   ticked.end(),
                                   [element] (const auto index) { return index < element; }));

                moved.push_back (element - applied);
            }

            excluded = std::move (moved);
        }

    refresh();
    return true;
}

void Suggestions::reject (std::string_view id)
{
    if (source == nullptr)
        return;

    if (isAuditioning (id))
        stopAudition();

    source->reject (id);
    std::erase_if (unticked, [id] (const auto& entry) { return entry.first == id; });
    refresh();
}

//==============================================================================
double Suggestions::intensityOf (std::string_view id, std::size_t element) const
{
    return isChecked (id, element) ? 1.0 : excludedIntensity;
}

double Suggestions::fillAlphaOf (std::string_view id) const
{
    return isAuditioning (id) ? auditionFillAlpha : pendingFillAlpha;
}

//==============================================================================
void ScriptedSuggestions::setSession (duet::model::Session* openProject)
{
    if (session != nullptr && made.has_value())
        endAudition();

    session = openProject;
    made.reset();
}

void ScriptedSuggestions::onSuggestionMade (std::function<void (std::string, std::string)> notify)
{
    suggestionMade = std::move (notify);
}

bool ScriptedSuggestions::fabricate()
{
    if (session == nullptr)
        return false;

    const auto tracks = session->tracks();

    if (tracks.empty())
        return false;

    // The track the producer would most likely be asking about: the first that
    // holds MIDI, and the first of any kind when none does.
    auto chosen = std::find_if (tracks.begin(),
                                tracks.end(),
                                [] (const auto& track)
                                { return track.kind == duet::model::TrackKind::midi; });

    if (chosen == tracks.end())
        chosen = tracks.begin();

    endAudition();

    const auto beatsPerSecond = std::max (1.0, session->tempoBpm()) / 60.0;
    const auto movedBy = movedByBeats / beatsPerSecond;
    const auto madeLength = madeClipBeats / beatsPerSecond;

    Made suggestion;

    suggestion.id = "development-" + std::to_string (++fabricated);
    suggestion.summary = "Rework the intro on " + chosen->name;
    suggestion.madeAtRevision = session->revision();

    if (! chosen->clips.empty())
    {
        const auto& first = chosen->clips.front();

        Element moved { { "Move " + first.name + " four beats later", {}, {} },
                        duet::model::Suggestion { "Move " + first.name } };
        moved.changes.moveClip (first.clip, first.startSeconds + movedBy);
        moved.view.clips.push_back (GhostClip { chosen->track,
                                                first.name,
                                                first.startSeconds + movedBy,
                                                first.lengthSeconds,
                                                first.holdsMidi });

        Element doubled { { "Double " + first.name + " after itself", {}, {} },
                          duet::model::Suggestion { "Double " + first.name } };
        doubled.changes.duplicateClip (
            first.clip, chosen->track, first.startSeconds + (2.0 * movedBy));
        doubled.view.clips.push_back (GhostClip { chosen->track,
                                                  first.name,
                                                  first.startSeconds + (2.0 * movedBy),
                                                  first.lengthSeconds,
                                                  first.holdsMidi });

        suggestion.elements.push_back (std::move (moved));
        suggestion.elements.push_back (std::move (doubled));
    }
    else if (chosen->kind == duet::model::TrackKind::midi)
    {
        for (auto index = 0; index < 2; ++index)
        {
            const auto at = static_cast<double> (index) * madeLength;
            const auto name = index == 0 ? std::string { "Idea" } : std::string { "Answer" };

            Element added { { "Add " + name + " to " + chosen->name, {}, {} },
                            duet::model::Suggestion { "Add " + name } };
            added.changes.insertMidiClip (chosen->track, name, at, madeLength);
            added.view.clips.push_back (GhostClip { chosen->track, name, at, madeLength, true });

            suggestion.elements.push_back (std::move (added));
        }
    }

    Element lifted { { "Bring " + chosen->name + " up to -3.0 dB", {}, {} },
                     duet::model::Suggestion { "Set " + chosen->name + "'s level" } };
    lifted.changes.setTrackVolumeDb (chosen->track, proposedLevelDb);
    lifted.view.faders.push_back (GhostFader { chosen->track, proposedLevelDb });

    suggestion.elements.push_back (std::move (lifted));
    made = std::move (suggestion);

    if (suggestionMade)
        suggestionMade (made->id, made->summary);

    return true;
}

std::vector<SuggestionCardView> ScriptedSuggestions::pending()
{
    if (! made.has_value())
        return {};

    SuggestionCardView card;

    card.id = made->id;
    card.summary = made->summary;

    // While an Audition is live the project holds the Suggestion's own changes
    // rather than the producer's, so there is nothing to measure it against.
    card.stale = session != nullptr && ! session->isAuditioning()
                 && session->revision() != made->madeAtRevision;

    for (const auto& element : made->elements)
        card.elements.push_back (element.view);

    return { card };
}

duet::model::Suggestion ScriptedSuggestions::applicable (const std::vector<std::size_t>& elements,
                                                         std::string_view name) const
{
    duet::model::Suggestion applied { std::string { name } };

    if (! made.has_value())
        return applied;

    for (const auto element : elements)
        if (element < made->elements.size())
            applied.append (made->elements[element].changes);

    return applied;
}

void ScriptedSuggestions::endAudition()
{
    if (session == nullptr || ! session->isAuditioning())
        return;

    session->stopAudition();

    if (made.has_value())
        made->madeAtRevision += session->revision() - revisionEnteringAudition;
}

bool ScriptedSuggestions::audition (std::string_view id, const std::vector<std::size_t>& elements)
{
    if (session == nullptr || ! made.has_value() || made->id != id || elements.empty())
        return false;

    endAudition();
    revisionEnteringAudition = session->revision();

    return session->auditionSuggestion (applicable (elements, made->summary));
}

void ScriptedSuggestions::stopAudition() { endAudition(); }

bool ScriptedSuggestions::accept (std::string_view id, const std::vector<std::size_t>& elements)
{
    if (session == nullptr || ! made.has_value() || made->id != id || elements.empty())
        return false;

    const auto name =
        elements.size() == 1 ? made->elements[elements.front()].view.description : made->summary;

    if (! session->acceptSuggestion (applicable (elements, name)))
        return false;

    // What was applied leaves the Suggestion, and what is left keeps its order.
    std::vector<Element> left;

    for (std::size_t element = 0; element < made->elements.size(); ++element)
        if (std::find (elements.begin(), elements.end(), element) == elements.end())
            left.push_back (made->elements[element]);

    if (left.empty())
    {
        made.reset();
        return true;
    }

    made->elements = std::move (left);

    // A Suggestion's own acceptance is not the project moving out from under
    // it, so what is left is measured against the project it now stands on.
    made->madeAtRevision = session->revision();
    return true;
}

void ScriptedSuggestions::reject (std::string_view id)
{
    if (session == nullptr || ! made.has_value() || made->id != id)
        return;

    session->rejectSuggestion (applicable ({}, made->summary));
    endAudition();
    made.reset();
}
} // namespace duet::gui
