# Duet DAW

**A native desktop digital audio workstation built for collaboration between a music producer and an AI.**

[![CI](https://github.com/builtbystef/duet-daw/actions/workflows/ci.yml/badge.svg)](https://github.com/builtbystef/duet-daw/actions/workflows/ci.yml)
[![Nightly sanitizers](https://github.com/builtbystef/duet-daw/actions/workflows/nightly.yml/badge.svg)](https://github.com/builtbystef/duet-daw/actions/workflows/nightly.yml)
[![License: AGPL v3](https://img.shields.io/badge/license-AGPL--3.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](#building-from-source)
[![Platform: Linux](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](#status)

Duet is a DAW written in C++ on [JUCE](https://juce.com) and [Tracktion Engine](https://www.tracktion.com/develop/tracktion-engine), with an AI participant, the **Collaborator**, sitting in a dock beside the arrangement. The Collaborator perceives the project through a closed set of read-only tools rather than through audio, and it never changes anything on its own. Its output is a **Suggestion** the producer can audition, revise, cherry-pick, accept, or reject, and an accepted Suggestion lands as one ordinary undo step.

## Screenshots

Duet follows the desktop's theme and ships both palettes. Light on the left, dark on the right.

<table>
  <tr>
    <td align="center"><b>Arrangement</b> with the Browser, the Mixer and the Collaborator dock</td>
    <td align="center"></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/light/07-arrangement-full.png" alt="Arrangement view, light theme" width="100%"></td>
    <td><img src="docs/screenshots/dark/07-arrangement-full.png" alt="Arrangement view, dark theme" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><b>Piano roll</b></td>
    <td align="center"></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/light/04-piano-roll-max.png" alt="Piano roll, light theme" width="100%"></td>
    <td><img src="docs/screenshots/dark/04-piano-roll-max.png" alt="Piano roll, dark theme" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><b>Mixer</b></td>
    <td align="center"></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/light/06-mixer-max.png" alt="Mixer, light theme" width="100%"></td>
    <td><img src="docs/screenshots/dark/06-mixer-max.png" alt="Mixer, dark theme" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><b>Model access</b>: bring your own key</td>
    <td align="center"></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/light/12-provider-dialog.png" alt="Provider settings, light theme" width="100%"></td>
    <td><img src="docs/screenshots/dark/12-provider-dialog.png" alt="Provider settings, dark theme" width="100%"></td>
  </tr>
</table>

The full set, including the menus, the settings tabs and the export dialog, is in [docs/screenshots/](docs/screenshots/).

## How the Collaborator works

Most "AI in a DAW" designs hand a model rendered audio and hope. Duet is built on a different bet, recorded in [ADR 0002](docs/adr/0002-collaborator-perceives-through-tools-never-audio.md): the model reads the project the way a session musician reads the room.

- **A closed Tool Vocabulary.** The Collaborator sees the project through deterministic, read-only tools: the tracks, the clips, the notes, the mixer, the plugin chains, and measured analysis of what a track actually renders.
- **Provenance on every fact.** Each tool result says whether a value was read from the project, measured from rendered audio, or estimated. A guess crosses the seam wrapped as an Estimate, and a run that leaned on one says so afterwards.
- **One write tool, one edit vocabulary.** The Collaborator's only way to change anything is a Suggestion, written in the same edit operations the producer has in the interface. Nothing enters the project until the producer accepts it.
- **The Duet Loop.** A Suggestion is a conversation, not a patch. It is shown as ghosts where it would land, it can be auditioned without touching the undo history, each Element can be accepted or rejected on its own, a rejection with a reason becomes the next prompt, and a Suggestion the producer has edited underneath is marked stale and can be asked again.
- **Bring your own model.** The producer signs in to their own provider with their own key or subscription and picks a model. Duet privileges none of them.
- **Off the audio thread, always.** The agent loop runs in a separate sidecar process behind a local socket ([ADR 0003](docs/adr/0003-pi-sdk-sidecar-behind-a-socket-protocol.md)). A sidecar that dies is one failed Task Run, not a DAW that stopped playing.

## Features

Milestone one targets the bedroom electronic producer working in the box.

- **Arrangement timeline** with an adaptive grid, pointer-anchored zoom, follow-playhead, named sections and a project key
- **Piano roll** for MIDI clips, and **automation lanes** under each track for volume, pan and plugin parameters
- **Mixer** with faders, pan, sends, insert chains, cycle-safe routing and peak-hold meters
- **Built-in instruments and effects** from the engine, and **VST3 hosting** with an out-of-process scanner, native plugin editors and a preset library
- **Recording** with record-arm, input selection and monitoring, where a take lands as one undo step under the project folder
- **Browser** for instruments, effects, plugins and sample folders, with source audition through the main output
- **Import** of audio files and Standard MIDI Files, and **export** of the whole project or a bar range in the format, depth and rate you choose
- **Projects as folders**, snapshot saves, autosave with recovery, and a per-project layout that a save remembers
- **Light and dark themes** that follow the desktop, an interface scale setting, and the Inter typeface compiled in
- **Undo and redo** across every gesture and every accepted Suggestion, with one boundary for both

## Status

Duet is pre-release and under active development. Version 0.1.0 builds and runs on Linux, where the checks in CI keep it that way. There are no packaged builds yet, and the dev-machine audio stack is PipeWire through its JACK shim. Other platforms are not built or tested.

The AI seam is built end to end: the sidecar, the socket protocol, the Tool Vocabulary, the Suggestion manager and the panel in the interface. A provider still has to be set up before the Collaborator answers.

## Building from source

### Prerequisites

Duet builds on Ubuntu 24.04 with CMake 3.22 or later, Ninja, and a C++20 compiler. Clang 18 and GCC both work; the sanitizer presets name the compiler they need. The apt packages CI installs are the ones a build needs:

```sh
sudo apt-get install --no-install-recommends \
  ninja-build ccache \
  libasound2-dev libjack-jackd2-dev \
  libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxi-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libglu1-mesa-dev mesa-common-dev
```

[Bun](https://bun.sh) 1.3 builds the Collaborator sidecar. Without it the sidecar target does not exist and the sidecar tests skip themselves, so the DAW still builds, but the Collaborator cannot answer. Pass `-D DUET_SIDECAR_ENABLED=OFF` to turn the sidecar off on purpose.

JUCE, Tracktion Engine, nlohmann/json and Catch2 are fetched by CMake at configure time, pinned to exact commits. Nothing needs to be installed for them.

### Build and run

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug -j 4
ctest --preset linux-debug --output-on-failure
pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet
```

A Tracktion translation unit peaks near 2 GB of memory, so keep the job count at or below half your RAM in gigabytes. The `linux-release` preset builds the optimised binary the same way. `ccache` is used when it is on the machine, which is what makes a rebuild after a header-only change cheap.

The `pw-jack` wrapper is needed on a PipeWire desktop, where pipewire-jack is not the system-wide libjack. To put Duet in the desktop's application list, with its icon and the same wrapper, run:

```sh
./scripts/install-desktop-entry.sh
```

### Setting up the Collaborator

Open **Settings** from the Duet menu and go to the **Collaborator** tab. Sign in to a provider with your own API key or subscription, then pick the model the next Task Run should use. Credentials are stored beside the app-global settings, outside every project folder. Until a provider is set up, the Collaborator panel shows the way to that tab instead of a composer.

## Repository layout

```
modules/
  duet_model/        the edit vocabulary over the engine, Actions, undo, rendering
  duet_persistence/  project folders, snapshot save, autosave and recovery
  duet_gui/          the interface: view-models without JUCE, and thin components
  duet_app/          the application shell, the Collaborator host, model access
  duet_collab/       the Collaborator service: socket, Task Runs, tools, Suggestions
  duet_realtime/     the audio-callback annotation and the RTSan seam
sidecar/             the agent loop, built by bun into one standalone binary
tests/               Catch2 suite, RT probes, VST3 fixtures, the sidecar double
scripts/             lint sweep, desktop entry installer
docs/                architecture, glossary, ADRs, engine notes, screenshots
```

## Architecture in brief

Duet is a set of modules with deliberate seams between them, described in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

- **The engine seam.** The model's public interface exposes no engine or JUCE types. Every project change, a producer gesture or an accepted Suggestion alike, is one named Action, and an Action is the only undo boundary ([ADR 0004](docs/adr/0004-edit-vocabulary-actions-shared-undo.md)).
- **The interface seam.** Every surface splits into a paintless view-model and a thin component that only paints and forwards events. The view-models link no JUCE at all, so the test suite drives the whole interface headless.
- **The AI seam.** The DAW is a socket server, the sidecar is a client, and everything between them is newline-delimited JSON-RPC. Reads of the project marshal onto the message thread, analysis renders on worker threads, and nothing on the AI side touches the audio thread.
- **Audio testing.** Tests assert on features of a rendered signal rather than on golden files, and RealtimeSanitizer is the backstop for real-time safety ([ADR 0006](docs/adr/0006-audio-testing-feature-assertions-rtsan.md)).

The decisions behind these shapes are recorded in [docs/adr/](docs/adr/), and the project's shared vocabulary is in [docs/GLOSSARY.md](docs/GLOSSARY.md).

## Development

The checks CI runs on every push are the ones to run before a commit. They are listed with their reasons in [AGENTS.md](AGENTS.md).

```sh
clang-format-18 --dry-run --Werror $(git ls-files '*.cpp' '*.h')   # format
./scripts/lint.sh                                                    # clang-tidy, Duet's sources only
ctest --preset linux-debug --output-on-failure                       # tests
```

A nightly workflow runs the three sanitizer configurations the push gate is too slow to carry: `linux-asan`, `linux-tsan` and `linux-rtsan`, each with its own preset. The coding conventions beyond the linter are in [docs/CODING_STANDARDS.md](docs/CODING_STANDARDS.md), and what the engine actually does, one fact per entry, is in [docs/ENGINE_NOTES.md](docs/ENGINE_NOTES.md).

## Contributing

Bug reports, feature requests, and discussion are welcome. Please open an issue.

Outside code contributions are not accepted at this time. The project keeps the option of a commercial edition open, which requires holding relicensing rights over every line, and running a Contributor License Agreement is overhead the project does not want yet. The reasoning is in [CONTRIBUTING.md](CONTRIBUTING.md) and [ADR 0001](docs/adr/0001-agplv3-no-outside-contributions.md).

## License

Duet is released under the [GNU Affero General Public License v3](LICENSE). The third-party artifacts Duet redistributes beside its own binary, and the notices they oblige it to carry, are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
