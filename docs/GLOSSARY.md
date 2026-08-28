# Glossary

The project's shared language. The rules: use one term for each concept — the rejected synonyms go under _Avoid_. A definition is one or two sentences that say what the term IS, not what it does. Only terms specific to this project belong here — general concepts from programming do not. No implementation details. Group the terms under subheadings when clusters appear.

## Language

- **Target Producer** — the bedroom electronic producer: composes in the box with MIDI and loops, records the occasional audio part, works alone. Milestone one serves this persona's workflow end to end. _Avoid: user, musician, artist._

## AI collaboration

- **Collaborator** — the AI participant in the session. It perceives the whole project through the Tool Vocabulary, never through audio, and works only when the Target Producer invokes it with a task. _Avoid: assistant, copilot, chatbot._
- **Suggestion** — a concrete set of project changes produced by the Collaborator that alters nothing until the Target Producer accepts it. The only way Collaborator output enters the project. _Avoid: proposal, draft, AI edit._
- **Element** — one human-meaningful change inside a Suggestion, carrying the edit operations that make it. It is the unit the Target Producer accepts or rejects on its own, so it is applicable on its own. _Avoid: change, item, step._
- **Edit Vocabulary** — the closed set of edit operations a Suggestion is written in, mirroring exactly what the Target Producer can do through Duet's own interface. Its edit-side counterpart to the Tool Vocabulary. _Avoid: edit API, operation set, ops._
- **Tool Vocabulary** — the closed set of read-only tools through which the Collaborator perceives the project. Its perception-side counterpart to the Edit Vocabulary. _Avoid: tools, API, context._
- **Provenance** — where a fact given to the Collaborator came from: read from the project data model, measured from rendered audio, or estimated. Carried structurally in every tool result, so a guess is never indistinguishable from a fact. _Avoid: source, confidence, certainty._
- **Estimate** — a value the Collaborator is given that a routine guessed rather than read or measured, carrying the routine that made it and how much that routine trusts itself. It crosses the seam wrapped, where a fact crosses bare. _Avoid: guess, inference, prediction._
- **Estimate Ledger** — the record of every Estimate one Task Run was handed. A run whose ledger holds anything has everything it says afterwards marked as based on estimates. _Avoid: taint list, provenance log._
- **Task Run** — one producer-initiated, non-blocking, cancelable execution of the Collaborator, from request to commentary and/or Suggestion. _Avoid: query, request, job._
- **Duet Loop** — the conversation mechanics around Suggestions: revise-on-reply, rejection-with-a-reason as input, stale-marking when the producer's edits touch a pending Suggestion, and per-element cherry-pick. _Avoid: feedback loop, chat._
- **Audition** — the transient state in which a pending Suggestion's changes are made audible in the project context for evaluation, without entering the project state or its undo history. Ends by acceptance, rejection, or stopping the audition. _Avoid: preview, ghost mode._

## Editing

- **Action** — one named producer-meaningful unit of project change and the only undo-transaction boundary. A producer gesture and an accepted Suggestion are each one Action. _Avoid: command, operation batch._
