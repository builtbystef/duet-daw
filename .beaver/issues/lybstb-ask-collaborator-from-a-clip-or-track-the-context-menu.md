---
id: lybstb
title: 'Ask Collaborator from a clip or track: the context-menu entry and the implicit context'
state: done
assignee: claude
priority: medium
depends_on:
    - 7tw2tz
    - nelbwc
parent: js437t
created: 2026-08-12T04:04:08Z
updated: 2026-08-29T01:23:09Z
---

## What to build

The one-gesture path to asking about *this thing here*. The ordinary right-click menu of a clip and of a track gains an "Ask Collaborator" entry carrying the ✦ badge. Choosing it makes that item the implicit context of the next message — it becomes what the run's opening context names and what the chip on the producer's message shows — opens the panel if it is closed, and focuses the composer without sending anything.

No floating affordances appear on hover, and clicking a clip still only selects it. Quick-prompt chips follow the implicit context exactly as they follow a selection.

## Acceptance criteria

- [ ] The entry appears in a clip's and a track's ordinary context menu with the ✦ badge, and is absent from context menus of surfaces the Collaborator cannot be asked about.
- [ ] Choosing it opens the panel if closed, focuses the composer, and sends nothing.
- [ ] Worked: right-clicking a clip that is not selected and choosing the entry, then sending "what's wrong here" → the message carries a chip naming that clip and the run's opening context names that clip's id.
- [ ] Right-clicking a track and choosing the entry does the same for that track.
- [ ] Invoking the entry on a clip that is inside the current selection keeps the whole selection as the context; invoking it on a clip outside the selection makes that clip the context.
- [ ] Quick-prompt chips adapt to the implicit context exactly as they adapt to a selection.
- [ ] No hover affordance appears on clips or tracks, and clicking a clip selects it and does nothing else.
- [ ] Dismissing the context menu without choosing the entry changes neither the implicit context nor the focus.

## Notes

**claude** — 2026-08-29T01:23:06Z

Asking about *this thing here* is one gesture. A clip's and a track's context
menu carry the ✦ entry; choosing it says what the ask is about, opens the panel
if it was closed, and hands the producer the keyboard with nothing sent.

Seams used, and why. The implicit context is the arrangement's, so it is
asserted where the arrangement is paintless: `ArrangementView::askAboutClip` /
`askAboutTrack` / `askContext`, in `tests/AskCollaboratorTests.cpp` against a
real project. What a Task Run then carries is one step further out and lives
where the two vocabularies meet — `duet::app::openingContextOf`, moved out of
`Main.cpp` into `duet_app_core` for exactly that reason, and asserted in the
same suite, which is what makes "the run's opening context names that clip's
id" a thing a test says rather than a thing the app binary does unwatched. The
menus, the panel opening and the keyboard are the window's, and are asserted in
`tests/gui/CollaboratorPanelCanvasTests.cpp` on a real shell.

Against the criteria:

- The entry is on a clip's menu and a track's, under a separator, with the badge
  as its own image. It is on neither the empty timeline's menu nor a MIDI note's
  — the two menus of things the Tool Vocabulary cannot name.
- Choosing it opens the panel, leaves the keyboard wanted in the composer, and
  says nothing: the conversation is still empty afterwards.
- Worked: with the Verse clip selected, asking about Drop and sending "what's
  wrong here" chips the message "Drop", and the run's opening context carries
  `clip-<Drop>`, bar 9 beat 1 and playing true when it was started there.
- A track asked about is the context whatever is selected, and the run carries
  `track-<Keys>`.
- A clip inside the selection keeps the whole selection; a clip outside it is
  the context alone, and the selection is not disturbed by the ask.
- The quick prompts follow the implicit context because it is what the chip is
  made of: the panel is handed one context and does not know how it was chosen.
- No hover affordance: the pointer over a clip adds nothing to the surface, and
  a click selects the clip and neither opens the panel nor takes the keyboard.
- A menu dismissed is result zero, and every chooser returns on it.

Decisions made:

- **An ask stands until the producer's own hand moves.** The issue says "the
  implicit context of the next message", which leaves what becomes of it after
  that. Consuming it on send would have needed the panel to tell the shell a
  message went; instead the ask is remembered with the selection and the focused
  track as they were, and it is the context until either changes. So a producer
  can ask twice about the same clip, and one click anywhere says they have moved
  on. No new seam, and the rule is asserted at the arrangement's own.
- **A context of one clip is named, not counted.** The chip read "one clip",
  which does not say *which*. `clipSelected(name)` names it, and two or more
  clips still count, so "a chip naming that clip" is literally true. This is the
  chip for a one-clip selection too — one rule, not two.
- **The keyboard is intent in the panel and focus in the window.**
  `CollaboratorPanel::focusComposer` / `composerWantsKeyboard` /
  `composerLostKeyboard`: the canvas hands the keyboard over while the panel
  asks for it and says when the producer takes it elsewhere. A headless test
  cannot give a component the keyboard — JUCE refuses focus to anything not on
  screen — so the intent is what is asserted, and the grab beside it is the one
  line a window alone can run.
- **Menus are built apart from being shown.** `clipMenu()` / `clipMenuChosen`
  and the three others: a popup on screen is JUCE's, and what a menu offers and
  what an entry does are this surface's. That is what makes "the entry is there"
  and "dismissing does nothing" assertable at all; the behaviour of every
  existing entry is unchanged and moved verbatim.
- **The badge stays reserved.** `askCollaboratorItem` is in
  `CollaboratorPainting`, where the ✦ and the teal are already gathered, so the
  arrangement asks for the entry rather than naming either. The star is the same
  path the surfaces fill, handed to the menu as a `DrawablePath`.

Facts a reviewer needs:

- `MainShell::selectedClips()` and `focusedTrack()` are now one
  `MainShell::askContext()`, which is what both the chip and the opening context
  are made of, so the two can no longer disagree. The shell's own
  `Session*` had `currentSelectionContext` as its last reader and is gone with
  it.
- Right-clicking a clip outside the selection already selected it (nelbwc), and
  that is untouched: the implicit context is what makes a *track's* ask mean the
  track while clips are selected, and what would keep the clip case honest if
  that ever changed.
- Started while 7tw2tz waits on your review rather than on work: its code is on
  `main` (7739848) and this builds on it. If that review asks for changes to the
  panel's `Source` or its chip, this slice's `focusComposer` and
  `clipSelected` are what would move with them.
- Discovered and published, not done here: q1mnpd — three clang-tidy errors on
  `main` in files this issue did not touch, which is why a full sweep exits
  non-zero until it is done. Every file this issue touches lints clean.

Checks: clang-format clean, `./scripts/lint.sh` clean on everything this issue
touches (the three in q1mnpd are the whole of what the full sweep reports), full
Debug build of `duet_tests`, `duet_gui_tests` and `duet_app`, and 489/489 CTest
entries pass.
