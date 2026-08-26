---
id: sh2dkg
title: A whole-project render ignores mute and solo
state: todo
priority: medium
labels:
    - bug
created: 2026-08-25T16:50:11Z
updated: 2026-08-26T10:09:43Z
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
