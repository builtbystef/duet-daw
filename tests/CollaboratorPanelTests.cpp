#include <duet/gui/CollaboratorPanel.h>

#include <catch2/catch_test_macros.hpp>

using duet::gui::clipsSelected;
using duet::gui::CollaboratorPanel;
using duet::gui::EntryKind;
using duet::gui::noSelection;
using duet::gui::ScriptedCollaborator;
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

TEST_CASE ("commentary answers a message as its own kind of entry")
{
    CollaboratorPanel panel;

    panel.setComposerText ("Tighten this up");
    panel.send();
    panel.say ("Nudged the hats off the grid.");

    REQUIRE (panel.conversation().size() == 2);
    REQUIRE (panel.conversation().back().kind == EntryKind::commentary);
    REQUIRE (panel.conversation().back().text == "Nudged the hats off the grid.");

    // Commentary is the Collaborator's, so it carries no selection chip.
    REQUIRE (panel.conversation().back().context.empty());
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
    panel.failTaskRun ("the Collaborator is not connected");

    REQUIRE_FALSE (panel.taskRunning());
    REQUIRE (panel.composerEnabled());
    REQUIRE (panel.conversation().size() == 1);
    REQUIRE (panel.conversation().back().kind == EntryKind::failure);
    REQUIRE (panel.conversation().back().text
             == "That task failed: the Collaborator is not "
                "connected. Nothing changed.");

    // The panel is ready for the next message rather than stuck on the failed
    // one: a failure is a line in the conversation and nothing more.
    panel.setComposerText ("Try again");

    REQUIRE (panel.canSend());
}

TEST_CASE ("the development source answers a sent message with a Task Run and commentary")
{
    CollaboratorPanel panel;
    ScriptedCollaborator scripted;

    panel.setSource (&scripted);
    panel.setComposerText ("Tighten this up");
    panel.send();

    REQUIRE (panel.taskRunning());

    // Time on the card is time the source is counting too, so one tick drives
    // the whole thing.
    panel.advance (ScriptedCollaborator::taskRunSeconds / 2.0);

    REQUIRE (panel.taskRunning());

    panel.advance (ScriptedCollaborator::taskRunSeconds);

    REQUIRE_FALSE (panel.taskRunning());
    REQUIRE (panel.conversation().size() == 2);
    REQUIRE (panel.conversation().back().kind == EntryKind::commentary);
}

TEST_CASE ("the development source reaches a failed run too, without a socket or a network")
{
    CollaboratorPanel panel;
    ScriptedCollaborator scripted;

    panel.setSource (&scripted);

    // The runs alternate, so the second one is the failure a reviewer needs to
    // be able to see.
    panel.setComposerText ("Tighten this up");
    panel.send();
    panel.advance (ScriptedCollaborator::taskRunSeconds);

    panel.setComposerText ("Try that again");
    panel.send();
    panel.advance (ScriptedCollaborator::taskRunSeconds);

    REQUIRE_FALSE (panel.taskRunning());
    REQUIRE (panel.conversation().back().kind == EntryKind::failure);
}

TEST_CASE ("canceling stops the development source's run where it stands")
{
    CollaboratorPanel panel;
    ScriptedCollaborator scripted;

    panel.setSource (&scripted);
    panel.setComposerText ("Tighten this up");
    panel.send();
    panel.requestCancel();

    REQUIRE_FALSE (panel.taskRunning());
    REQUIRE (panel.conversation().back().kind == EntryKind::notice);

    // The run the producer stopped does not come back and finish itself later.
    const auto afterCancel = panel.conversation().size();

    panel.advance (ScriptedCollaborator::taskRunSeconds * 4.0);

    REQUIRE (panel.conversation().size() == afterCancel);
    REQUIRE_FALSE (panel.taskRunning());
}
