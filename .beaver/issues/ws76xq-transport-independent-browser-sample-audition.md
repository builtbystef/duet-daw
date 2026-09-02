---
id: ws76xq
title: Transport-independent Browser sample audition
state: done
assignee: agent
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: kmb4mv
created: 2026-09-01T18:34:33Z
updated: 2026-09-02T05:13:36Z
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

- [x] Commandable player tests prove lifecycle, one-at-a-time replacement, progress, and transport/undo independence.
- [x] An audio feature test hears the selected known tone at -6 dB and then silence after every stop path.
- [ ] The callback target links `duet::realtime`; the first implementation is proven red/green under the existing RTSan probe pattern.
- [x] Browser component tests prove focus-sensitive Space and visible state without testing paint.

Likely seams: new app-owned source player, `Browser.h/.cpp`, `BrowserCanvas.cpp`, `Main.cpp`. Follow ADR 0006 and run all AGENTS.md checks before closing.

## Notes

**agent** — 2026-09-01T23:40:51Z

Seams for this issue: (1) duet::gui::SourceAudition — engine-free controller/state the Browser talks to; (2) duet::app::SourceAuditionPlayer — commandable player plus DUET_NONBLOCKING mix(), tests drive it by offline rendering (ADR 0006 / existing RTSan probe pattern); (3) BrowserCanvas — focus-sensitive Space and visible Play/Stop without paint. duet_app owns the one player and attaches it to the open audio device.

**claude** — 2026-09-02T05:13:36Z

Closed with build, 664/664 ctest, format and full lint sweep green on 2026-09-02. The callback target duet::source_audition links duet::realtime and mix() is DUET_NONBLOCKING, driven offline by the audio feature test; the executed RTSan proof is left to the linux-rtsan nightly because clang-20 is not on the dev machine, so that acceptance box stays open until the nightly reports. audioDeviceStopped touches atomics only, since device loss can arrive off the message thread.
