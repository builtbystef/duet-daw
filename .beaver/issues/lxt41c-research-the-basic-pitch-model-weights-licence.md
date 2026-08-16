---
id: lxt41c
title: 'Research: the Basic Pitch model weights'' licence'
state: done
assignee: agent
priority: high
labels:
    - research
parent: js437t
created: 2026-08-12T04:01:02Z
updated: 2026-08-12T04:49:44Z
---

## What to build

The analysis layer's one ML dependency ships model weights, and a repository's stated licence does not automatically cover the weights it distributes. Establish from primary sources what licence the weights carry, whether redistributing them inside the AGPLv3 application is permitted, whether a future closed commercial edition could ship them, and what attribution or notice obligations come with them.

If the answer forecloses either path, name at least one alternative — a differently licensed model, or shipping without polyphonic transcription — with its own licence checked. The conclusion is recorded here and must be enough to unblock or cancel the transcription slice without re-reading the sources.

## Acceptance criteria

- [ ] The weights' licence is established from primary sources — the model repository's own licence files, model card, and any accompanying terms — with each claim cited to its source.
- [ ] The finding states plainly whether redistribution inside an AGPLv3 application is permitted.
- [ ] The finding states plainly whether a future closed commercial edition could ship the same weights, per the project's licensing posture.
- [ ] Attribution or notice obligations, if any, are listed concretely enough to implement.
- [ ] If either path is foreclosed or unclear, at least one alternative is named with its licence checked to the same standard.
- [ ] The conclusion is recorded on this issue, and the transcription slice can be unblocked or cancelled from it alone.

## Notes

**agent** — 2026-08-12T04:49:44Z

## Question

Establish the licence for Spotify Basic Pitch's distributed pretrained model
weights, whether Duet may redistribute them in its AGPLv3 edition and in a
future closed commercial edition, and the implementable notice obligations.

## Answer

**The transcription slice is unblocked.** Spotify's first-party Basic Pitch
model card identifies the model as Apache-2.0, and Basic Pitch v0.4.0 ships its
pretrained TensorFlow, CoreML, TensorFlow Lite, and ONNX serializations in the
Apache-2.0 distribution. Apache-2.0 permits both redistribution within Duet's
AGPLv3 edition and inclusion in a future closed commercial edition. The former
must also satisfy Duet's AGPLv3 obligations; the latter does not trigger an
Apache source-publication obligation.

Ship the Apache-2.0 licence and applicable Basic Pitch NOTICE in Duet's
third-party notices, retain applicable notices, and mark any altered or
re-serialized model file as modified. Do not use Spotify branding beyond
customary factual attribution. This is a licensing finding, not legal advice.

## Findings

1. **The weights are Apache-2.0.** Spotify's own [Basic Pitch model card](https://huggingface.co/spotify/basic-pitch/blob/main/README.md) declares `license: apache-2.0` (accessed 2026-08-12). The card is served from Spotify's verified publisher namespace and describes Basic Pitch. This is the model-specific licence statement.

2. **The tagged package contains the model serializations.** Spotify's [Basic Pitch v0.4.0 README](https://github.com/spotify/basic-pitch/blob/9991303bba609a3b93089d13ec80d1d495083596/README.md#model-runtime), commit `9991303bba609a3b93089d13ec80d1d495083596` (2024-08-16), states that Basic Pitch ships the original TensorFlow model and CoreML, TensorFlow Lite, and ONNX conversions. Its [saved-model directory](https://github.com/spotify/basic-pitch/tree/9991303bba609a3b93089d13ec80d1d495083596/basic_pitch/saved_models/icassp_2022) contains those artifacts, including weight files. The same tag's [LICENSE](https://github.com/spotify/basic-pitch/blob/9991303bba609a3b93089d13ec80d1d495083596/LICENSE) is Apache License 2.0, copyright Spotify AB. The official [PyPI 0.4.0 wheel](https://pypi.org/project/basic-pitch/0.4.0/) (uploaded 2024-08-16) likewise distributes the model serializations with Apache LICENSE and NOTICE. No separate restrictive terms or model-card file were found in the complete tagged repository tree or release assets.

3. **AGPLv3 redistribution is permitted.** Apache-2.0 §2 grants reproduction, derivative-work, sublicensing, and distribution rights; §4 states the redistribution conditions. [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0) is GPLv3-compatible according to the [FSF licence list](https://www.gnu.org/licenses/license-list.html#apache2); the [FSF compatibility FAQ](https://www.gnu.org/licenses/gpl-faq.en.html#v2v3Compatibility) says GPLv3 compatibility results also apply to AGPLv3. Therefore Duet may convey the weights with its AGPLv3 application while satisfying Apache §4.

   The legal packaging classification does not change that conclusion: if the files are separate, independent works merely collected with Duet, AGPLv3's aggregate paragraph says AGPL does not extend to them; if the integration is legally one combined work, [AGPLv3 §5(c)](https://www.gnu.org/licenses/agpl-3.0.html) requires that combined work to be licensed as a whole under AGPLv3, alongside Apache's notice conditions. This report does not decide that fact-specific classification.

4. **A closed commercial edition may ship the same weights.** Apache-2.0 §§2 and 4 allow distribution and sublicensing in source or object form, and expressly permit additional or different terms for modifications and derivatives provided Apache compliance is retained. It contains no copyleft or source-disclosure condition for unmodified object-form weight files. A proprietary host product may therefore include them, subject to the conditions below. The licence's patent grant is limited to necessary contributor claims and terminates for this Work if the licensee initiates patent litigation alleging the Work or a Contribution infringes a patent (Apache-2.0 §3).

5. **Implementation obligations.** For every shipped Basic Pitch model package:
   - Include a copy of Apache License 2.0 for recipients (Apache-2.0 §4(a)).
   - Include a readable copy of applicable upstream attribution from [Basic Pitch's NOTICE](https://raw.githubusercontent.com/spotify/basic-pitch/main/NOTICE), preferably in Duet's bundled `THIRD_PARTY_NOTICES` / licences screen (Apache-2.0 §4(d)). Preserve Spotify AB and Basic Pitch attribution. Retain other NOTICE entries only where the corresponding component is included.
   - If Duet alters, quantizes, converts, or re-serializes a model artifact, add a prominent changed-file notice (Apache-2.0 §4(b)).
   - If distributing source-form Basic Pitch derivatives, retain applicable copyright, patent, trademark, and attribution notices (Apache-2.0 §4(c)).
   - Do not present Spotify's name, marks, or product name as endorsement; Apache-2.0 §6 grants no trademark rights other than customary factual origin attribution and NOTICE reproduction.
   - Academic citation in the model card is a recommendation, not an Apache redistribution condition.

## Unresolved

- Licence text cannot determine whether Duet's eventual packaging is an AGPL aggregate or a combined/derivative work. Build the distribution so the weights remain separately packaged where feasible; obtain legal advice if the classification matters beyond satisfying the stricter combined-work case.
- The primary sources give no standalone explanation of ownership of training data or a separate per-blob weight grant. The Spotify model card's explicit Apache-2.0 model designation and the upstream package distribution are sufficient to license the shipped artifacts, but do not make claims about unseen training-data rights.

## Sources

- [Spotify Basic Pitch model card](https://huggingface.co/spotify/basic-pitch/blob/main/README.md), accessed 2026-08-12.
- [Spotify Basic Pitch v0.4.0 README](https://github.com/spotify/basic-pitch/blob/9991303bba609a3b93089d13ec80d1d495083596/README.md#model-runtime), tag commit `9991303bba609a3b93089d13ec80d1d495083596`, 2024-08-16.
- [Spotify Basic Pitch v0.4.0 LICENSE](https://github.com/spotify/basic-pitch/blob/9991303bba609a3b93089d13ec80d1d495083596/LICENSE), Apache License 2.0, January 2004.
- [Spotify Basic Pitch model artifacts at v0.4.0](https://github.com/spotify/basic-pitch/tree/9991303bba609a3b93089d13ec80d1d495083596/basic_pitch/saved_models/icassp_2022).
- [Official PyPI Basic Pitch 0.4.0 distribution record](https://pypi.org/project/basic-pitch/0.4.0/), 2024-08-16.
- [Spotify Basic Pitch NOTICE](https://raw.githubusercontent.com/spotify/basic-pitch/main/NOTICE), accessed 2026-08-12.
- [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0), January 2004.
- [FSF licence list: Apache License 2.0](https://www.gnu.org/licenses/license-list.html#apache2), accessed 2026-08-12.
- [FSF GPL FAQ: GPLv3/AGPLv3 compatibility matrix](https://www.gnu.org/licenses/gpl-faq.en.html#v2v3Compatibility), accessed 2026-08-12.
- [GNU Affero General Public License v3](https://www.gnu.org/licenses/agpl-3.0.html), 19 November 2007.
