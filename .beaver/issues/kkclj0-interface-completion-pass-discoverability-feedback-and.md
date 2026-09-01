---
id: kkclj0
title: 'Interface completion pass: discoverability, feedback, and keyboard access'
state: todo
priority: high
labels:
    - review
    - roadmap:yfpnps
depends_on:
    - uj5a96
    - 0x49el
parent: yfpnps
created: 2026-09-01T18:08:12Z
updated: 2026-09-01T18:42:01Z
---

## What to build

After the six workflow slices have put the missing controls on screen, make the whole interface teach and report itself consistently. This is not a redesign and not a paint-only polish pass: it closes the gaps that leave a valid operation undiscoverable, mouse-only, or silent when it cannot happen.

## Acceptance criteria

- [ ] Every icon-only or abbreviated interactive control has a concise tooltip, an accessible name, and a focus indicator; labels and terms follow `docs/GLOSSARY.md`.
- [ ] Pointer cursors communicate click, text, drag, resize, trim, loop, and invalid-drop regions consistently across arrangement, Piano Roll, Browser, and Mixer.
- [ ] Keyboard traversal reaches the transport, track I/O, Browser, Mixer graph, Piano Roll, Collaborator composer, dialogs, and built-in editors in a predictable order; Enter/Space activate the focused control and Escape cancels or returns focus without losing uncommitted text unexpectedly.
- [ ] Bare-letter shortcuts remain suppressed while any text field is active, and focus-sensitive edit commands always act on the surface whose focus indicator is visible.
- [ ] Empty states say what belongs there and the next useful action: no clips, no MIDI clip open, empty sample folder, no plugins scanned, no input selected, no groups/sends, no provider configured, and no Suggestions.
- [ ] Recoverable failures are local and actionable: unavailable device/input, unreadable sample, invalid import, failed plugin editor, impossible route, failed export, and Collaborator failure never read as a silent no-op or disable unrelated work.
- [ ] Long lists and timelines have visible scroll position and usable scrollbars without changing the settled wheel/zoom conventions; the pinned ruler, headers, and Master strip remain legible at density.
- [ ] Controls use the Graphite tokens in both themes and at every supported interface scale; no default JUCE chrome, clipped label, overlapping hit target, raw model id, or ambiguous single-letter label remains without an explanation.
- [ ] The shipping build shows no development tool trace or fixture-only control, and every control shown in an empty fresh project performs a real reachable action.
- [ ] A source inventory maps every milestone-one producer operation to its visible route and finds no operation reachable only from the Collaborator or a direct model call.
- [ ] A dry run of the release-gate walkthrough reaches every step without coaching; each point of hesitation or misclick is recorded and fixed here before the gate is attempted.

## Review

Closure waits for Target Producer review of the running application in both themes. Automated tests cover policy and component behavior; they do not declare a workflow discoverable on the producer's behalf.

If review finds a product defect, create a bounded `session:task` child carrying `roadmap:yfpnps`, add it as a dependency of this review, note the failed step, and release this issue. The AFK selector can then resume safely; the review issue itself is never used as an implementation bucket.

## Notes

**agent** — 2026-09-01T18:42:01Z

Mechanical completion is split into fsaacq, np7wjh, cbc13c, and 1bo4s7, then parity inventory uj5a96. This issue is now the Target Producer review stop, also waiting on live Track I/O review 0x49el; it is intentionally excluded from AFK selection.
