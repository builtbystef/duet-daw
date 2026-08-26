# The Collaborator's fixture corpus

Seven projects, each one a musical situation recognisable from a real session.
They began as the hand-written JSON of the fod077 prototype, which asked whether
a model reasons usefully about structured music data with no audio; the answer
was yes, and the schemas it validated became the Tool Vocabulary. They are here
because that makes them the regression corpus for the tools: `ToolFixtureTests`
builds each one as a real Duet project through the edit vocabulary, asks the
five project-read tools about it over the socket, and asserts every value this
file states.

A fixture is both the recipe and the expectation. Nothing is asserted that the
file does not say, and everything it says is asserted.

## What changed in the rebuild

fod077's fixtures described projects; these are projects. Three consequences:

- **The analysis is gone.** Its RMS, LUFS and spectral bands were written by
  hand, and a real project's are measured from its audio. They come back with
  the analysis layer, measured.
- **The plugins are Duet's.** Milestone one ships the engine's EQ, compressor
  and reverb, and its 4OSC and sampler. A fod077 chain naming a saturator, a
  filter, a chorus or a bit crusher has no Duet device to be, so the rebuild
  drops it and each file's `rebuild.dropped` says which.
- **The sends have somewhere to land.** fod077 sent to a `reverb` and a `delay`
  that were names and not tracks. Here they are group tracks, listed in
  `rebuild.bussesAdded`; the reverb bus carries a reverb, and the delay bus
  carries nothing, because Duet has no delay yet.

fod077 recorded two defects in its own fixtures, and both are fixed. fixture-a
named pitch 36 `C1` on the kick and `C2` on the bass; a tool result carries the
pitch and no name at all, so there is nothing left to disagree. fixture-f's pad
held Dm and F across the Rhodes' Bb and Am bars, which made the control fixture
ambiguous; its pad follows the changes now.

## The shape of a file

```
id          the fixture's name
prompt      what the producer asked fod077, kept because it says what the
            fixture is about
rebuild     notes, and what the rebuild dropped or added
arrangement key, tempoBpm, timeSignature, sections [{ name, startBar, endBar }]
tracks      in project order, the master last
```

A track:

```
id          this file's name for it, which the test maps onto the project's own
name        what the producer called it
kind        midi | audio | group | master
instrument  synth | sampler, on a midi track
output      another track's id; absent means the master
clips       [{ name, startBar, lengthBars, pattern? }]
patterns    { name: { lengthBars, notes: [{ pitch, startBeats, lengthBeats,
            velocity }] } }
plugins     [{ builtin, parameters: { paramId: value } }], in chain order and
            after the instrument
automation  [{ target, points: [{ timeBeats, value }] }], where a target is
            "volume", "pan", or { pluginIndex, paramId }
mixer       { volumeDb, pan, mute, solo, sends: [{ busId, levelDb }] }
```

A clip whose `lengthBars` is longer than its pattern's is looped over that
pattern, which is how one bar of a kick fills thirty-two.
