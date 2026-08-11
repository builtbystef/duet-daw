# PROTOTYPE — offline-render correctness (xciphe)

Disposable evidence for whether Tracktion offline renders are deterministic and
whether feature assertions can verify Duet-owned instruments and effects through
their public processing seam. Never ship this code.

## Run

```sh
cmake --preset default
cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --build --preset release
ctest --preset release --output-on-failure
```

The executable prints measurements for every render comparison and feature
assertion so the evidence is visible rather than hidden behind pass/fail.

## Evidence produced on the development host

- A fixed two-second Tracktion Edit rendered twice to separate temporary WAV
  destinations is bit-exact: 0 differing samples, maximum absolute difference
  0. The diagnostic fingerprint was `12856307083160462901` in five independent
  Debug processes and in the Release build. The fingerprint is evidence, never
  an assertion or golden value.
- Known MIDI notes A4 and C5 rendered through a Duet-owned stand-in JUCE
  `AudioProcessor`, adapted into the Tracktion plugin graph, measured 439.985 Hz
  and 523.243 Hz. Onsets measured 0.243832 s and 0.998481 s for MIDI positions
  0.25 s and 1.0 s.
- A synthetic 440 Hz sine rendered dry and through a Duet-owned stand-in JUCE
  gain effect measured RMS 0.353553 and 0.176777: exactly -6.0206 dB.
- Debug and Release both pass all 16 assertions. The Debug executable also
  survived ten consecutive CTest runs.

This prototype has only one host. It therefore proves bit-exact repeatability
within this GCC 13/Linux environment and across its Debug/Release builds, not
across different machines, CPUs, compilers, or operating systems. Product tests
must assert measured features with domain tolerances; they must not assert the
diagnostic fingerprint or compare rendered files.

## Transferable test patterns

1. Construct an Edit entirely from synthetic source material, render to a fresh
   temporary destination with fixed sample rate, block size, bit depth, and
   dithering disabled, then load the result into an `AudioBuffer<float>`.
2. For an instrument, insert known MIDI and the Duet-owned processor through its
   public Tracktion plugin seam. Measure pitch and onset from the rendered audio.
3. For an effect, render the same synthetic source before and after inserting the
   Duet-owned processor, then assert a feature ratio such as RMS or spectral-bin
   gain in decibels.
4. Analysis routines validate from synthetic reference signals in their own
   focused tests. This spike needed test-owned rising-zero-crossing pitch,
   amplitude-threshold onset, and RMS measurements because no production
   analysis DSP exists yet.

## Traps found

- Tracktion places a MIDI note-on at the start of the 512-sample render block
  containing its requested position. Instrument onset assertions must allow up
  to one render block early, or deliberately render with a smaller block size.
- Use a fresh destination file for each render; this also avoids Tracktion's
  audio-file cache obscuring a repeated render.
- `catch_discover_tests` in Catch2 3.15 writes its discovery JSON under
  `XDG_RUNTIME_DIR`. That directory is read-only in this sandbox, so the spike
  registers its one Catch2 executable with an explicit `add_test`.
- Disabling Tracktion's automatic device-manager initialization removes JACK and
  audio-device probing, but JUCE still attempts one ALSA MIDI enumeration when
  the first Engine starts. On a host without `/dev/snd/seq` it logs a non-fatal
  assertion; every offline render still completes.
- The transport warm-up/retry trap from `b1j3me` does not apply: `Renderer::RenderTask`
  drives the graph directly and never calls transport `play()`.
