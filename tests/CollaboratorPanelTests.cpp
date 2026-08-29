#include <duet/gui/CollaboratorPanel.h>

#include <catch2/catch_test_macros.hpp>

using duet::gui::clipsSelected;
using duet::gui::CollaboratorPanel;
using duet::gui::EntryKind;
using duet::gui::EstimateMarkLine;
using duet::gui::noSelection;
using duet::gui::ResolvedSuggestion;
using duet::gui::trackSelected;

TEST_CASE ("a message the producer sends carries the selection it was sent with")
{
    CollaboratorPanel panel;

    panel.setSelectionContext (clipsSelected (2));
    panel.setComposerText ("Tighten this up");
    panel.send();

    REQUIRE (panel.conversation().size() == 1);

    const auto& sent = panel.conversation().front();

    REQUIRE (sent.kind == EntryKind::producer);
    REQUIRE (sent.text == "Tighten this up");
    REQUIRE (sent.context == "two clips");

    // The composer is emptied by sending, and the chip is what was selected
    // then rather than what is selected now.
    REQUIRE (panel.composerText().empty());

    panel.setSelectionContext (trackSelected ("Drums"));

    REQUIRE (panel.conversation().front().context == "two clips");
}

TEST_CASE ("a message sent with a track selected names the track, and one sent with nothing "
           "selected carries no chip")
{
    CollaboratorPanel panel;

    panel.setSelectionContext (trackSelected ("Drums"));
    panel.setComposerText ("Add a fill");
    panel.send();

    REQUIRE (panel.conversation().back().context == "Drums");

    panel.setSelectionContext (noSelection());
    panel.setComposerText ("What next?");
    panel.send();

    REQUIRE (panel.conversation().back().context.empty());
}

TEST_CASE ("commentary is read as it streams rather than when the run finishes")
{
    CollaboratorPanel panel;

    panel.setComposerText ("Tighten this up");
    panel.send();
    panel.beginTaskRun();

    panel.streamCommentary ("The hats", {});

    // One entry, part-written: this is what the producer is reading while the
    // run is still going.
    REQUIRE (panel.conversation().size() == 2);
    REQUIRE (panel.conversation().back().kind == EntryKind::commentary);
    REQUIRE (panel.conversation().back().text == "The hats");

    panel.streamCommentary (" are late.", {});

    REQUIRE (panel.conversation().size() == 2);
    REQUIRE (panel.conversation().back().text == "The hats are late.");

    // Commentary is the Collaborator's, so it carries no selection chip.
    REQUIRE (panel.conversation().back().context.empty());

    // The next run says its own thing rather than lengthening this one.
    panel.finishTaskRun();
    panel.beginTaskRun();
    panel.streamCommentary ("And the snare.", {});

    REQUIRE (panel.conversation().size() == 3);
    REQUIRE (panel.conversation().back().text == "And the snare.");
}

TEST_CASE ("commentary that rests on a guess is marked, and the mark opens onto the ledger")
{
    CollaboratorPanel panel;

    panel.beginTaskRun();
    panel.streamCommentary ("It reads as ", {});

    REQUIRE (panel.conversation().back().estimates.empty());

    const std::vector<EstimateMarkLine> ledger {
        { "key", "C major", "krumhansl-schmuckler", 0.72 }
    };

    panel.streamCommentary ("C major.", ledger);

    // The mark is on the whole entry, not on the half of it that arrived after
    // the guess: the entry is one thing the producer reads.
    const auto marked = panel.conversation().size() - 1;

    REQUIRE (panel.conversation().at (marked).text == "It reads as C major.");
    REQUIRE (panel.conversation().at (marked).estimates.size() == 1);
    REQUIRE (panel.conversation().at (marked).estimates.front().field == "key");
    REQUIRE (panel.conversation().at (marked).estimates.front().value == "C major");
    REQUIRE (panel.conversation().at (marked).estimates.front().method == "krumhansl-schmuckler");
    REQUIRE (panel.conversation().at (marked).estimates.front().confidence == 0.72);
    REQUIRE_FALSE (panel.conversation().at (marked).estimatesOpen);

    panel.toggleEstimates (marked);

    REQUIRE (panel.conversation().at (marked).estimatesOpen);

    panel.toggleEstimates (marked);

    REQUIRE_FALSE (panel.conversation().at (marked).estimatesOpen);

    // A run that was handed no guess has no mark to open.
    panel.finishTaskRun();
    panel.beginTaskRun();
    panel.streamCommentary ("Measured, this time.", {});

    const auto plain = panel.conversation().size() - 1;

    REQUIRE (panel.conversation().at (plain).estimates.empty());

    panel.toggleEstimates (plain);

    REQUIRE_FALSE (panel.conversation().at (plain).estimatesOpen);
}

TEST_CASE ("the raw tool-call trace is one run's, and an ordinary build keeps none of it")
{
    CollaboratorPanel panel;

    // What kind of build this is decides what the panel does by default, and a
    // test says which of the two it is asserting about.
    REQUIRE (panel.toolTraceEnabled() == duet::gui::developmentBuild);

    panel.setToolTraceEnabled (true);
    panel.beginTaskRun();
    panel.recordToolCall ("list_tracks", "{}", R"({"tracks":[]})");
    panel.recordToolCall ("get_midi", R"({"trackId":"track-3"})", R"({"clips":[]})");

    REQUIRE (panel.toolTrace().size() == 2);
    REQUIRE (panel.toolTrace().front().tool == "list_tracks");
    REQUIRE (panel.toolTrace().back().tool == "get_midi");
    REQUIRE (panel.toolTrace().back().arguments == R"({"trackId":"track-3"})");
    REQUIRE (panel.toolTrace().back().result == R"({"clips":[]})");

    // A trace is of a run, so the next run starts with an empty one.
    panel.finishTaskRun();
    panel.beginTaskRun();

    REQUIRE (panel.toolTrace().empty());

    // An ordinary build keeps nothing at all, so there is nothing to show and
    // the status phrases are what is left.
    panel.setToolTraceEnabled (false);
    panel.recordToolCall ("list_tracks", "{}", R"({"tracks":[]})");

    REQUIRE (panel.toolTrace().empty());
    REQUIRE_FALSE (panel.statusPhrase().empty());
}

TEST_CASE ("the History section is the resolved Suggestions it is handed")
{
    CollaboratorPanel panel;

    REQUIRE (panel.history().empty());

    panel.setHistory (std::vector<ResolvedSuggestion> { { "Sidechain the bass", "accepted" },
                                                        { "Widen the pad", "rejected" } });

    REQUIRE (panel.history().size() == 2);
    REQUIRE (panel.history().front().summary == "Sidechain the bass");
    REQUIRE (panel.history().front().outcome == "accepted");
    REQUIRE (panel.history().back().outcome == "rejected");
}

TEST_CASE ("an empty composer has nothing to send")
{
    CollaboratorPanel panel;

    REQUIRE_FALSE (panel.canSend());

    panel.setComposerText ("   ");

    REQUIRE_FALSE (panel.canSend());

    panel.send();

    REQUIRE (panel.conversation().empty());

    panel.setComposerText ("Sketch a bassline");

    REQUIRE (panel.canSend());

    // One run at a time: a composer held by a run has nothing to send either.
    panel.beginTaskRun();

    REQUIRE_FALSE (panel.canSend());

    panel.send();

    REQUIRE (panel.conversation().empty());
}

TEST_CASE ("the quick prompts follow the selection, and one fills the composer without sending it")
{
    CollaboratorPanel panel;

    const auto withNothing = panel.quickPrompts();

    panel.setSelectionContext (clipsSelected (2));

    const auto withClips = panel.quickPrompts();

    panel.setSelectionContext (trackSelected ("Drums"));

    const auto withATrack = panel.quickPrompts();

    REQUIRE_FALSE (withNothing.empty());
    REQUIRE (withClips != withNothing);
    REQUIRE (withATrack != withClips);
    REQUIRE (withATrack != withNothing);

    panel.useQuickPrompt (0);

    REQUIRE (panel.composerText() == withATrack.front());
    REQUIRE (panel.conversation().empty());

    // A prompt the set does not have leaves the composer as the producer had it.
    panel.useQuickPrompt (withATrack.size());

    REQUIRE (panel.composerText() == withATrack.front());
}

TEST_CASE ("a running Task Run holds the composer and rotates its status phrase")
{
    CollaboratorPanel panel;

    REQUIRE_FALSE (panel.taskRunning());
    REQUIRE (panel.composerEnabled());

    panel.setComposerText ("Tighten this up");
    panel.send();
    panel.beginTaskRun();

    REQUIRE (panel.taskRunning());
    REQUIRE_FALSE (panel.composerEnabled());

    const auto first = panel.statusPhrase();

    REQUIRE_FALSE (first.empty());

    // The phrase stands until its turn is over, and then it is another one.
    panel.advance (CollaboratorPanel::statusPhraseSeconds / 2.0);

    REQUIRE (panel.statusPhrase() == first);

    panel.advance (CollaboratorPanel::statusPhraseSeconds);

    REQUIRE (panel.statusPhrase() != first);

    // They rotate rather than running out: a long run keeps saying something.
    for (auto turn = 0; turn < 12; ++turn)
        panel.advance (CollaboratorPanel::statusPhraseSeconds);

    REQUIRE_FALSE (panel.statusPhrase().empty());

    panel.finishTaskRun();

    REQUIRE_FALSE (panel.taskRunning());
    REQUIRE (panel.composerEnabled());
    REQUIRE (panel.statusPhrase().empty());
}

TEST_CASE ("canceling a Task Run says that nothing changed")
{
    CollaboratorPanel panel;

    panel.beginTaskRun();
    panel.cancelTaskRun();

    REQUIRE_FALSE (panel.taskRunning());
    REQUIRE (panel.composerEnabled());
    REQUIRE (panel.conversation().size() == 1);
    REQUIRE (panel.conversation().back().kind == EntryKind::notice);
    REQUIRE (panel.conversation().back().text == CollaboratorPanel::cancelNotice);

    // A cancel with no run to cancel says nothing.
    panel.cancelTaskRun();

    REQUIRE (panel.conversation().size() == 1);
}

TEST_CASE ("a Task Run that fails leaves one plain line and takes the panel no further")
{
    CollaboratorPanel panel;

    panel.beginTaskRun();
    panel.failTaskRun ("The Collaborator isn't working right now - try again later.");

    REQUIRE_FALSE (panel.taskRunning());
    REQUIRE (panel.composerEnabled());
    REQUIRE (panel.conversation().size() == 1);
    REQUIRE (panel.conversation().back().kind == EntryKind::failure);

    // The reason is the line. What failed a run is the Collaborator's own
    // sentence about it, already written for the producer to read.
    REQUIRE (panel.conversation().back().text
             == "The Collaborator isn't working right now - try again later.");

    // A run that failed saying nothing still leaves one line, so the producer
    // is never left looking at a card that stopped.
    panel.beginTaskRun();
    panel.failTaskRun ({});

    REQUIRE (panel.conversation().size() == 2);
    REQUIRE (panel.conversation().back().text == CollaboratorPanel::failureNotice);

    // The panel is ready for the next message rather than stuck on the failed
    // one: a failure is a line in the conversation and nothing more.
    panel.setComposerText ("Try again");

    REQUIRE (panel.canSend());
}

TEST_CASE ("a revision takes the place of the card it revises rather than standing beside it")
{
    CollaboratorPanel panel;

    panel.setComposerText ("give me a turnaround into bar 9");
    panel.send();
    panel.showSuggestion ("sug-1", "A turnaround on the bass", {});

    REQUIRE (panel.conversation().size() == 2);
    REQUIRE (panel.conversation().back().kind == EntryKind::suggestion);
    REQUIRE (panel.conversation().back().suggestion == "sug-1");

    // The producer said what was wrong with it and the Collaborator answered
    // with a better one: the conversation gains no second card, and the one it
    // has is the revision.
    panel.showSuggestion ("sug-2", "A quieter turnaround", "sug-1");

    REQUIRE (panel.conversation().size() == 2);

    const auto& card = panel.conversation().back();

    REQUIRE (card.kind == EntryKind::suggestion);
    REQUIRE (card.suggestion == "sug-2");
    REQUIRE (card.text == "A quieter turnaround");

    // A Suggestion that revises one the conversation never showed is a card of
    // its own, wherever the one it revises went.
    panel.showSuggestion ("sug-4", "Something else again", "sug-3");

    REQUIRE (panel.conversation().size() == 3);
    REQUIRE (panel.conversation().back().suggestion == "sug-4");
}
