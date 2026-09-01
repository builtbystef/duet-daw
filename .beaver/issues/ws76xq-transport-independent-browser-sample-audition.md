---
id: ws76xq
title: Transport-independent Browser sample audition
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: kmb4mv
created: 2026-09-01T18:34:33Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Add one source-file audition player and wire it to Browser selection. This is not Suggestion Audition: call it `Source audition` in code and UI text so the glossary term remains unambiguous.

## Settled behavior and architecture

- `duet_app` owns one player for the open audio device; `duet_gui::Browser` sees an engine-free controller/state interface. File decode/read-ahead happens off the message and audio callbacks; the callback only consumes prepared blocks and performs allocation-free mixing.
- Browser-focused Space and a visible Play/Stop button toggle the selected sample. Space elsewhere remains transport Play/Stop.
- Selecting another sample switches to it; pressing again, project replacement, Browser close, device loss, or app shutdown stops and releases it. Only one source plays.
- Audition is routed to Main Output at a fixed -6 dB safety gain, does not move/stop/start the project transport, cannot be recorded as an input, and creates no Action/dirty state.
- State reports loading, playing, progress 0..1, stopped, and a plain row-local error. Unsupported/unreadable files never disturb project audio.

## Acceptance and tests

- [ ] Commandable player tests prove lifecycle, one-at-a-time replacement, progress, and transport/undo independence.
- [ ] An audio feature test hears the selected known tone at -6 dB and then silence after every stop path.
- [ ] The callback target links `duet::realtime`; the first implementation is proven red/green under the existing RTSan probe pattern.
- [ ] Browser component tests prove focus-sensitive Space and visible state without testing paint.

Likely seams: new app-owned source player, `Browser.h/.cpp`, `BrowserCanvas.cpp`, `Main.cpp`. Follow ADR 0006 and run all AGENTS.md checks before closing.
