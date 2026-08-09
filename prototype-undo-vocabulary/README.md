# PROTOTYPE — undo vocabulary (roadmap node skb4tp)

Disposable code. Answers: what does Duet's edit vocabulary look like as a layer
over `tracktion::engine::Edit`, such that a producer edit and an accepted
Proposal are indistinguishable to undo, a multi-step Proposal is ONE undo step,
and a rejected Proposal leaves zero trace? Never ship any of this.

## The design under test

- `src/DuetEdit.h` — the vocabulary. Exposes **no tracktion/juce types**; the
  scenario runner includes only this header. That is the engine seam, made
  concrete.
- Ops (insert/move/trim clip, add MIDI note, set automation point) are each
  expressed once and always pass the Edit's `UndoManager` — never `nullptr`.
- `performAction (name, ops)` is the **only** place `beginNewTransaction` is
  called. A producer gesture and `acceptProposal` (all ops of a Proposal) both
  go through it → parity and one-step collapse by construction.
- A `Proposal` is data until accepted; rejection discards data and cannot touch
  the history.

## Run

```sh
sudo apt install -y libasound2-dev libjack-jackd2-dev libglu1-mesa-dev mesa-common-dev

cmake --preset default
cmake --build --preset release

./build/duet_undo_spike_artefacts/Release/duet_undo_spike
```

Scenarios S1–S4 are silent and assert via state digests and undo-stack dumps.
S5 plays an 8 s loop for ~20 s and toggles an audible lead line + bass duck via
accept/undo/redo while the transport rolls — listen for glitches; xruns are
counted and asserted zero.
