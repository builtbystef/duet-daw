---
id: bdj09t
title: Piano Roll title trips JUCE's ASCII String assertion every repaint
state: done
priority: medium
labels:
    - bug
created: 2026-08-25T14:54:34Z
updated: 2026-08-26T10:09:57Z
---

## What to fix

`PianoRollCanvas::paint` passes the UTF-8 em dash in its title literals through JUCE's ambiguous `String(const char*)` path. A Debug app with the Piano Roll visible reports `JUCE Assertion failure in juce_String.cpp:327` at roughly the surface repaint rate (about 110 assertions in a four-second idle smoke run).

Construct the title explicitly as UTF-8, as the app shell's `text` helper already does, and audit the other GUI literals with non-ASCII bytes for the same boundary.

## Acceptance criteria

- [ ] Running the Debug app for ten seconds with the Piano Roll visible produces no `juce_String.cpp:327` assertion.
- [ ] The visible title still uses the specified em dash.

## Notes

**claude** — 2026-08-26T10:09:57Z

Closed as a duplicate of s2e041 (2026-08-26, during implement-loop triage).

s2e041 'Non-ASCII literals reach juce::String as 8-bit text' is the superset: it names the same two PianoRollCanvas.cpp literals (:81 and :82) that this issue targets, plus Main.cpp:475 and :485, and its third acceptance criterion settles the single project-wide convention this issue's body asked for as an audit ('audit the other GUI literals with non-ASCII bytes for the same boundary').

Both acceptance criteria here are covered there: no juce_String.cpp:327 assertion from Duet's own strings, and the visible title keeps one em dash. Track the work on s2e041.
