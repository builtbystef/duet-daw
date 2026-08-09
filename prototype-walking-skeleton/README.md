# PROTOTYPE — walking skeleton (roadmap node ddp1qt)

Disposable code. Proves JUCE 9 + Tracktion Engine develop + the psmj4y toolchain
recipe end to end on Linux. Never ship any of this.

## Run

```sh
sudo apt install -y libasound2-dev libjack-jackd2-dev libglu1-mesa-dev mesa-common-dev

cmake --preset default          # FetchContent clones JUCE 9 + Tracktion develop (pinned SHAs)
cmake --build --preset release  # measure this: psmj4y expects a cold build in the 4-11 min band

pw-jack ./build/Release/duet_skeleton_artefacts/Release/"Duet Walking Skeleton PROTOTYPE"
```

(`pw-jack` because pipewire-jack is not enabled system-wide on this machine;
plain launch uses ALSA, which also works.)

## What to exercise

1. **Play** — 4 tone tracks + 1 MIDI/4OSC track, looping over 8 s.
2. **Mutate once / Auto-mutate** — structural model changes mid-playback
   (clip move + clip add/remove). Listen for glitches: this is the
   TreeWatcher → graph-rebuild smoothness question.
3. **Save/Reload test** — writes a custom property onto an engine-owned TRACK
   node, saves, reloads, reports SURVIVED/LOST (rquzdc rider).
4. **Benchmark + Auto-scroll + Zoom cycle** — dense timeline (60 tracks,
   ~3600 clips, waveforms + piano-roll grids). FPS prints to stdout every
   120 frames. Toggle **OpenGL** to switch the viewport between the software
   renderer and an attached OpenGLContext, and compare.
