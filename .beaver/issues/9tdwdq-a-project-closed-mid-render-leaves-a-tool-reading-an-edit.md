---
id: 9tdwdq
title: A project closed mid-render leaves a tool reading an Edit that has gone
state: todo
priority: medium
depends_on:
    - 7tw2tz
created: 2026-08-28T22:18:40Z
updated: 2026-08-29T06:22:13Z
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

- [ ] Opening or creating another project while a measured or estimated tool call
      is in flight never reads a `Session` that has been destroyed, asserted with
      a render that is deliberately slow.
- [ ] The producer is not made to wait for the render: swapping the project stays
      a message-thread call that returns at once.
- [ ] The run about the project that has gone still ends, exactly once, with the
      panel showing one ending.

## Notes

**claude** — 2026-08-28T22:27:53Z

Also in scope, found in the same place: `Collaborator::setSession` calls `ToolRegistry::add` on the message thread while the service thread may be inside `ToolRegistry::call` for a tool of the same project. The registry is a `std::map` and neither side takes a lock, so a project swap during a tool call is a data race on the container as well as on what the tools read. Whatever closes the render window should close this one — the two have the same shape and the same fix.

**claude** — 2026-08-29T06:22:13Z

Correction (2026-08-29): this issue's body states that 'every project read is safe: the marshal the tools are given checks that the project it was built for is still the open one, so a read of a project that has gone never starts.' That is not what the code does. ProjectTools::read dereferences its own marshal and session members before the marshal's guard is ever reached, so the guard protects the Session and does nothing for a destroyed ProjectTools. Collaborator::setSession destroys the tool objects while ToolRegistry keeps the [this] lambdas they registered, and the registry has no removal. Recorded in full on 7tw2tz. Do not close this as already-handled on the strength of that sentence.
