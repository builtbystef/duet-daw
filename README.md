# Duet DAW

A native desktop digital audio workstation, written in C++, built to make collaboration between a music producer and an AI easy.

The AI participant — the Collaborator — perceives the project through a closed set of read-only tools rather than through audio, and never changes anything on its own: its output is a Suggestion the producer can audition, revise, cherry-pick, accept, or reject.

## Status

Early. No product code exists yet — the project is in its specification phase, and the modules, seams, and decisions are written down before the first line of the implementation.

Built on JUCE 9 and Tracktion Engine, with CMake and Catch2. Milestone one targets the bedroom electronic producer working in the box.

## Docs

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — the modules and the seams between them
- [docs/GLOSSARY.md](docs/GLOSSARY.md) — the project's shared language
- [docs/adr/](docs/adr/) — the decisions already made, and why
- [docs/CODING_STANDARDS.md](docs/CODING_STANDARDS.md) — the conventions beyond the linter

## Contributing

Bug reports, feature requests, and discussion are welcome; outside code contributions are not accepted at this time. See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[GNU AGPL v3](LICENSE). The third-party artifacts Duet redistributes beside its own binary, and the notices they oblige it to carry, are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
