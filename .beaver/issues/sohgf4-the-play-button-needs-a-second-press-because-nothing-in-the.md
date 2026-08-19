---
id: sohgf4
title: The Play button needs a second press, because nothing in the app survives the device rebuild
state: todo
priority: high
labels:
    - bug
parent: b1j3me
created: 2026-08-19T05:03:52Z
updated: 2026-08-19T05:03:52Z
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
