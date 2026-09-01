---
id: oy5ubt
title: Post-fader send controls in the Mixer
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - rog54z
    - 808ncc
parent: jsfhhg
created: 2026-09-01T18:36:20Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Add paintless send snapshots/gestures to `Mixer` and visible per-strip controls to `MixerCanvas`.

## Settled interaction

- Every non-Master strip has `+ Send`; eligible choices are cycle-safe Groups not already sent to. Each existing send row names its destination and shows a -60..+6 dB horizontal control plus Remove.
- Add creates at 0 dB as one `Add Send` Action. A level gesture previews audibly without an Action, then restores and commits one `Set Send Level` Action; Escape restores with none. Remove is one `Remove Send` Action.
- Sends are post-fader by contract. Group sends are allowed when cycle-safe. The strip area scrolls vertically within a fixed send section so several groups do not cover inserts/output.

## Acceptance and tests

- [ ] Choices, values, add/set/remove/no-op, cycle filtering, and deletion refresh work through the Mixer seam.
- [ ] One undo per completed gesture restores exact graph/value.
- [ ] Offline renders independently verify: source at 0 dB is heard in return; lowering source fader by 6 dB lowers send return by 6 dB within tolerance; changing send level changes it independently.
- [ ] Component tests assert menu/control/focus/scroll composition, not paint.

Start in `Mixer.h/.cpp`, `MixerCanvas`, and `MixerViewTests.cpp`; use ADR 0006 feature assertions. Run all AGENTS.md checks before closing.
