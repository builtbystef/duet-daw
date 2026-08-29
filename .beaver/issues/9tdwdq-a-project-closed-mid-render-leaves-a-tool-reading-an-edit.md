---
id: 9tdwdq
title: A project closed mid-render leaves a tool reading an Edit that has gone
state: done
assignee: claude
priority: medium
depends_on:
    - 7tw2tz
created: 2026-08-28T22:18:40Z
updated: 2026-08-29T09:34:31Z
---

## What is wrong

`duet::app::Collaborator::setSession` swaps the Tool Vocabulary when the
producer opens or creates another project. Every *project read* is safe: the
marshal the tools are given checks, on the message thread and therefore in the
same place the swap happens, that the project it was built for is still the open
one, so a read of a project that has gone never starts.

A *render* is not. `get_track_analysis` and `estimate_audio_content` render the
track on the Collaborator service's own thread, through a `TrackRenderer` closure
holding `model::Session&`, and nothing on that path is synchronised with the
message thread. The swap cancels the run in progress and `TrackRenders` asks
whether the run is still wanted between blocks, so a render stops soon — but not
before `setSession` returns and the host destroys the old `Session`. A producer
who opens another project while a first measurement is in flight can therefore be
inside `renderDetachedTrackToFile` on an Edit that is being torn down.

Nothing has been seen to fail: the window is the width of one render block, and
reaching it means opening a project during the seconds a first analysis costs.
It is a real dangling reference all the same.

## Acceptance criteria

- [x] Opening or creating another project while a measured or estimated tool call
      is in flight never reads a `Session` that has been destroyed, asserted with
      a render that is deliberately slow.
- [x] The producer is not made to wait for the render: swapping the project stays
      a message-thread call that returns at once.
- [x] The run about the project that has gone still ends, exactly once, with the
      panel showing one ending.

## Notes

**claude** — 2026-08-28T22:27:53Z

Also in scope, found in the same place: `Collaborator::setSession` calls `ToolRegistry::add` on the message thread while the service thread may be inside `ToolRegistry::call` for a tool of the same project. The registry is a `std::map` and neither side takes a lock, so a project swap during a tool call is a data race on the container as well as on what the tools read. Whatever closes the render window should close this one — the two have the same shape and the same fix.

**claude** — 2026-08-29T06:22:13Z

Correction (2026-08-29): this issue's body states that 'every project read is safe: the marshal the tools are given checks that the project it was built for is still the open one, so a read of a project that has gone never starts.' That is not what the code does. ProjectTools::read dereferences its own marshal and session members before the marshal's guard is ever reached, so the guard protects the Session and does nothing for a destroyed ProjectTools. Collaborator::setSession destroys the tool objects while ToolRegistry keeps the [this] lambdas they registered, and the registry has no removal. Recorded in full on 7tw2tz. Do not close this as already-handled on the strength of that sentence.

**claude** — 2026-08-29T07:23:57Z

One more of the same shape, found while reviewing 7tw2tz and left for this
issue because the fix is the same fix: `~Collaborator` clears the panel source,
the run listener, the estimate ledger and the life token, but not the
`"tool.call"` method handler it registered on the service in its constructor.
The handler captures `this`. A service that outlives its Collaborator — and
`CollaboratorService` is owned separately in both hosts — dispatches that
handler into freed memory.

Both call sites happen to stop the service first, so nothing has been seen to
fail. `CollaboratorService::Impl` already answers an unregistered method with
methodNotFound (src/CollaboratorService.cpp:490), so the fix is
`service.setMethodHandler ("tool.call", {})` in the destructor, beside the
three unregistrations already there. What it wants is a seam to demonstrate it
on: the sidecar double only issues `tool.call` from inside a run, so there is
no deterministic way today to make one arrive after the Collaborator has gone.

**claude** — 2026-08-29T09:34:26Z

Done, at the seam the criteria name, and every one of the three notes on this
issue is closed with it.

**What was wrong, once all four pieces are put together.** A tool call is
answered on the Collaborator service's own thread, and everything it reaches —
the four tool objects, the renders they keep, and the project itself — was
owned by `Collaborator` and destroyed on the message thread by `setSession`,
with nothing between the two. `registry.clear()` (7tw2tz) closed the window
after a swap; it did nothing for a call already inside one.

**Why the obvious fix is not available.** `setSession` cannot wait for the
render. A detached render's setup and teardown are the message thread's —
`renderTracksToFile` puts its guards up and takes them down inside
`callBlockingCatching`, and `~DetachedProject` resets the copy the same way
(engine notes, "A render's setup and teardown are the message thread's, its
blocks are not"). A message thread blocked waiting for a render to abandon is a
message thread the render is waiting for. That is why the second criterion is a
constraint and not a preference.

**What was built instead.** The project outlives the swap for exactly as long
as a call is inside it.

- `duet::collab::ToolRegistry` replaces its whole vocabulary as one — the tools
  and, new, a hold on the thing they read — behind a mutex taken only over the
  swap. `call()` takes that hold before it reads the tool name and lets it go
  when the tool has answered, so `add`/`clear` from the message thread no longer
  race the `std::map` and no longer pull an object out from under a call. That
  closes the first note.
- `Collaborator::OpenProject` is one open project and everything built over it:
  the `shared_ptr<model::Session>`, the renders, and the four tool objects.
  `setSession` hands it to the registry as that hold, and a swap moves it to a
  retired list and returns — no wait anywhere on the path. `ProjectTools::read`
  dereferencing its own members before it reaches the marshal's guard is
  therefore safe: the object is alive because the call holds it. That closes the
  second note.
- A retired project is put down on the message thread — in `setSession` and
  after every posted run event — because the engine's teardown is the message
  thread's work and the service thread that let the last call out cannot do it.
- `duet::persistence::Project` holds its `Session` by `shared_ptr` and lends one
  through `sessionHandle()`; `~Project` clears the model's change callback,
  which was over the facade and can now outlive it.
- `~Collaborator` unregisters the `"tool.call"` handler, beside the three
  unregistrations already there. That closes the third note.

**The seam for a slow render.** `Collaborator` takes an optional
`TrackRendererFor` — the project's own detached offline render unless the host
names another. It is the one part of answering a tool call that is not the
message thread's, which is exactly the part that has to be held still to drive a
swap into the middle of it.

**Tests, each red without its half of the fix.**

- "a project closed while a measurement renders is not read once it has gone"
  drives a real `get_track_analysis` through the socket and the sidecar double,
  stops the render on the service thread, and swaps the project there. It
  asserts the swap returned with the render still inside (criterion 2); that
  dropping the producer's own hold leaves the project alive (criterion 1); that
  the render read the same revision on the far side of the wait; that the panel
  shows exactly one ending (criterion 3); and that the project was put down on
  the message thread. Without the registry's hold the project-alive assertion
  fails; without the retired list the put-down-thread assertion fails.
- "a Collaborator that has gone answers no tool call either" starts the run on
  the service directly, there being no Collaborator left to ask for one, and
  requires methodNotFound. Without the destructor line it SEGVs — the seam the
  third note said did not exist is starting the run from the service rather than
  through the Collaborator.

**One thing worth knowing about the test process.** A `Session` carries this
process's JUCE initialiser, so the last one put down takes the message loop with
it — and a project a call was inside is put down from inside that loop. The
application holds an initialiser for its whole life and never meets this;
`duet::testing::MessageLoop` is how a case holds one too. The first run of the
new case died in `InternalMessageQueue::popNextMessage` before that.

Checks on the whole tree: clang-format clean, full `scripts/lint.sh` sweep
clean, full build, 499/499 CTest entries pass.
