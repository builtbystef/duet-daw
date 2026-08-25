---
id: bdj09t
title: Piano Roll title trips JUCE's ASCII String assertion every repaint
state: todo
priority: medium
labels:
    - bug
created: 2026-08-25T14:54:34Z
updated: 2026-08-25T14:54:34Z
---

## What to fix

`PianoRollCanvas::paint` passes the UTF-8 em dash in its title literals through JUCE's ambiguous `String(const char*)` path. A Debug app with the Piano Roll visible reports `JUCE Assertion failure in juce_String.cpp:327` at roughly the surface repaint rate (about 110 assertions in a four-second idle smoke run).

Construct the title explicitly as UTF-8, as the app shell's `text` helper already does, and audit the other GUI literals with non-ASCII bytes for the same boundary.

## Acceptance criteria

- [ ] Running the Debug app for ten seconds with the Piano Roll visible produces no `juce_String.cpp:327` assertion.
- [ ] The visible title still uses the specified em dash.
