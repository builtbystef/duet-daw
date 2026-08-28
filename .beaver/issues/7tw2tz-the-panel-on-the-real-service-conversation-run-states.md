---
id: 7tw2tz
title: 'The panel on the real service: conversation, run states, estimate marks, development trace'
state: in-progress
priority: medium
labels:
    - needs-review
depends_on:
    - oocnng
    - 2z0y5u
    - 4jipx2
parent: js437t
created: 2026-08-12T04:03:44Z
updated: 2026-08-28T22:53:32Z
---

## What to build

The Collaborator panel stops being driven by its development-only conversation source and starts being driven by the Collaborator service. Sending a message starts one Task Run carrying the opening context — the selection at send time, the playhead position, whether the transport is rolling. Commentary lands in the conversation as it streams. The Task Run card is driven by real events: spinner, the keep-editing hint, rotating friendly status phrases, Cancel.

A canceled run leaves the "task canceled, nothing changed" line; a failed one leaves a single plain error line while the DAW keeps working fully — no offline state, no queue, no retry. Output from a tainted run carries its estimate mark, and opening the mark shows that run's ledger. In development mode only, the raw tool-call trace of a run is visible; the Target Producer sees the friendly phrases instead.

## Acceptance criteria

- [ ] Sending a message starts exactly one Task Run; the composer is disabled while it runs and re-enabled at its terminal event.
- [ ] Commentary appears progressively as it streams, not only when the run finishes.
- [ ] Opening context, worked: with two clips selected and the playhead at bar 9 beat 1 while the transport rolls, the run carries those two clip ids, that position, and playing true; changing the selection after sending does not change what the run carries.
- [ ] Playback, editing, and recording continue unaffected while a run is in flight, asserted with the transport rolling.
- [ ] Cancel ends the run and leaves the "task canceled, nothing changed" line, and no commentary arrives after it.
- [ ] A failed run leaves exactly one plain error line, and saving, playing, and editing all still work afterwards; there is no dedicated offline state, nothing is queued, and nothing retries by itself.
- [ ] Estimate mark, worked: a run that called an estimating tool has its commentary marked, and opening the mark lists the estimated values with their methods and confidences; a run that called none carries no mark.
- [ ] In a development build the raw tool-call trace of a run is inspectable with tool names, arguments, and results; in an ordinary build it is absent and only the status phrases appear.
- [ ] The development-only conversation source is gone from the shipping path.
- [ ] Closure waits for user review.

## Notes

**claude** — 2026-08-17T04:12:26Z

Scope note (2026-08-17): the panel's History section — applied/resolved Suggestions, in-memory per session (kept at s11o4w, mechanics js437t's) — is in-scope of this slice, fed by the Suggestion manager's resolved list. No other ticket covers it.

**claude** — 2026-08-28T22:53:32Z

The panel is on the real service. `duet::app::Collaborator` in `duet_app_core` is
where the two meet: it is the panel's `Source` and the service's
`TaskRunListener` at once, and it owns the read-only Tool Vocabulary against the
open project and the estimate ledger those tools write to. It lives in
`duet_app` because neither of the modules it joins can name the other — one
links no JUCE, the other no engine — and it links no JUCE itself, reaching the
message thread through the two functions its host supplies, which is what lets
`duet_tests` drive the whole of it headless.

Seams used: the socket protocol, which the spec names as this area's primary one
(`tests/CollaboratorTests.cpp` — a real service, a real socket, the test-double
sidecar as a real child process, and the panel on the far end of it), and the
panel's own view-model seam for what the panel answers by itself
(`tests/CollaboratorPanelTests.cpp`). One component case joins them: the estimate
mark is a thing the producer presses, so it is asserted where a window can
(`tests/gui/CollaboratorPanelCanvasTests.cpp`).

Against the criteria:

- One run per Send, and the composer held until its terminal event. `canSend()`
  is now false while a run is on, so the panel keeps the spec's one-run-at-a-time
  rule at the same place the service does — a second Send starts nothing and says
  nothing.
- Commentary streams into one entry: the first delta of a run opens it, every one
  after extends it. Asserted with a sidecar script that streams and then never
  ends, so the whole of what was said is read while the run is still going.
- Opening context, worked: two clip ids, bar 9 beat 1, playing true, read off the
  report the double sends back — and unchanged after the selection moves on.
- Playing, editing and saving all continue with a run in flight, asserted with the
  transport rolling on a real project.
- Cancel leaves the notice and nothing after it; a failed run leaves exactly one
  line, and so does a Collaborator with no sidecar at all — no offline state,
  nothing queued, nothing retried.
- The estimate mark is the run's ledger: opening it lists each guess, what made
  it and how far that routine trusted itself. A run handed none carries none.
- The trace is one run's, with tool names, arguments and results, and a
  development build's alone.
- `ScriptedCollaborator` is gone.

Decisions made:

- **The failure line is the reason.** 4jipx2 wrapped it ("That task failed: X.
  Nothing changed."), which read as two sentences out of one once the service
  started supplying its own ("The Collaborator isn't working right now — try
  again later."). The panel now shows the reason as given, and has its own
  sentence for a failure that said nothing.
- **What a development build is** is `DUET_DEVELOPMENT_BUILD`, defined on
  `duet_gui` for `$<CONFIG:Debug>` and PUBLIC, since it decides a constant in a
  public header. It sets the panel's default; the panel also takes the answer
  from a caller, because the criterion is about both kinds of build and only one
  of them is ever compiled by the suite.
- **The shell holds no source.** `MainShell` exposes its panel and the host wires
  the Collaborator to it — the shell knows about panels, the host knows about the
  seam. `ScriptedSuggestions` stays where 0wdwin put it and is reachable through
  `MainShell::developmentSuggestions()` until 2suzzi puts the Suggestion manager
  behind those surfaces.
- **History** is fed from the Suggestion manager's resolved list through
  `Collaborator::setSuggestions` / `refreshHistory`. Nothing in the app owns a
  manager yet — that is 2suzzi's — so the app's History is empty until then; the
  seam and the section are asserted against a real manager and a real `suggest`
  call.
- **The write-tool is not registered here.** This slice wires the read-only
  vocabulary; `suggest` and what becomes of a Suggestion are 2suzzi's, and half a
  Suggestion path would have been worse than none.
- **Project swaps** end the run that was asking about the project, and the marshal
  the tools are given refuses a read of a project that has gone — the check runs
  on the message thread, which is where the swap runs.

Two facts worth having:

- A `Harness` in the test support destroyed its observation state before the
  service that writes into it, because members go in reverse order of
  declaration and the service was not last. A report still in flight during
  teardown was a write into freed memory, and it crashed this suite about four
  times in five once a test canceled a run at the end. `~Harness` now stops the
  service first. Anything holding a service and something it calls into owes it
  the same order — `ShellHost` stops the service before the Collaborator goes,
  and `PanelOnService` does the same.
- Discovered and published, not done here: 9tdwdq — a project closed while a
  measured or estimated tool call is in flight leaves a render reading an Edit
  that is being torn down, and the same swap races `ToolRegistry`'s map.

Checks: clang-format clean, full `scripts/lint.sh` sweep clean, full build, and
all 466 CTest entries pass.

**This issue's closure waits for your review.** Close it to approve, or note the
changes you want and remove the `needs-review` label.
