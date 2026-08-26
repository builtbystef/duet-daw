---
id: 9alksy
title: Two loose ends in js437t's Tool Vocabulary table
state: done
assignee: claude
priority: low
labels:
    - maintenance
parent: js437t
created: 2026-08-26T15:11:18Z
updated: 2026-08-26T18:02:15Z
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

## Notes

**claude** — 2026-08-26T18:02:15Z

Both loose ends decided, and js437t's Tool Vocabulary table now says what the
built tools do.

**The key crosses bare.** `get_arrangement.key` is typed `string`, not
`Estimate<string>`. This is not a new decision but the table catching up with the
rule it sits under: a declared key is read from the project model, and u24m3x's
provenance principle makes everything read from the model a bare scalar. The
spec's own prose, v5yhh1's criterion and `ProjectTools.cpp:410` already agreed;
the table was the only dissenter. The prose under the table now states the
reasoning and keeps the consequence that was already there — a project that
declares no key omits the field, and that absence is what sends the model to
`estimate_audio_content`.

**`role` is gone from the contract.** The issue offered two ways out — drop it,
or name what declares a role and put that in a milestone. Dropped, because
nothing in milestone one declares one: not the project model, not the timeline or
mixer UI (535bbo), not hll1mo's edit vocabulary. A tool cannot read a fact no
producer can state, and the alternative is inventing a per-track field for the
Collaborator's sake alone. The evidence that the field is not missed is fod077:
every run identified parts — "the drums", "the two leads" — from names,
instruments and content, unaided, and none of the 14 asked for more. The prose
records that, so a later reader sees a decision rather than an omission.

`role` also appears in u24m3x's closing note, in the sentence listing what
`list_tracks` returns. That is a closed session's record, inherited verbatim from
fod077's fixture schema, and it is left as written: js437t is the normative
contract, and rewriting a settled node's history to match would hide where the
field came from.

**No code changed.** The tools were built to both decisions at v5yhh1 — a
declared key crosses as a string, and `list_tracks` emits no `role` — so this
issue is documentation only, and there is no seam to test that the existing
ProjectTools tests do not already hold.

**Checks.** Format clean. Full `ctest`: 331 of 332 pass. The one failure,
`the transport keeps rolling while the Collaborator reads the project`, is
pre-existing at HEAD and untouched by this diff, which reaches no source file; it
is published as fnxdcx — the case reads a block-quantized playhead before and
after a tool run and requires the number to have changed, which it has not when
the run costs less than one audio block. Lint and a rebuild were no-ops: no
`.cpp` or `.h` is in the diff.
