---
id: wyfdjb
title: TSan reports on the engine's plugin scanner and on a hosted VST3's MessageManagerLock
state: todo
priority: low
parent: b1j3me
created: 2026-08-26T15:16:59Z
updated: 2026-08-26T15:16:59Z
---

## What was measured

Under `linux-tsan` on the dev machine (2026-08-26, `setarch $(uname -m) -R`,
`TSAN_OPTIONS=detect_deadlocks=0`), the existing case "a scanned VST3 inserts
through the vocabulary, processes audio, and undoes a parameter exactly" reports
34 ThreadSanitizer warnings on its own. They are of two kinds:

- **Data races inside the engine's out-of-process scanner.** Both stacks bottom
  out in `PluginScanHelpers::PluginScanMasterProcess::findReply` and its
  `OwnedArray<juce::XmlElement>` of pending replies — the main thread reads the
  array while the scan's own thread appends to it, with no lock between them.
- **`destroy of a locked mutex`** from `juce::MessageManagerLock`'s destructor,
  reached through `VST3PluginInstanceHeadless::VST3Parameter::getText` and from
  the fixture plugin's own copy of JUCE at teardown.

Neither is Duet's memory and neither is reached from Duet's own threads. ADR 0006
trusts Tracktion Engine outright and puts only Duet's code under test, so this is
a record rather than a defect to fix here.

## Why it is worth recording

`v5yhh1` added a `[collab]` case that hosts a scanned VST3, so `[collab]` under
TSan is no longer clean: 21 warnings, all of these two kinds. Without the same
case it is 0 warnings over 36 cases, which is what says the Collaborator's own
threading — the service thread, and the marshal that puts a project read on the
message thread — is clean. Anyone reading a TSan run of this suite needs to know
which reports are the engine's before they go looking for their own.

## What to do

- [ ] Establish whether the nightly TSan job is already red on these, and say so
      in `AGENTS.md` beside the vptr note, which is where the suite's known TSan
      reports are recorded.
- [ ] Decide whether an exact upstream-function suppression is warranted for the
      scanner and the `MessageManagerLock` teardown, under the rule the coding
      standards already state: an exact upstream function, a documented false
      positive, and nothing broad enough to hide a violation reached from Duet.
