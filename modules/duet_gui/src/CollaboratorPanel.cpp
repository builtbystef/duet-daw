#include <duet/gui/CollaboratorPanel.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace duet::gui
{
namespace
{
    /** The counts a producer reads as words rather than as digits. Past the end
        of this the digits are what read faster.
    */
    constexpr std::array numberWords { "no",  "one",   "two",   "three", "four", "five",
                                       "six", "seven", "eight", "nine",  "ten" };

    [[nodiscard]] std::string spelled (int count)
    {
        if (count >= 0 && static_cast<std::size_t> (count) < numberWords.size())
            return numberWords.at (static_cast<std::size_t> (count));

        return std::to_string (count);
    }

    /** What the card says while a run is on. Friendly, and honest about the
        fact that a Task Run takes as long as it takes: they rotate rather than
        counting anything down.
    */
    constexpr std::array statusPhrases { "Listening to the project",
                                         "Working out what you mean",
                                         "Trying an idea",
                                         "Nearly there" };

    [[nodiscard]] bool isBlank (const std::string& text)
    {
        return std::all_of (text.begin(),
                            text.end(),
                            [] (unsigned char character) { return std::isspace (character) != 0; });
    }
} // namespace

//==============================================================================
SelectionContext noSelection() { return {}; }

SelectionContext clipsSelected (int count) { return { SelectionScope::clips, count, {} }; }

SelectionContext trackSelected (std::string name)
{
    return { SelectionScope::track, 0, std::move (name) };
}

std::string contextChipText (const SelectionContext& context)
{
    switch (context.scope)
    {
        case SelectionScope::clips:
            return spelled (context.clipCount) + (context.clipCount == 1 ? " clip" : " clips");

        case SelectionScope::track:
            return context.trackName;

        case SelectionScope::nothing:
            break;
    }

    return {};
}

//==============================================================================
void CollaboratorPanel::setSelectionContext (SelectionContext context)
{
    selection = std::move (context);
}

void CollaboratorPanel::setComposerText (std::string text) { composer = std::move (text); }

bool CollaboratorPanel::canSend() const { return ! running && ! isBlank (composer); }

void CollaboratorPanel::send()
{
    if (! canSend())
        return;

    auto message = composer;

    entries.push_back ({ EntryKind::producer, message, contextChipText (selection) });
    composer.clear();

    if (source != nullptr)
        source->producerSent (*this, message);
}

std::vector<std::string> CollaboratorPanel::quickPrompts() const
{
    switch (selection.scope)
    {
        case SelectionScope::clips:
            return { "Tighten this up", "Suggest a variation", "What is wrong with this part?" };

        case SelectionScope::track:
            return { "Write a part that answers this track",
                     "Sit this track in the mix",
                     "Suggest an effect for this track" };

        case SelectionScope::nothing:
            break;
    }

    return { "What should I work on next?", "Sketch a drum groove", "Suggest a chord progression" };
}

void CollaboratorPanel::useQuickPrompt (std::size_t index)
{
    const auto prompts = quickPrompts();

    if (index < prompts.size())
        composer = prompts[index];
}

void CollaboratorPanel::streamCommentary (const std::string& delta,
                                          std::vector<EstimateMarkLine> estimates)
{
    if (! streaming)
    {
        entries.push_back ({ EntryKind::commentary, {}, {}, {} });
        streamingInto = entries.size() - 1;
        streaming = true;
    }

    auto& entry = entries.at (streamingInto);
    entry.text += delta;

    // The mark is the ledger, so a run handed a guess halfway through marks the
    // whole of what it said: the entry is one thing the producer reads, and the
    // ledger only ever grows within a run.
    if (! estimates.empty())
        entry.estimates = std::move (estimates);
}

void CollaboratorPanel::toggleEstimates (std::size_t entry)
{
    if (entry >= entries.size() || entries.at (entry).estimates.empty())
        return;

    auto& marked = entries.at (entry);
    marked.estimatesOpen = ! marked.estimatesOpen;
}

void CollaboratorPanel::recordToolCall (std::string tool, std::string arguments, std::string result)
{
    if (! tracing)
        return;

    trace.push_back ({ std::move (tool), std::move (arguments), std::move (result) });
}

void CollaboratorPanel::setToolTraceEnabled (bool shouldKeepTrace)
{
    tracing = shouldKeepTrace;

    if (! tracing)
        trace.clear();
}

void CollaboratorPanel::setHistory (std::vector<ResolvedSuggestion> resolvedSuggestions)
{
    resolved = std::move (resolvedSuggestions);
}

void CollaboratorPanel::showSuggestion (std::string id, std::string summary)
{
    entries.push_back ({ EntryKind::suggestion, std::move (summary), {}, std::move (id) });
}

//==============================================================================
void CollaboratorPanel::beginTaskRun()
{
    // A trace is one run's, so what the run before it did goes with it.
    trace.clear();
    streaming = false;
    running = true;
    phraseIndex = 0;
    phraseElapsed = 0.0;
    phrase = statusPhrases.front();
}

void CollaboratorPanel::finishTaskRun() { endRun(); }

void CollaboratorPanel::cancelTaskRun()
{
    if (! running)
        return;

    endRun();
    entries.push_back ({ EntryKind::notice, cancelNotice, {} });
}

void CollaboratorPanel::failTaskRun (const std::string& reason)
{
    if (! running)
        return;

    endRun();

    // The reason is the line. What failed a run is the Collaborator's own
    // sentence about it, already written for the producer to read, and putting
    // it inside a sentence of the panel's would make two out of one.
    entries.push_back ({ EntryKind::failure, reason.empty() ? failureNotice : reason, {} });
}

void CollaboratorPanel::requestCancel()
{
    if (! running)
        return;

    cancelTaskRun();

    if (source != nullptr)
        source->taskRunCanceled (*this);
}

void CollaboratorPanel::advance (double seconds)
{
    if (! running)
        return;

    phraseElapsed += seconds;

    while (phraseElapsed >= statusPhraseSeconds)
    {
        phraseElapsed -= statusPhraseSeconds;
        phraseIndex = (phraseIndex + 1) % statusPhrases.size();
    }

    phrase = statusPhrases.at (phraseIndex);
}

void CollaboratorPanel::endRun()
{
    running = false;
    streaming = false;
    phrase.clear();
    phraseElapsed = 0.0;
    phraseIndex = 0;
}
} // namespace duet::gui
