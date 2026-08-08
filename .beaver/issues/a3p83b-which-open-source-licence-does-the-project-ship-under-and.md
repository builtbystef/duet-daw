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
updated: 2026-08-08T01:28:45Z
---

Grill session, made sharp by node 1hn16k: choosing JUCE means the JUCE modules are used under AGPLv3, so the distributed open-source application is AGPLv3. That fixes part of the answer and forces the rest.

Settle, limited to:

- Whether the project ships under AGPLv3 (the licence JUCE's open-source arm imposes on the distributed binary), and whether the user accepts AGPL's network clause — it also reaches any future server-side component that links JUCE-derived code.
- Whether contributions require a CLA. Without one, contributed code cannot be relicensed for a closed commercial edition even after a JUCE commercial licence is bought — buying the JUCE licence does not un-AGPL other people's contributions. This is the decision that is expensive to reverse late.
- What the commercial edition would be, roughly, and which JUCE tier it lands in: Starter free up to $20k revenue, Indie $800 perpetual up to $300k, Pro $3,500 perpetual uncapped (node 1hn16k's note has the citations). Enough shape to know which threshold matters, not a business plan.

Deliverable: the licence choice, the CLA decision, and the reason — ready to become an ADR and a LICENSE file.

## Notes

**claude** — 2026-08-08T01:28:45Z

Materially changed by node lf8tnt (2026-08-07), which adopted Tracktion Engine. Read that node's note before starting; the licence arithmetic in this issue's body is now incomplete.

THE OPEN-SOURCE SIDE IS UNAFFECTED AND FREE. Tracktion Engine is dual-licensed 'GPL3 (or later)/Commercial' (LICENSE.md, master). GPLv3 §13 expressly permits combining GPLv3 code into an AGPLv3 work — 'you have permission to link or combine any covered work with a work licensed under version 3 of the GNU Affero General Public License into a single combined work, and to convey the resulting work' (https://www.gnu.org/licenses/gpl-3.0.txt). So an AGPLv3 Duet containing GPLv3 Tracktion Engine is lawful, costs nothing, and has no revenue limit. The AGPLv3 question in this issue's body stands exactly as written.

THE COMMERCIAL SIDE NOW COSTS TWO LICENCES, AND ONE OF THEM IS RENT. 'Although Tracktion Engine utilises JUCE, it is not part of JUCE nor owned by the same company. As such it is licensed separately… Similarly, Tracktion Engine is not included in a JUCE licence' (README.md). So a closed edition needs a JUCE licence AND a Tracktion Engine licence. The structural difference between them is the thing to weigh here:

- JUCE commercial is a perpetual purchase for a version (Indie $800 perpetual up to $300k revenue).
- Tracktion Engine commercial is a per-seat SUBSCRIPTION with no published perpetual SKU: Personal free (up to $50k), Indie $35/mo (up to $200k, 12-month commitment), Pro 1 $50/mo (up to $400k), Pro 2 $150/mo, Pro 3 $300/mo, Enterprise on request (https://engine.tracktion.com/, 2026-08-07). It must be maintained for the whole distribution period — 'You will need to maintain a licence for at least the duration over which you are distributing closed-source binaries containing Tracktion Engine' — and on termination 'you must cease all activities authorized by this Agreement, including distribution of your Application that incorporates the Code' (EULA cl. 8.5). Stopping payment ends the right to keep distributing a binary already shipped.

TWO TRAPS FOR THIS SESSION TO DECIDE ABOUT:

1. The revenue limit is ORGANISATION-WIDE GROSS REVENUE INCLUDING FUNDING, not product revenue: 'Minimum Commitments are based on gross revenue, meaning all monies collected, including through funding, whether it be received in connection with your Tracktion Engine Applications or not, without offsets of any kind' (EULA), and the FAQ says the limit 'is that of the whole organisation or individual that owns the Tracktion Engine licence'. Whether that reaches an individual licensee's unrelated income (salary) is UNRESOLVED and worth settling before any paid tier is relied on. It does not bite on the GPL route.
2. Exceeding a limit forces one of three outcomes, one being to 'license your Applications under the GNU General Public License' (EULA cl. 1.x). Back-fees plus a floor of 'no less than $1,000 USD' and an on-site audit right apply (cl. 3.5, 10.4); all fees are non-refundable (cl. 3.3).

ALSO UNRESOLVED, AND IT CHANGES THE SHAPE OF THE ANSWER: whether perpetual Tracktion Engine licences exist. The FAQ implies they do — it says a distributor of third-party-contributed code 'is legally obliged to ensure that the corresponding licences are perpetal, rather than subscription, Tracktion Engine licences' — but no perpetual SKU appears on the pricing page and the EULA defines only subscription and free tiers. Requires contacting Tracktion sales. Note the phrasing ties perpetual licences to THIRD-PARTY CONTRIBUTIONS, which is exactly the CLA question this issue exists to settle — so the two are entangled and should be answered together.

THE CLA QUESTION GAINS A SECOND EDGE. Tracktion does not accept third-party contributions to its own engine ('We don't accept third party GitHub pull requests directly due to copyright restrictions', README), which is a precedent for how a dual-licensed project handles this, and their FAQ language above suggests accepting outside contributions changes the licence type they require. Worth reading before deciding Duet's own contribution policy.
