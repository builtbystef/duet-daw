---
id: sohgf4
title: The Play button needs a second press, because nothing in the app survives the device rebuild
state: done
assignee: claude
priority: high
labels:
    - bug
parent: b1j3me
created: 2026-08-19T05:03:52Z
updated: 2026-08-19T05:35:14Z
---

## What is wrong

Reported while reviewing 4r7nlj: "sometimes I have to press play twice to hear sound, sometimes I press it once, it starts playing, then stops, then I have to press it again and it starts playing indefinitely."

That is hazard 6, which `AGENTS.md` already names and which every test goes through `duet::testing::playUntilRolling` to survive: seconds after the first playback the engine rebuilds its device list, and the rebuild frees the playback graph and stops the transport. It happens once per session, which is exactly why the second press always works.

`MainComponent`'s play button calls `session().startPlayback()` once and never looks again:

```cpp
playButton.onClick = [this]
{ withProject ([] (auto& p) { p.session().startPlayback(); }); };
```

The tests have known about this since the walking skeleton and the app never learned it. It is not from 4r7nlj — the button predates it — but it makes that issue's manual demo harder to judge, since a silent first press reads as a bug in whatever was just edited.

## What to build

The app already runs a timer for its status line (`statusRefreshMs`). The shape that matches the test helper: remember that the producer asked for playback, and while that is true and the transport is not rolling, ask again. Stop clears it. Worth deciding whether this belongs in the app or behind `Session::startPlayback`, since every future caller has the same problem and the model is where the engine's quirks are already absorbed — `playUntilRolling` existing in test support is a hint that the answer is the model.

## Acceptance criteria

- [ ] One press of Play starts audio and it stays started, on the first playback of a session.
- [ ] Stop still stops, and a stopped transport is not restarted by whatever handles this.
- [ ] The decision about where it lives — app or `Session` — is recorded, and if it lands in `Session` then `playUntilRolling` in test support says so or goes away.

## Notes

**claude** — 2026-08-19T05:35:14Z

Done. One press of Play is now enough, because `Session::startPlayback` keeps asking.

**The decision (criterion 3): it lives in the model, not in the app.** `Session::startPlayback` remembers that playback was asked for and, on a 100 ms timer, asks a transport that is not rolling to play again; `stopPlayback` stops the timer before it stops the transport, so a stop is the producer's last word. The reasons: the model is where the engine's quirks are already absorbed (hazard 5's pinned source references, hazard 2's open transaction, hazard 7's canonicalized digests all live behind this facade), and every caller of `startPlayback` — the shell's button today, the transport bar and the Collaborator's audition tomorrow — has the same problem, so absorbing it once is what keeps the app free of it. The shell's play button is unchanged. Recorded in `docs/ARCHITECTURE.md` (the `duet_model` sentence) and at the call site; no ADR, since the decision is one function wide and reverses in an afternoon.

**The retry is bounded**: 100 asks at 100 ms, ten seconds, and the counter resets every tick that finds the transport rolling — so the device rebuild, which arrives after playback has started, gets the whole window again. It ends because a machine with no working output would otherwise be asked forever, and every ask allocates a playback context. The memory of "playback was asked for" is the timer running, and nothing else, so there is no second copy of that state to keep in step.

**Tests** (`tests/ProjectAndTransportOpsTests.cpp`), both at the model seam, both on real audio devices, both skipped when the machine has none:

- "one call to start playback is enough, and it survives the device rebuild" — `startPlayback()` once, pump 8 s through the rebuild window, `isPlaying()`. Red before the change (this is the reported bug reproduced), green after.
- "a stopped transport is not asked to play again" — play, stop, then pump the whole retry window and require it is still stopped. Green before and after; it is the guard on criterion 2.

**`playUntilRolling` says so** (criterion 3, second half): it kept its name and lost its loop — it starts playback once and then only runs the message loop until the transport rolls, because the asking is the model's now. Its doc says that, and says what a headless test still owes the model: a pumped message loop, since the asking is on a timer. Every existing call site works unchanged.

Checks: format, lint, and 54/54 ctest green.

To confirm by ear: run the app, press Play once, and leave it for ten seconds — the phrase keeps playing through the rebuild instead of falling silent.
