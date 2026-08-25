---
id: s2e041
title: Non-ASCII literals reach juce::String as 8-bit text
state: todo
priority: low
labels:
    - bug
created: 2026-08-25T19:53:08Z
updated: 2026-08-25T19:53:08Z
---

## What happens

Duet builds several `juce::String`s from UTF-8 `const char*` literals containing an em dash, which `juce::String (const char*)` reads as ASCII. In a Debug build every one of them trips `jassert (CharPointer_ASCII::isValidString (...))` in `juce_String.cpp:327`, and the three bytes of the em dash are carried through as three separate characters rather than as one.

Reproduce: run the app (`pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet`). The window title is drawn every time the project name is refreshed, so the assertion repeats continuously on stderr.

The literals, as of 2026-08-25:

- `modules/duet_app/src/Main.cpp:475` and `:485` — the window title, `"Duet — "`.
- `modules/duet_gui/components_src/PianoRollCanvas.cpp:81` and `:82` — `"Piano Roll — "` and `"Piano Roll — No MIDI clip"`.

Found while running the app for issue 3rd6lu; it predates that slice and is unrelated to it.

## Acceptance criteria

- [ ] Running the Debug app raises no `juce_String.cpp` assertion from Duet's own strings.
- [ ] The window title and the Piano Roll header show one em dash, not three characters.
- [ ] Every string Duet builds from a literal that is not plain ASCII crosses into `juce::String` as UTF-8, by whatever single convention the fix settles on.
