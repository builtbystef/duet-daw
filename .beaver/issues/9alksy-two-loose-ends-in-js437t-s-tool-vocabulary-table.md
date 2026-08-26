---
id: 9alksy
title: Two loose ends in js437t's Tool Vocabulary table
state: todo
priority: low
labels:
    - maintenance
parent: js437t
created: 2026-08-26T15:11:18Z
updated: 2026-08-26T15:11:18Z
---

## What is wrong

Building the project-read tools (v5yhh1) turned up two places where spec js437t's
contract table says something the rest of the spec does not.

**`get_arrangement.key` is typed `Estimate<string>`.** Everywhere else says it is
a fact: the spec's own prose has it "present only when the project declares a
key", v5yhh1's criterion says "a project that declares a key reports it as a bare
value", and the provenance rule says everything read from the project model
crosses bare. The table is the odd one out, and the tool is built to the rule: a
declared key crosses as a string, and a project that declares none omits the
field, which is what sends the model to `estimate_audio_content`.

**`list_tracks[].role` has no source.** Nothing in milestone one — not the model,
not the UI (535bbo), not the edit vocabulary — gives a track a role, so a producer
has no way to say one and the tool has nothing to read. It is optional in the
table, and the tool omits it.

## What to do

Decide each, and make the table say it. For the key, the likely answer is deleting
the wrapper from the table. For role, either drop it from the contract or say what
declares a role and add that to a milestone — a Suggestion that talks about "the
drums" is reading names and instruments today, and doing well enough at it that
fod077's runs never asked for more.

Not urgent: both are already settled in the code and recorded here. This is the
spec catching up with what was built.

## Acceptance criteria

- [ ] js437t's table types `key` the way the provenance rule does.
- [ ] `role` is either gone from the contract or has a stated source.
