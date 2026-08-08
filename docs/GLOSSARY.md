# Glossary

The project's shared language. The rules: use one term for each concept — the rejected synonyms go under _Avoid_. A definition is one or two sentences that say what the term IS, not what it does. Only terms specific to this project belong here — general concepts from programming do not. No implementation details. Group the terms under subheadings when clusters appear.

## Language

- **Target Producer** — the bedroom electronic producer: composes in the box with MIDI and loops, records the occasional audio part, works alone. Milestone one serves this persona's workflow end to end. _Avoid: user, musician, artist._

## AI collaboration

- **Collaborator** — the AI participant in the session. It perceives the whole project through the Tool Vocabulary, never through audio, and works only when the Target Producer invokes it with a task. _Avoid: assistant, copilot, chatbot._
- **Proposal** — a concrete set of project changes produced by the Collaborator that alters nothing until the Target Producer accepts it. The only way Collaborator output enters the project. _Avoid: suggestion, draft, AI edit._
- **Tool Vocabulary** — the closed set of read-only tools through which the Collaborator perceives the project. Its perception-side counterpart to the Proposal's edit vocabulary. _Avoid: tools, API, context._
- **Provenance** — where a fact given to the Collaborator came from: read from the project data model, measured from rendered audio, or estimated. Carried structurally in every tool result, so a guess is never indistinguishable from a fact. _Avoid: source, confidence, certainty._
- **Task Run** — one producer-initiated, non-blocking, cancelable execution of the Collaborator, from request to commentary and/or Proposal. _Avoid: query, request, job._
- **Duet Loop** — the conversation mechanics around Proposals: revise-on-reply, rejection-with-a-reason as input, stale-marking when the producer's edits touch a pending Proposal, and per-element cherry-pick. _Avoid: feedback loop, chat._
