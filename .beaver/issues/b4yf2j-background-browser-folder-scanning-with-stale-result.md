---
id: b4yf2j
title: Background Browser folder scanning with stale-result cancellation
state: done
assignee: agent
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: kmb4mv
created: 2026-09-01T18:34:33Z
updated: 2026-09-01T22:43:34Z
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

## Notes

**agent** — 2026-09-01T21:44:15Z

Seams for this issue: (1) scanSampleFolder / Browser::sections() for the deterministic tree walk and symlink-cycle policy; (2) Browser::scanSnapshot + applyScanProgress/Outcome for busy/progress/error and stale-generation rejection; (3) SampleFolderScanner with a commandable executor and poster, never sleeps; (4) BrowserCanvas::composedStatus for status composition only. Filesystem work leaves refresh(); the host in duet_app owns the worker thread.

**agent** — 2026-09-01T22:43:30Z

Completed background Browser folder scanning with stale-result cancellation.

What is there
- `scanSampleFolder` walks one tree: ordinary directories only, no directory-symlink following (cycles are ignored), `Browser::sampleExtensions()` regular files, sorted case-insensitively by relative path with bytewise path as the tie-breaker. An unreadable subtree is one local `Could not read …` status; readable siblings remain.
- `Browser::refresh` no longer walks the disk. It keeps existing rows, bumps one generation (folder add/remove, plugin rescan, and `setSession` share it), and asks a host-supplied worker. A browser with no worker still walks on the caller's thread so existing tests stay synchronous.
- `applyScanProgress` / `applyScanOutcome` install only the current generation. Cancellation is silent. `scanSnapshot()` exposes busy, completed/known, and the `Scanning… n/m` message.
- `duet::app::SampleFolderScanner` is the worker in the layer that owns threads. Production posts results with `callAsync` off an owned thread (joined on destroy). Tests inject a commandable executor and poster; they never sleep.
- `BrowserCanvas::composedStatus` is the status-composition seam. `paint()` reads already-built rows and the snapshot string.

Facts for a reviewer
- tests/BrowserTests.cpp: deep deterministic tree + symlink cycles; unreadable subtree; existing rows while busy; stale generation rejected at the Browser seam.
- tests/SampleFolderScannerTests.cpp: commandable executor/poster; blocked old complete cannot overwrite a newer one; superseded queued scan posts nothing.
- tests/gui/BrowserCanvasTests.cpp: composedStatus only.
- 653 tests passed. Changed files lint clean; format dry-run clean.
