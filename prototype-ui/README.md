# PROTOTYPE — milestone-one UI (roadmap node r4m858)

Disposable code. GUI-only JUCE app (no engine, no audio) answering the
look-and-behave questions from grill s11o4w against the Graphite mockup
(~/Code/temp/duet-daw-ui-mockup). Never ship any of this.

## Run

```sh
cmake --preset default
cmake --build --preset release -j 4
./build/duet_ui_prototype_artefacts/Release/"Duet UI PROTOTYPE"
```

## What to exercise

1. **Teal tuning** — "Teal" button (top right) opens HSB sliders with a live
   hex readout; the theme toggle (☼/☾) switches Graphite dark/light. Each mode
   starts at the grill's approximation (#3fd0be dark / #0e7c70 light).
2. **Proposal ghosts** — Pad track carries two teal ghost+glow clips and Bass a
   ghost fader (Mixer tab). Collaborator panel: cherry-pick checkboxes,
   Audition (ghosts intensify; mixer gets A/B chip), Accept, Reject.
3. **Grid + zoom** — Ctrl-scroll h-zoom at pointer, Ctrl+Shift v-zoom,
   Shift-scroll pan; grid subdivides/coarsens with zoom; grid-size combo in the
   transport bar; `+`/`-`/`0` zoom keys.
4. **Smart tool** — click/rubber-band/Ctrl-toggle select, drag move (snapped,
   Alt bypass), Ctrl-drag copy, edge trim, Delete, double-click empty MIDI
   lane creates a clip, double-click a MIDI clip opens the piano roll
   (scale highlight, Fold, note-length chip; double-click adds/removes notes).
5. **Docking** — B/C/E toggle browser/collaborator/bottom panel, P/X switch
   tabs, draggable dividers; Space/R/L/M/F transport keys, Home/End.
