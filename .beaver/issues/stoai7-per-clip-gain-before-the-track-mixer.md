---
id: stoai7
title: Per-clip gain before the track mixer
state: todo
priority: low
labels:
    - spec
    - roadmap:h0eir5
depends_on:
    - jpv27l
parent: h0eir5
created: 2026-09-01T18:09:16Z
updated: 2026-09-01T18:42:01Z
---

## What to build

Add a producer-facing gain value to audio clips before their track's inserts, fader, sends, and automation. It is an edit property of the clip, distinct from track volume and from destructive normalisation.

## Acceptance criteria

- [ ] A selected audio clip exposes clip gain in dB through a visible inspector/control and a direct gesture with a numeric readout; MIDI clips do not show an inapplicable audio-gain control.
- [ ] The range includes useful attenuation, unity, and boost with a clear 0 dB reset; a completed gesture is one Set Clip Gain Action and Escape cancels it.
- [ ] Clip gain changes the waveform presentation enough to communicate relative level without rewriting source samples.
- [ ] The gain is applied before track processing and post-fader sends follow the resulting track signal; track fader and clip gain remain independently readable and undoable.
- [ ] Copy/duplicate, save/reopen, and Save As preserve clip gain; one undo restores the prior value digest-exactly.
- [ ] Feature assertions verify the rendered level delta at representative negative and positive settings and prove the source audio file is byte-unchanged.
- [ ] The Edit Vocabulary gains clip gain only with the direct producer route in place.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: -60..+24 dB model/preview miqvm0 -> selected-clip badge/direct gesture wi3f34 -> Suggestion parity 5a8nas.
