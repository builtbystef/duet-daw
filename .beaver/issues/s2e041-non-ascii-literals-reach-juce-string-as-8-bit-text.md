---
id: s2e041
title: Non-ASCII literals reach juce::String as 8-bit text
state: done
assignee: claude
priority: low
labels:
    - bug
created: 2026-08-25T19:53:08Z
updated: 2026-08-26T11:33:44Z
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

## Notes

**claude** — 2026-08-26T11:33:23Z

## What was built

One crossing, `duet::gui::utf8` in the new `modules/duet_gui/components/duet/gui/Text.h`, and the four literals now go through it: `Main.cpp:471` and `:481` (the window title) and `PianoRollCanvas.cpp:82`/`:83` (the Piano Roll header). The helper is the one that already sat in `Main.cpp`'s anonymous namespace as `text()`, moved to where both targets see it — `duet_gui_components` is the JUCE-linked half of duet_gui, and `duet_app` already links it — and deleted from `Main.cpp`.

The convention the fix settles, written into `docs/CODING_STANDARDS.md` under a new **Text** heading:

- A literal that is not plain ASCII reaches `juce::String` only through `duet::gui::utf8`.
- A source that mentions no JUCE keeps such a literal as it is. It needs no crossing: its text reaches an interface as a `std::string`, and `juce::String (const std::string&)` reads its bytes as UTF-8 (`createFromFixedLength` → `CharPointer_UTF8`), unlike the `const char*` constructor. That is what leaves `TaskRun.h:125` and `CollaboratorPanel.cpp:40` — the two other non-ASCII literals in the tree — correct where they are.

## Seams

No spec names one, so the outermost seams that can observe the criteria:

- `duet::gui::utf8`, in the JUCE-linked suite (`tests/gui/TextTests.cpp`): the em dash arrives as the one character U+2014, and the ASCII around it is unchanged. The Piano Roll header and the window title are both built in `paint()`/an anonymous class, and paint stays untested (spec 535bbo), so this is as close to them as a test gets.
- Duet's own module sources, in `tests/TextEncodingTests.cpp`: a scan that tells a string literal from a comment, a character literal and a digit separator, and fails on any non-ASCII literal in a JUCE-mentioning source that is not the argument of `utf8 (`. Same shape as the reserved-accent guard in `GraphiteTokenTests.cpp`. It named exactly the four sites the issue named before the fix, and it carries its own self-check, because a guard whose scan went blind would pass silently.

## For a reviewer

- The issue's reading of `Main.cpp` was half right: both title literals already went through the local `text()` helper, so neither could have been the assertion that repeated on stderr. `PianoRollCanvas` was the broken one — `"Piano Roll — " + juce::String {...}` selects `operator+ (const char*, const String&)`, which builds the String from ASCII.
- Criterion 1 is verified as far as this machine allows: the Debug app ran for 30 s and raised no `juce_String.cpp` assertion. It could not open a project — `~/Music/Duet Projects` is not writable under the sandbox — so it sat on the `problem` branch of `titleText()`, which is one of the two title sites, and the Piano Roll never drew. The tree-wide guard is what covers the rest.
- Checks: format clean, `./scripts/lint.sh` clean over the whole tree, `ctest --preset linux-debug` 312/312 passed (8 skipped, all needing a real audio device).
