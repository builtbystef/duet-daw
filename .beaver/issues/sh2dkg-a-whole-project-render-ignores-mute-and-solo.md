---
id: sh2dkg
title: A whole-project render ignores mute and solo
state: done
assignee: claude
priority: medium
labels:
    - bug
created: 2026-08-25T16:50:11Z
updated: 2026-08-26T12:16:33Z
---

## What to build

A render of the whole project honours what the producer muted and soloed.

Today it does not. The engine's own render path — and, since 6zog6s, Duet's own
`Session::renderToFile`, which keeps that path's guards — puts a
`FreezePointPlugin::ScopedTrackSoloIsolator` around the render, and that guard
solo-isolates *and unmutes* every track it is given. For a whole-project render
that is every track, so mute and solo are both erased for the duration.

Measured at 6zog6s with `tests/scratch`: a two-track project of a 440 Hz and a
1760 Hz tone rendered with the 440 Hz track muted still held that tone at the
same level as the unmuted render, −9.02 dB at 440 Hz in both.

The guard is right where it is on the *per-track* render (`renderTrackToFile`):
rendering one track has to ignore what is soloed elsewhere, and that isolation
is an acceptance criterion of 6zog6s. It is the whole-project render that should
not have it, because mute and solo are the project.

The one thing to settle before building: whether a whole-project render honours
solo, or only mute. Exporting a project with a track soloed and getting only
that track is defensible and is what several DAWs do; so is exporting what the
project holds. Decide it, record the decision in a note here, then build.

## Acceptance criteria

- [ ] A whole-project render of a two-track project with one track muted holds
      the unmuted track's tone and at least 40 dB less of the muted track's.
- [ ] Undoing the mute and rendering again brings the tone back — the render
      follows the project rather than a state the render itself left behind.
- [ ] Solo behaves as the decision recorded on this issue says, and a test states
      which it is.
- [ ] A single-track render is unchanged: it still ignores another track's solo
      and renders the track asked for.

## Notes

**claude** — 2026-08-26T10:09:43Z

Decision (2026-08-26, user, settled before an implement-loop run): a whole-project render honours **both mute and solo**. Exporting a project with a track soloed renders only the soloed track(s); exporting with a track muted renders it silent. The rationale is that the render matches what the producer hears — solo is project state at render time, not merely a monitoring aid, and this is what several DAWs do.

This settles the open question in the body ("whether a whole-project render honours solo, or only mute"). Build against it; no further escalation is needed on this point. The fourth acceptance criterion is unchanged: a single-track render still ignores another track's solo and renders the track asked for.

**claude** — 2026-08-26T12:15:28Z

Done. A whole-project render now honours mute and solo; a single-track render
still ignores both. Four cases in tests/OfflineRenderTests.cpp, one per
criterion. Full suite 316/316 (8 skipped, all needing a real audio device);
format and lint clean.

**Seams** (AFK session, recorded as the test skill asks): `Session::renderToFile`
and `Session::renderTrackToFile`, observed through the offline-render harness
(`duet::testing::renderProject` / `renderTrack`) and measured with
`toneLevelDbBetween` at each track's own frequency. The two tracks carry 440 Hz
and 1760 Hz, so one frequency's level says which track reached the file.

**The change.** `renderTracksToFile` takes a flag, and the engine's
`FreezePointPlugin::ScopedTrackSoloIsolator` is now a `unique_ptr` the render
guards create only when it is asked for. `renderToFile` renders without it
(`honouringMuteAndSolo`); `renderTrackToFile` keeps it (`ignoringMuteAndSolo`),
which is 6zog6s's isolation criterion doing its job. Nothing else about either
render moved. The isolator is declared last among the guards so it is undone
first: the edit is the project again before the render status comes down.

**Measured on the dev machine.** Before: whole-project render with the 440 Hz
track muted held it at -9.02 dB, the same as unmuted. After: 440 Hz at -inf dB
against 1760 Hz at -9.02 dB, so far past the 40 dB the criterion asks for that
the muted track is not in the file at all. Solo the same way round. Undo the
mute and both tones come back at -9.02 dB. The low track rendered alone while
the high one is soloed: its own tone at -6.02 dB, the other below -100 dB.

**Solo** follows the decision noted above: a whole-project render holds only the
soloed track, and the test named 'a whole-project render holds only the soloed
track' states it.

**Engine fact recorded** in docs/ENGINE_NOTES.md, under Further facts: the
render's solo isolator also unmutes the tracks it is given. `Session::renderToFile`'s
header doc now states what mute and solo mean for the file.

**Nothing discovered outside the issue.** zm174o (Export/Bounce), which depends
on this, is now unblocked on this point.

**claude** — 2026-08-26T12:16:33Z

Correction to the figures in the note above: the level the harness reports for a
track that did not reach the file is -100 dB, its floor, and not -inf. The
measured runs, each printed by the case's own INFO:

- muted: 440 Hz at -100 dB, unmuted 1760 Hz at -9.0206 dB
- soloed: 1760 Hz at -9.0206 dB, unsoloed 440 Hz at -100 dB
- after the undo: 440 Hz at -9.0206 dB, 1760 Hz at -9.0206 dB
- the unsoloed low track rendered alone while the high one is soloed: 440 Hz at
  -6.0206 dB, 1760 Hz at -100 dB

Everything else in that note stands.
