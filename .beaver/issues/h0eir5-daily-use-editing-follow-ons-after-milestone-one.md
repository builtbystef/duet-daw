---
id: h0eir5
title: Daily-use editing follow-ons after milestone one
state: todo
priority: low
labels:
    - spec
    - roadmap:h0eir5
depends_on:
    - jpv27l
created: 2026-09-01T18:08:47Z
updated: 2026-09-01T18:42:01Z
---

## Problem Statement

The usable-workflow gate yfpnps deliberately finishes the milestone-one promise rather than expanding it indefinitely. Passing that gate still leaves four ordinary daily-use operations absent: splitting a clip, setting clip gain, fading clip edges and crossfading overlaps, and recording with a count-in.

These are the first post-gate editing follow-ons. They are recorded now so they do not disappear, but their implementation must not delay the milestone-one usability gate.

## Solution

Add four independent producer-facing capabilities after jpv27l passes:

1. Split audio and MIDI clips non-destructively at the playhead or pointer.
2. Draw and edit fades, and form explicit crossfades between overlapping audio clips.
3. Set clip gain before the track's mixer path.
4. Configure and run a metronome count-in before recording begins.

Each remains one or more named Actions at the existing vocabulary seam, preserves the self-contained project, and is available to the producer before it is added to the Edit Vocabulary for Suggestions.

## Testing Decisions

Editing behavior is asserted at the Action seam and by digest-exact undo. Audible gain/fade/crossfade behavior uses ADR 0006 feature assertions. Count-in timing uses commandable transport/device seams rather than sleeps, plus one live recording review.

## AFK implementation contract

The four direct children are `spec` containers, not claimable implementations. After release gate `jpv27l` passes, an unattended loop claims only ready executable leaves:

```bash
beaver list --ready --label roadmap:h0eir5 --label session:task
```

One iteration starts the first result, implements it red → green, runs its targeted checks plus the complete `AGENTS.md` gate, closes it, and commits code with issue state. A blocked implementation is noted and released without changing acceptance. An empty result means the release gate has not passed, dependencies remain, or a human review is ready.

Run one implementation worker at a time. Every leaf contains the settled behavior, test seam, and repository-check obligation. `review` issues are human/device stops and are excluded from that selector.

Count-in is Off/1/2 bars. Preparation deliberately chose Tracktion's native bar-count modes instead of adding a Duet-authored real-time click scheduler solely to provide the originally proposed 4-bar option.

## Out of Scope

Comping, punch-in, loop recording, destructive sample editing, automatic crossfades on every overlap, warp/time correction, and recorded automation modes remain separate future work.

## Notes

**agent** — 2026-09-01T18:42:01Z

Prepared for post-gate unattended implementation: claim only ready issues with labels roadmap:h0eir5 + session:task. The four direct children are spec containers; count-in live approval lkq8fn is a review stop.
