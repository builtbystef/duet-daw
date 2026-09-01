---
id: b4yf2j
title: Background Browser folder scanning with stale-result cancellation
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

Move sample-folder enumeration out of `Browser::refresh` and painting. Keep the Browser paintless: a host-supplied worker scans, and a generation-tagged result is installed on the message thread.

## Settled scan policy

- Recurse into ordinary directories, do not follow directory symlinks, and include only `Browser::sampleExtensions()` regular files.
- Sort case-insensitively by relative path with bytewise path as the tie-breaker, so results are deterministic.
- A new refresh/project replacement cancels or supersedes the old generation; stale results never replace newer ones.
- Existing rows remain while refreshing, with a `Scanning…` status and completed/known count. Cancellation is silent; an unreadable subtree yields one local status while readable siblings remain.
- Folder add/remove and plugin rescan use the same refresh generation; no filesystem work occurs in `paint()` or the audio callback.

## Acceptance and tests

- [ ] A deep temporary tree produces the exact deterministic rows and ignores symlink cycles.
- [ ] A deliberately blocked old generation cannot overwrite a completed newer generation.
- [ ] Progress/error/busy snapshots are observable through the Browser public seam.
- [ ] Component tests assert status composition only; worker tests use a commandable executor, never sleeps.

Start in `Browser.h/.cpp`, `BrowserCanvas.cpp`, and app wiring in `Main.cpp`. Add the narrow worker interface in the layer that owns threads. Run all AGENTS.md checks before closing.
