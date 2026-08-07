---
id: a3p83b
title: Which open-source licence does the project ship under, and does it need a CLA?
state: todo
priority: high
labels:
    - roadmap:d9gioe
    - session:grill
depends_on:
    - 1hn16k
parent: d9gioe
created: 2026-08-07T06:43:22Z
updated: 2026-08-07T06:43:22Z
---

Grill session, made sharp by node 1hn16k: choosing JUCE means the JUCE modules are used under AGPLv3, so the distributed open-source application is AGPLv3. That fixes part of the answer and forces the rest.

Settle, limited to:

- Whether the project ships under AGPLv3 (the licence JUCE's open-source arm imposes on the distributed binary), and whether the user accepts AGPL's network clause — it also reaches any future server-side component that links JUCE-derived code.
- Whether contributions require a CLA. Without one, contributed code cannot be relicensed for a closed commercial edition even after a JUCE commercial licence is bought — buying the JUCE licence does not un-AGPL other people's contributions. This is the decision that is expensive to reverse late.
- What the commercial edition would be, roughly, and which JUCE tier it lands in: Starter free up to $20k revenue, Indie $800 perpetual up to $300k, Pro $3,500 perpetual uncapped (node 1hn16k's note has the citations). Enough shape to know which threshold matters, not a business plan.

Deliverable: the licence choice, the CLA decision, and the reason — ready to become an ADR and a LICENSE file.
