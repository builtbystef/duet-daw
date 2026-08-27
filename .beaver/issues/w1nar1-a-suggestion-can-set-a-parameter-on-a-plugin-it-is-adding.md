---
id: w1nar1
title: A Suggestion can set a parameter on a plugin it is adding, and nothing checks it
state: todo
priority: low
labels:
    - bug
parent: js437t
created: 2026-08-27T06:53:13Z
updated: 2026-08-27T06:53:13Z
---

## What is wrong

`suggest` holds every value it is given inside the range the thing it is written
to has, except one: `plugin.setParam` against a plugin the same element is
adding. The plugin does not exist yet, so nothing can be asked what parameters
it has or what they may be, and both the parameter id and the value cross
unchecked.

What the producer sees is an operation that lands and does nothing. A Suggestion
that says "add a compressor and set its ratio to 4" gets the compressor; a
`paramId` the plugin does not have is ignored by the model, and a value outside
the parameter's range is held at an end of it. Neither says so, and the
Collaborator has no way to find out — the same silent failure v6ac5c was about,
in the one place that decision could not reach.

Every other write is checked: found while building the vocabulary (cwz0of), and
recorded in `SuggestTool.h` as the one value that crosses unchecked.

## What to decide, then build

Duet ships the built-ins and knows their parameters, so the fact exists — it is
just not reachable without an instance. Either the model states what a built-in
has (a parameter list per `BuiltinPlugin`, ids, units and ranges, off the same
table `unitsOfBuiltinParameter` already is), or `suggest` refuses to set a
parameter on a plugin an earlier operation adds and the Collaborator is told to
add the plugin in one element and set it in the next — which the closure
principle would have to answer for, since the producer can do both in one
gesture.

An external plugin an element adds is the harder half: Duet does not own its
mapping, and 0..1 is the only thing that can be said about it, which is at least
something that can be checked.

## Acceptance criteria

- [ ] An element that adds a built-in and then sets a parameter it does not have
      is refused, naming the parameter and the operation's position.
- [ ] An element that adds a built-in and then sets one of its parameters outside
      its range is refused the same way, and at a legal value is accepted.
- [ ] An element that adds an external plugin and then sets a parameter outside
      0..1 is refused.
- [ ] `SuggestTool.h` no longer names a value that crosses unchecked.
