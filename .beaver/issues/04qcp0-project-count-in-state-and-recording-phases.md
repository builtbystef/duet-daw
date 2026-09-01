---
id: 04qcp0
title: Project count-in state and recording phases
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - jpv27l
parent: 3xxk8b
created: 2026-09-01T18:40:41Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Wrap Tracktion's native recording count-in behind engine-free project state and observable phases. Milestone two offers Off, 1 bar, or 2 bars: Tracktion natively implements those exact bar modes; a custom 4-bar click scheduler is deliberately not introduced.

## Settled contract

- `CountInBars` is off/one/two. It is stored in the project's VIEW/transport state with nullptr UndoManager writes, default Off, save/reopen preserved, and no producer undo.
- Before `startRecording`, apply the open project's choice to Tracktion `Edit::setCountInMode`; native pre-roll keeps the requested capture/playhead start, including at project time zero.
- `RecordingPhase` is idle/countingIn/recording and exposes remaining whole beats (ceiling) during count-in. Counting begins after the existing device-settle wait, so device preparation is never shown as beat 1.
- Count-in click sounds regardless of the ordinary metronome toggle and is excluded from the recording inputs/project render. At the boundary phase changes to recording with no second command.
- Stop/Escape while counting cancels native pre-roll, restores the intended playhead, creates no take/Action, and preserves arm/input/monitoring.

## Acceptance and tests

- [ ] Hosted-device tests drive blocks for 1 and 2 bars in 4/4 and 3/4, assert exact phase/remaining-beat transitions and first captured sample/note at the intended project time, with no sleeps.
- [ ] Cancel from every count beat leaves no clip/file Action and unchanged routing/arm state.
- [ ] Count-in click is audible in output but absent from recorded audio and offline export.
- [ ] Off preserves current recording behavior; save/reopen preserves each choice.

Start in `Session`, `Recording.cpp`, `ViewState`, and `RecordingTests.cpp`. Use Tracktion's native CountIn modes rather than a Duet audio callback. Run all AGENTS.md checks before closing.
