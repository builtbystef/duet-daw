---
id: 4jipx2
title: Collaborator panel shell and styling
state: done
assignee: claude
priority: high
depends_on:
    - nelbwc
parent: 535bbo
created: 2026-08-12T03:51:21Z
updated: 2026-08-25T16:05:32Z
---

## What to build

The right dock, in Graphite: the surface the Collaborator speaks from. A scrolling conversation of producer messages and Collaborator commentary in accent bubbles, a composer at the bottom, a selection context chip on the producer's message showing what was selected when it was sent, quick-prompt chips above the composer that adapt to the current selection, and the Task Run card with its spinner, its "you can keep editing" hint, Cancel, and rotating friendly status phrases. The ✦ badge and the teal accent belong to these surfaces and nowhere else.

This slice owns placement, styling, and the panel's own state — not the mechanics. The Collaborator service, the socket, Task Runs and the Duet Loop are spec js437t's; here the panel is driven by a development-only conversation source that can produce messages, a running Task Run, a cancellation, and a failure, so every state is reachable and reviewable before the service exists.

## Acceptance criteria

- [ ] The panel docks right, toggles with C and from the Duet menu, and its width and visibility persist with the project's view state.
- [ ] Producer messages and Collaborator commentary render distinctly, commentary in accent bubbles, and the conversation scrolls with the newest message in view; a long conversation scrolls without the composer moving.
- [ ] The context chip, worked: with two clips selected, sending a message shows a chip reading two clips on that message; with a track selected it names the track; with nothing selected there is no chip. The chip records the selection at send time and does not change when the selection later changes.
- [ ] Quick-prompt chips change with the selection — a clip selection, a track selection and an empty selection each offer their own set — and clicking one fills the composer without sending.
- [ ] A running Task Run shows the card with a spinner, the keep-editing hint, a Cancel button, and status phrases that rotate; the composer is disabled while it runs and re-enabled when it ends.
- [ ] Cancel ends the run and leaves a "task canceled, nothing changed" line in the conversation.
- [ ] A failed run leaves one plain error line in the conversation, and the rest of the app keeps working — playback, editing and saving are unaffected.
- [ ] Nothing outside this panel and the Suggestion surfaces uses the teal accent token or the ✦ badge.
- [ ] The panel reads and styles only; every state above is reachable from the development-only source with no AI backend, no socket and no network.
- [ ] Typing in the composer suppresses bare-letter shortcuts, and Escape returns focus to the last focused surface without clearing the composer's text.

## Notes

**claude** — 2026-08-25T16:05:32Z

Completed the Collaborator panel shell. The right dock is now the panel rather than an empty area: `CollaboratorPanel` (paintless, duet::gui) holds the conversation, the composer, the quick prompts, the frozen context chip and the Task Run card's state; `CollaboratorPanelCanvas` (duet::gui_components) paints it and forwards the producer's gestures, and is the one surface in the interface that draws the reserved teal and the ✦ badge. Naming follows the module's convention (PianoRoll / PianoRollCanvas). The panel reads and styles only — it performs no Action and never reads the project.

Seams used, both agreed by spec 535bbo: the view-model seam for everything the panel's own state answers (tests/CollaboratorPanelTests.cpp), and the component seam for what only a window can answer — the dock is the panel, C still toggles it, the composer is a text field for the keyboard policy, Send chips a message with what the shell says is selected, a long conversation scrolls to its newest entry while the composer stays put, the card appears and holds the composer, Cancel ends the run, a failure leaves the rest of the window working, Escape hands the keyboard back with the composer's text intact (tests/gui/CollaboratorPanelCanvasTests.cpp).

Decisions made:

- What answers the producer is behind `CollaboratorPanel::Source`, which spec js437t's service will implement. Until then `ScriptedCollaborator` is the development-only stand-in: no AI backend, no socket, no network. Its runs alternate — the first ends in commentary, the second fails — so both endings are reachable by hand, and Cancel is the third. It goes when the service arrives.
- The context chip reads counts as words to ten ("two clips"), a track by its name, and nothing at all when nothing is selected. It is copied onto the message at send and never re-read.
- What "selected" means is the shell's answer, not the panel's: a clip selection first, otherwise the track the producer last put their hand on (ArrangementView::focusedTrack, the accessor this slice added — this app has no separate track-selection model). A note selection reads as no context, which is outside what the criteria name.
- The badge is drawn as a path rather than typed. Not every copy of the typeface carries U+2726, and JUCE asserts on a juce::String built from a non-ASCII const char*, so every literal in this code is ASCII.
- The reserved-accent criterion is about the whole tree rather than any object's behaviour, so it is a guard over the module sources (tests/GraphiteTokenTests.cpp): only Tokens.*, Collaborator* and Suggestion* files may name ColourToken::collaborator or the badge. Verified it goes red when an offender is added elsewhere.
- The panel ticks at 8 Hz while idle and 30 Hz only while a run is on — a spinner is worth thirty frames a second and a selection poll is not — and repaints only when something it shows has changed.

Facts worth having for later interface slices:

- juce::TextEditor::onTextChange arrives on a later pass of the message loop, not on the keystroke. The panel reads the editor in refresh() instead, so Send lights the moment there is something to send and a test needs no message loop.
- juce::Font::getStringWidth is gone in JUCE 9; juce::GlyphArrangement::getStringWidth (font, text) is the replacement.
- A dock whose pixel width comes from the view state keeps that width when the interface scale changes, so a same-bounds setBounds skips resized() and the panel's own chrome would never re-lay out. The panel listens to Appearance itself for that reason.

Paint stays untested, per the spec. It was verified once by rendering the panel through a disposable probe in the gui suite, which was removed again; the panel reads as Graphite with the teal confined to the commentary bubble, the badge and the card.

Checks: clang-format clean, full scripts/lint.sh sweep clean, full build, and all 271 CTest entries pass with the 8 expected hardware skips. The sandbox needs XDG_RUNTIME_DIR pointing at a writable directory for Catch2's generated test listing; that is the environment, not the repository.
