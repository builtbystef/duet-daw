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

    /** What the development source says when a scripted run succeeds. Placeholder
        prose: the Collaborator of spec js437t is what will have something to
        say.
    */
    constexpr std::array scriptedCommentary {
        "Had a look. Nothing has changed in the project — this is the panel, not the "
        "Collaborator.",
        "Noted. A real answer arrives with the Collaborator service.",
        "That would be a Suggestion once there is something behind this panel."
    };

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

bool CollaboratorPanel::canSend() const { return ! isBlank (composer); }

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

void CollaboratorPanel::say (std::string commentary)
{
    entries.push_back ({ EntryKind::commentary, std::move (commentary), {} });
}

//==============================================================================
void CollaboratorPanel::beginTaskRun()
{
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
    entries.push_back (
        { EntryKind::failure, "That task failed: " + reason + ". Nothing changed.", {} });
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
    if (source != nullptr)
        source->advance (*this, seconds);

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
    phrase.clear();
    phraseElapsed = 0.0;
    phraseIndex = 0;
}

//==============================================================================
void ScriptedCollaborator::producerSent (CollaboratorPanel& panel, const std::string& message)
{
    static_cast<void> (message);

    remaining = taskRunSeconds;
    running = true;
    panel.beginTaskRun();
}

void ScriptedCollaborator::taskRunCanceled (CollaboratorPanel& panel)
{
    static_cast<void> (panel);

    running = false;
    remaining = 0.0;
}

void ScriptedCollaborator::advance (CollaboratorPanel& panel, double seconds)
{
    if (! running)
        return;

    remaining -= seconds;

    if (remaining > 0.0)
        return;

    running = false;

    // The endings alternate, so a reviewer reaches the failed one by sending a
    // second message rather than by editing this file.
    const auto fails = ++runsSoFar % 2 == 0;

    if (fails)
    {
        panel.failTaskRun ("the Collaborator is not connected yet");
        return;
    }

    panel.say (scriptedCommentary.at ((runsSoFar / 2) % scriptedCommentary.size()));
    panel.finishTaskRun();
}
} // namespace duet::gui
