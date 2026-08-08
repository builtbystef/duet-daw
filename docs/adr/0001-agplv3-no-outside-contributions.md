# 0001 — Ship under AGPLv3; accept no outside code contributions

**Context.** JUCE 9's open-source terms are AGPLv3, so the distributed open-source application is AGPLv3; Tracktion Engine (GPLv3-or-later) combines lawfully into an AGPLv3 work under GPLv3 §13. The project intends to keep a closed commercial edition possible, which requires the right to relicense every line of Duet's own code — a right that outside contributions destroy unless each contributor signs a CLA.

**Decision.** Duet ships under AGPLv3, network clause accepted. Outside code contributions (pull requests) are not accepted; bug reports and issues are welcome. No CLA exists because none is needed while all code is the maintainer's own.

**Reason.** Sole authorship keeps the commercial path open with zero legal machinery — nothing un-relicensable can enter the codebase, and it sidesteps Tracktion's rule that distributing third-party-contributed code requires perpetual (not subscription) engine licences. If contributions ever open, a CLA must be introduced first; that reversal is cheap in this direction and impossible in the other.
