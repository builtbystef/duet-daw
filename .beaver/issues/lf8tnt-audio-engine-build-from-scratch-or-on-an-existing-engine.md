---
id: lf8tnt
title: 'Audio engine: build from scratch, or on an existing engine?'
state: todo
priority: high
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - 1hn16k
parent: d9gioe
created: 2026-08-07T06:02:31Z
updated: 2026-08-07T06:27:08Z
---

Research session. With the application foundation chosen (node 1hn16k), decide the engine layer: write playback/recording/mixing from scratch on the foundation's audio classes, or adopt an existing engine (e.g. Tracktion Engine if JUCE; alternatives otherwise).

Weigh: control over the project data model (the AI seam needs a clean, scriptable model of the session — an adopted engine's model may fight that), licensing, maturity of features milestone one needs (recording, latency compensation, mixing graph), and how much scratch-building delays a usable product. Primary sources.

Deliverable: a recommendation with cited trade-offs, ready to become an ADR.

## Notes

**claude** — 2026-08-07T06:27:08Z

Constraint from the user (2026-08-07): the project starts as open source, with the potential to become a commercial product later. Evaluate every candidate technology, library, and service against that path — prefer permissive or dual-licensable licenses; a copyleft-only option with no commercial-license route (e.g. GPL with no paid tier) forecloses a commercial edition and needs explicit justification. Recorded in the roadmap root (d9gioe) under 'Licensing posture'.
