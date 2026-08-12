---
id: 6zog6s
title: 'Offline render harness: feature assertions and the determinism canary'
state: todo
priority: medium
depends_on:
    - 4r7nlj
parent: b1j3me
created: 2026-08-11T01:51:58Z
updated: 2026-08-11T01:51:58Z
---

## What to build

The audio-correctness test harness per spec b1j3me / ADR 0006 and branch prototype/offline-render-correctness (16/16): headless offline rendering of an Edit through the engine's Renderer on a worker thread, plus feature-assertion helpers — measured pitch, onset positions, RMS/spectral change — with domain tolerances. Never golden files, fingerprints, or stored-sample comparison. Renders go to a fresh destination every time (the engine caches by destination file), at fixed sample rate, block size, and bit depth, with dithering off. The one permitted sample comparison is the within-process determinism canary. This harness is also the per-track render path js437t's tier-2 analysis will call.

## Acceptance criteria

- [ ] A whole-Edit render and a single-track (isolated from a multi-track Edit) render both produce a readable audio file headlessly, off the message thread, with no audio device.
- [ ] Worked example: an Edit with a 440 Hz sine clip spanning 1.0s–2.0s renders to: RMS below the silence threshold before 1.0s, a detected onset no more than one render block before 1.0s and not after it (the engine places note-ons at the start of their containing block), and measured pitch of 440 Hz within the helper's stated tolerance during the clip.
- [ ] A MIDI note through a built-in instrument yields a detected onset within the same one-block-early tolerance of its scheduled start.
- [ ] An RMS/spectral-change assertion distinguishes a real change (a gain drop of 6 dB mid-render) from an unchanged render.
- [ ] Determinism canary in the ordinary suite: the same Edit rendered twice in one process produces byte-identical output; the comparison is never against a stored file.
- [ ] Two consecutive renders of different Edit states produce different output (fresh destination per render — no stale cache result).
- [ ] The suite contains no golden files and no stored audio fingerprints; reference signals are synthetic (ground truth by construction).
