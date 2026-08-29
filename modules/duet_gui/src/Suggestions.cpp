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

void Suggestions::reject (std::string_view id, const std::string& reason)
{
    if (source == nullptr)
        return;

    if (isAuditioning (id))
        stopAudition();

    source->reject (id, reason);
    forget (id);
    refresh();
}

bool Suggestions::redo (std::string_view id)
{
    if (source == nullptr)
        return false;

    if (isAuditioning (id))
        stopAudition();

    if (! source->redo (id))
        return false;

    forget (id);
    refresh();

    return true;
}

void Suggestions::forget (std::string_view id)
{
    std::erase_if (unticked, [id] (const auto& entry) { return entry.first == id; });
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
} // namespace duet::gui
