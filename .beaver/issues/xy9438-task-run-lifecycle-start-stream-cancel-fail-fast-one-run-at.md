---
id: xy9438
title: 'Task Run lifecycle: start, stream, cancel, fail fast, one run at a time'
state: done
assignee: claude
priority: medium
depends_on:
    - d7h5f5
parent: js437t
created: 2026-08-12T04:01:28Z
updated: 2026-08-26T05:58:10Z
---

## What to build

A Task Run across the seam: `run.start` carrying the prompt and the opening context, the streamed notifications coming back — commentary deltas, tool activity, and the terminal event with its status — and `run.cancel`. The rules around them are the point: one active run at a time, with the panel's input closed while it runs; cancel tears down and reports "canceled"; a provider error, an unreachable backend, or a dead sidecar fails the run fast; nothing queues, nothing retries, and the DAW is never blocked waiting.

This slice also lands the DAW-side listener interface a UI attaches to later. Exercised entirely against the test-double sidecar — no LLM, no Node, no UI.

## Acceptance criteria

- [ ] `run.start` carries the prompt and the opening context — selection kind and ids, playhead bar and beat, whether the transport is playing — and returns immediately; completion arrives later as a terminal event.
- [ ] Streamed commentary deltas reach a registered listener in order, and their concatenation equals what the double sent.
- [ ] Tool-activity start and end notifications reach the listener with their tool names, in the order sent.
- [ ] A second `run.start` while a run is active is rejected and the active run is unaffected.
- [ ] `run.cancel` ends the active run with status canceled; the listener sees exactly one terminal event, and a further cancel of the same run is harmless.
- [ ] Worked: the double reports the run finished with status failed and an error string → the listener sees one failed terminal event carrying that string, and the service accepts a new run immediately afterwards.
- [ ] Sidecar death mid-run fails that run with a terminal event; nothing is queued and nothing is retried, and the next run spawns a fresh sidecar.
- [ ] Events naming an unknown or already-finished run id are ignored, and no terminal event is ever delivered twice for one run.
- [ ] The DAW never blocks on a run: starting, cancelling, and receiving events all return without waiting on the sidecar, asserted against a double that delays its responses.
- [ ] A canceled or failed run leaves no partial state behind: the next run starts as cleanly as the first.

## Notes

**claude** — 2026-08-26T05:58:10Z

Completed 2026-08-26. The Task Run lifecycle exists across the socket seam: start, streamed events, cancel, fail fast, one run at a time — every rule asserted through the protocol against the test-double sidecar.

WHAT LANDED
- modules/duet_collab/include/duet/collab/TaskRun.h — the run vocabulary: OpeningContext (SelectionKind, ids, playhead bar/beat, transportPlaying), ToolPhase, RunStatus, TaskRunListener, RunStart, and backendUnavailableMessage.
- CollaboratorService gains setTaskRunListener, startRun, cancelRun, activeRunId; JsonRpc.h gains rpcError::runAlreadyActive (-32001, inside the range the JSON-RPC spec reserves for a server's own errors).
- tests/CollaboratorHarness.h — TempSocketFolder, Harness, waitUntil, processExists, connectDirectly lifted out of CollaboratorServiceTests.cpp so both suites drive one real service over one real socket with one real child process. Harness takes an executable path so a sidecar that is not there can be driven too.
- tests/TaskRunTests.cpp — twelve cases; tests/sidecar_double gains nine run-* scripts.

DECISIONS A REVIEWER NEEDS
1. Whether the service holds an active run IS the one-run-at-a-time rule, and clearing that run's id under the same lock that reads it IS "no terminal event twice". There is no second guard and no set of finished ids: an event naming a run that is not the one in progress finds nothing and is ignored, which covers an unknown id, a finished one, and words after the last word with the same three lines.
2. startRun does not touch the sidecar on the calling thread. It records the run, asks the service thread to spawn if nothing is connected, and returns. The service thread sends run.start when a connection exists, and fails the run when the spawn failed or the connect deadline passed. This is why the DAW is never held up — asserted against a double that takes 600 ms to call home and 500 ms to answer anything: startRun and cancelRun both return inside 200 ms.
3. Cancel is client-side, as the spec says. The run ends canceled here and run.cancel goes on to stop the work; whatever the sidecar says about that run afterwards is ignored. run.cancel is sent only if run.start was — there is nothing to stop otherwise. cancelRun returns whether this call is the one that ended it, so a second cancel says false and delivers nothing.
4. Every listener call comes from the service thread. cancelRun sets a flag rather than delivering the terminal event itself, so a run's whole event sequence is produced by one thread and "exactly one terminal event, in order" needs no reasoning about interleaving.
5. The listener has a lock of its own (listenerMutex), never held while the state mutex is. That buys two things: clearing a listener waits for the call in flight, so a listener cleared before it is destroyed is never called again; and a listener may call back into the service from inside its own callback without deadlocking. Do not call setTaskRunListener from inside a callback.
6. A listener must not throw — documented on TaskRunListener rather than caught. A first pass wrapped the call in try/catch, which bugprone-empty-catch rejects and rightly: there is no response to make and nowhere in this module to log. The MethodHandler path catches because it must produce a JSON-RPC response either way; a notification has no such obligation, so the contract is the honest tool. Same shape as CollaboratorPanel::Source.
7. Transport failures carry backendUnavailableMessage — the spec's own conversation phrase — and every other failure carries the sidecar's error string unchanged. A surface can therefore show what it is handed without deciding which kind of trouble it was looking at.
8. Two paths beyond the criteria, both "fails fast" and both tested: a run.start answered with a JSON-RPC error fails the run with that message (nobody waits on that request, so without this a refusal would leave the run with no ending at all), and a sidecar that cannot be spawned fails the run rather than letting it sit until the deadline with no word.
9. A run in progress when the connection drops fails with a terminal event — sidecar death, shutdownSidecar, and service teardown all reach it through dropConnection. Every run ends exactly once, including the one the DAW exits on.

TESTING
The seam is the spec's primary one and every criterion is driven through it. Two mechanics worth knowing before the next slice:
- Every run-* script reports the run.start and run.cancel it received, as test.report requests the suite answers. A test that needs to act on a run the sidecar actually has waits for that report — waiting on activeRunId() instead is wrong, because startRun fills it in before the request has gone anywhere. The first cancel test failed exactly there.
- A RecordingListener is declared BEFORE its Harness in every case. The other order destroys the listener while the service is still reporting to it, and that is a pure-virtual-call abort, not a flake. It is the contract the header states, and the tests are the first thing to obey it.

CHECKS — all four green
- Format: clang-format-18 --dry-run --Werror over git ls-files — clean.
- Lint: ./scripts/lint.sh, full sweep — clean. Findings on the way there were real: a nested conditional, an unforwarded forwarding reference, the empty catch above, and eight harnesses that wanted const.
- Build: cmake --build --preset linux-debug -j 4 — clean.
- Test: ctest --preset linux-debug — 308/308, of which 12 are this slice's.

Beyond the four, because the whole substance here is a state machine shared between a caller thread and a service thread:
- linux-asan (ASan + UBSan, vptr on): all 24 [collab] cases pass with no sanitizer output.
- linux-tsan (TSan + UBSan): all 24 pass with zero ThreadSanitizer warnings, stable over four runs.

FACTS FOR THE NEXT SESSION
- Task Run event dispatch is structural, not registered: run.text, run.toolActivity and run.finished are answered inside handleRequest and never reach the setMethodHandler table, which stays free for tool.call and the Tool Vocabulary (v5yhh1).
- Run ids are the DAW's, minted as run-1, run-2, and monotonic per service. Nothing reads their shape.
- The service is still not wired into duet_app, and nothing here asks for it. CollaboratorPanel::Source is the UI-side seam that TaskRunListener will meet; joining them is a later slice's, and neither had to change for this one.
- On this dev machine the TSan binary's Catch2 test-discovery step fails at build time with the documented "unexpected memory mapping", which makes ctest report the suite NOT_BUILT. The binary itself is fine: run it directly under setarch $(uname -m) -R with a Catch2 filter. Worth knowing because the build exits non-zero for a reason that has nothing to do with the code.
