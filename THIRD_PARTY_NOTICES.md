# Third-party notices

Duet itself is licensed under the [GNU AGPL v3](LICENSE). This file is about the
third-party artifacts Duet **redistributes as files of their own**, beside its
own binary, and the notices their licences oblige it to carry.

Duet's other dependencies — JUCE, Tracktion Engine, nlohmann/json, Catch2, pi —
are fetched at build time from their own repositories and are not redistributed
from here; each carries its own licence file where the build put it.

The copies here travel with the installation: the build puts this file, the
Apache licence text under `licences/`, the model and the runtime in the same
directory as the application.

---

## Basic Pitch model weights — Apache License 2.0

Duet's polyphonic transcription reads Spotify's Basic Pitch model, as serialised
to ONNX at tag `v0.4.0` (`basic_pitch/saved_models/icassp_2022/nmp.onnx`). The
build fetches that file by URL and by SHA-256, ships it beside the application as
`nmp.onnx`, and does not alter, quantize, convert or re-serialise it — so no
changed-file notice under Apache-2.0 §4(b) applies to it.

The licence text is in [`licences/Apache-2.0.txt`](licences/Apache-2.0.txt). The
upstream NOTICE, reproduced as Apache-2.0 §4(d) requires, follows. Only the
entries that apply to what Duet ships are kept: the model file itself, and
Spotify's own attribution.

```
Basic Pitch
Copyright 2022 Spotify AB

This product includes software developed at
Spotify AB (http://www.spotify.com/).
```

Spotify's name and marks are used here for factual attribution only, which is
all Apache-2.0 §6 grants. Nothing here is an endorsement by Spotify AB.

The licence finding this rests on — that the weights are Apache-2.0, that
redistribution inside an AGPLv3 application is permitted, and that a future
closed edition could ship them — was established from primary sources in issue
`lxt41c`, which cites each claim. It is a licensing finding, not legal advice.

---

## ONNX Runtime — MIT License

Duet runs the Basic Pitch model on Microsoft's ONNX Runtime. The build fetches
the prebuilt Linux x64 release by URL and by SHA-256 and ships
`libonnxruntime.so.1` beside the application, unmodified.

```
MIT License

Copyright (c) Microsoft Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

ONNX Runtime carries its own `ThirdPartyNotices.txt` for the components inside
it; the build leaves that file where the release archive puts it.

---

Both of the above are behind one build option,
`DUET_ENABLE_POLYPHONIC_TRANSCRIPTION`. A Duet configured with it off ships
neither file, and this document is then about nothing that is there.
