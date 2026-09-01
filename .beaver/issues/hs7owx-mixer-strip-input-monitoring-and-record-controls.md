---
id: hs7owx
title: Mixer-strip input, monitoring, and record controls
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - oscfrz
    - 808ncc
parent: uxkosp
created: 2026-09-01T18:33:36Z
updated: 2026-09-01T18:41:15Z
---

## Bounded implementation

Mirror Track I/O in Mixer strips using the same `TrackIo` snapshots and writes as the arrangement. Do not create a second compatibility or device-loss policy.

## Settled layout and interaction

- Each non-group, non-Master strip shows an `Input` row, a three-state `Monitor` row, and an Arm button above the existing output row. The chosen names are visible without opening a menu; ellipsis is allowed only with a tooltip carrying the full value.
- Group strips show `Bus (sum)` and no monitor/arm control. Master keeps only `Main Output`.
- Arm and recording use the same idle/armed/rolling states as the arrangement; either surface updates the other on the next model refresh.
- Existing horizontal scrolling remains, and the new rows must not hide insert-chain access at the minimum supported Mixer height.

## Acceptance and tests

- [ ] Mixer controls expose exactly the choices and enabled states from `TrackIo`.
- [ ] Changing input/monitor/arm updates the arrangement without an Action; output remains one Action.
- [ ] None and unavailable states explain why Record cannot start.
- [ ] Several tracks and groups remain horizontally scrollable with readable full-value tooltips.
- [ ] `MixerViewTests` cover shared state and `MixerCanvas` component tests cover routing/focus/composition rather than pixels.

Start in `Mixer.h/.cpp`, `MixerCanvas.h/.cpp`, and `tests/MixerViewTests.cpp`. Run all AGENTS.md checks before closing.
