---
id: d7h5f5
title: 'The socket seam: Collaborator service, JSON-RPC transport, and the sidecar process'
state: done
assignee: claude
priority: high
depends_on:
    - sea14w
parent: js437t
created: 2026-08-12T04:01:14Z
updated: 2026-08-20T07:24:48Z
---

## What to build

The DAW half of the AI seam (ADR 0003). A Collaborator service module that listens on a local socket, speaks newline-delimited JSON-RPC 2.0, and owns the sidecar as a child process: spawned lazily on first Collaborator use with the socket path as its argument, killed on DAW exit, respawned on demand when found dead. `configure` and `shutdown` round-trip across it.

The socket is serviced by the service's own thread — nothing here runs on the message thread or the audio thread. Everything is verified against a test-double sidecar that speaks the protocol and nothing else: no Node, no LLM, no UI. That double is the harness every later protocol slice reuses, and it is also the escape-hatch guarantee — anything that speaks this protocol can replace the sidecar.

## Acceptance criteria

- [ ] With the service started, a test-double sidecar connects to the socket and `configure` returns a success result.
- [ ] Framing, worked: two complete requests delivered in a single write, each newline-terminated, are handled as two requests; a request carrying an escaped newline inside a string value is handled as one.
- [ ] A request split across several reads is handled once its terminating newline arrives, and not before.
- [ ] A malformed line yields a JSON-RPC parse error response and the connection survives; an unknown method yields a method-not-found error and the connection survives.
- [ ] The sidecar is spawned on first Collaborator use, not at DAW start: a session that never invokes the Collaborator spawns no child process.
- [ ] A sidecar killed externally is detected, and the next use spawns a fresh one whose `configure` round-trips again.
- [ ] DAW exit terminates the sidecar and removes the socket; no orphan process and no stale socket file survive.
- [ ] `shutdown` ends the sidecar cleanly, and a second `shutdown` against an already-dead sidecar is harmless.
- [ ] A second connection attempt while a sidecar is connected is refused, and the connected sidecar is unaffected.
- [ ] All socket, framing, and process-management code runs on the service's own thread; none of it runs on the message thread or the audio thread, and none of it takes a lock the audio callback can take.

## Notes

**claude** — 2026-08-20T07:24:48Z

Completed 2026-08-20. The socket seam exists: a Collaborator service that listens, frames JSON-RPC 2.0, and owns the sidecar process, verified end to end against a test-double sidecar.

WHAT LANDED
- modules/duet_collab (duet::collab) — CollaboratorService, and behind it LineReader (framing), LocalSocketServer (the AF_UNIX listener), SidecarProcess (spawn/watch/end). Public interface: JsonRpc.h (Json, the error codes, RpcError, RpcOutcome) and CollaboratorService.h (start/stop, configure, shutdownSidecar, setMethodHandler, isSidecarRunning, sidecarProcessId, serviceThreadId).
- tests/sidecar_double — a program that speaks the protocol and nothing else, sharing no code with the service, driven by a script name in its second argument. It is the harness xy9438 and every later protocol slice reuses.
- tests/CollaboratorServiceTests.cpp — twelve cases, one per acceptance criterion, all driving the real service over a real socket with a real child process.
- nlohmann/json 3.12.0 (65ee68451d8eb2b5f3a30b410476ab83deb3289b, MIT) added by FetchContent with a full-SHA pin, as spec js437t names it. ARCHITECTURE.md's AI-integration bullet now describes the built half.

DECISIONS A REVIEWER NEEDS
1. The socket is POSIX AF_UNIX, not a JUCE primitive, and that is a correction to the spec's "local sockets use JUCE's own primitives" line rather than a preference. JUCE 9 has no Unix domain socket at all — grep the vendored tree for AF_UNIX or sockaddr_un and it is empty; StreamingSocket is TCP and NamedPipe is a pair of FIFOs. The criteria are written on a socket *path*: it is what the sidecar is launched with, and "no stale socket file survives" is an assertion about a file. The binding half of the spec's line — "no new dependency" — holds exactly: the socket, the poll loop and the child process are libc calls.
2. The module links no engine, no JUCE, nothing graphical, and only nlohmann/json. That is what makes the last acceptance criterion structural: a module that cannot name an audio object cannot share a lock with the audio callback, and cannot post to the message thread either. It is also why duet_tests can link the real service and drive the real seam.
3. nlohmann::json is in the public header, not hidden behind a Duet type. The payloads that cross this seam are JSON in both directions and the tool results of the later slices are JSON objects; a second shape to copy them into would buy nothing. The include directory is marked SYSTEM, the same treatment duet_gui_fonts gets, so the lint sweep cannot reach a vendored header.
4. Requests are synchronous for the caller: configure() blocks its thread until the answer or the timeout. That is what the criterion asks of configure, and it is right for a message-thread caller only because nothing in milestone one calls it from there yet. run.start is explicitly non-blocking and is xy9438's, and the listener interface it lands is where the asynchrony belongs.
5. Spawn is lazy and it is the *request* that spawns: ensureSidecar posts the spawn to the service thread and waits for the connection. start() only puts the socket in place, so a session that never asks the Collaborator anything has no child process — asserted, not asserted-about.
6. A dropped connection and a dead sidecar are one event. EOF on the socket terminates and reaps the child, so a sidecar killed from outside stops counting as one within a poll interval, and the next request spawns a fresh one. shutdownSidecar() spawns nothing: with nothing running it succeeds and does nothing, which is what makes the second shutdown harmless.
7. The second connection is refused by accepting and immediately closing it. The listen backlog means connect() succeeds at the kernel level whatever we do, so "refused" can only mean the peer gets an immediate EOF — which is what the test reads.
8. posix_spawn rather than fork/exec: this process is multithreaded when it spawns, and only async-signal-safe calls are legal between a fork and its exec. Every descriptor the service opens is CLOEXEC (SOCK_CLOEXEC, EFD_CLOEXEC, accept4), so the child inherits none of them — least of all the listening socket, which it would otherwise hold open past the DAW's exit.
9. Three NOLINTNEXTLINE suppressions, all the same one: cppcoreguidelines-pro-type-reinterpret-cast on the cast to sockaddr, which is the sockets API's calling convention. Nothing else is suppressed and no check was switched off project-wide.

TESTING
The seam is the one the spec names — the socket protocol, outermost — and every criterion is asserted through it. Two mechanics are worth knowing before writing the next slice's tests:
- The double reports what it saw back over the same connection, as `test.report` requests the suite answers with a registered handler. So an assertion about a response the double received is made in this process, with no child-stdout parsing and no race.
- A message written in pieces cannot have anything else written between them — one stream, one message at a time — so the split-read test's double writes its three pieces only after replying to a request, and the test's own clock starts when that reply lands. An earlier attempt released the newline on a second inbound request instead; the reply to that request concatenated onto the unterminated line and corrupted the stream. That is a property of the protocol, not a bug that was fixed.

CHECKS — all four green
- Format: clang-format-18 --dry-run --Werror over git ls-files — clean.
- Lint: ./scripts/lint.sh, full sweep — clean.
- Build: cmake --build --preset linux-debug -j 4 — clean.
- Test: ctest --preset linux-debug — 102/102, of which 12 are this slice's. The collab cases were run repeatedly for flakiness and were stable.

Beyond the four, because a module whose whole substance is a thread, a socket and a child process deserves more than the push gate:
- Release (CI's other matrix leg): built and 102/102.
- linux-asan (ASan + UBSan, vptr on): the 12 collab cases pass with no sanitizer output at all — no ASan error, no UBSan runtime error, no leak.
- linux-tsan (TSan + UBSan): the 12 collab cases pass with zero ThreadSanitizer warnings, stable over three runs. Getting there took a detour worth recording — below.

THE TSAN DETOUR, AND A WRONG TURN INSIDE IT
The TSan binary fails to start on the dev machine about four times in five: `FATAL: ThreadSanitizer: unexpected memory mapping`, before a single test runs, with a core dump on some of the rest. My first reading of it was wrong and I acted on it: seeing the spawn-dependent cases fail, I concluded that a sanitized process cannot spawn a sanitized child and built the double without sanitizers to prove it. The next run failed at startup with no child involved at all, which killed that theory — the same failure hits an untouched engine test in the same binary, so it is this kernel's address-space randomization and nothing to do with spawning or with Duet. The CMake change was reverted; the double is built exactly like everything else. `setarch $(uname -m) -R` makes TSan reliable here, and AGENTS.md now says so beside the nightly commands, since that file promises the nightly reproduces in one command and on this machine it does not.

FACTS FOR THE NEXT SESSION
- The service is not wired into duet_app, and nothing in this issue asks for it. There is no Collaborator UI to own one yet, and xy9438 is also service-level. "DAW exit terminates the sidecar" is the service's destructor, which the test exercises by destroying it.
- Configuration carries the socket path and the sidecar launch, so production has no default sidecar path yet — the bundled binary's location is the packaging slice's to name.
- sun_path is 108 bytes, so a socket path is not free-form. LocalSocketServer throws rather than truncating, and the suite puts its socket in a short temp folder.
- Notifications already work in both directions: an inbound message with no id is dispatched to its handler and answered with nothing, which is what run.text and run.finished need.
