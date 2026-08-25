#pragma once

#include <duet/model/Session.h>

/** The two real-time probes: the smallest Duet audio callback at each seam.

    ADR 0006 puts RealtimeSanitizer behind the Real-time audio standard as a
    dynamic backstop, and a backstop needs something to stand behind. Milestone
    one has no Duet-authored DSP — its instruments and effects are the engine's,
    surfaced through the Duet facade — so these two are what the nightly runs:
    one processor at each of the two callback entry points the standard names,
    doing the least a processor can do and nothing that breaks the rule.

    - The **engine-native** probe is a `tracktion::engine::Plugin`, entered at
      `applyToBuffer`, and it goes into a track's chain from here.
    - The **JUCE-hosted** probe is a `juce::AudioProcessor`, entered at
      `processBlock`. It is built as a VST3 so that the engine hosts it the only
      way it hosts one — scanned, then inserted through the vocabulary — and its
      bundle is at `DUET_REALTIME_PROBE_VST3`.

    Both entry points are `noexcept` and carry `DUET_NONBLOCKING`, and both are
    reached by an offline render, which is what lets the nightly mean something
    on a machine with no audio hardware. Neither is production code and neither
    is a step towards any: what they are for is to be entered.

    This header follows the same engine-free rule as the facades it supports.
*/
namespace duet::testing
{
/** What both probes multiply the audio they are given by: half, which is 6 dB
    down and unmistakable in a render. A probe that ran and a probe that did not
    are told apart by measuring it, which is the only way a test can watch a
    callback from outside.
*/
inline constexpr double realtimeProbeGain = 0.5;

/** The name the JUCE-hosted probe's bundle scans under. */
inline constexpr const char* juceRealtimeProbeName = "Duet Realtime Probe VST3";

/** Puts the engine-native probe at the head of a track's plugin chain.

    Test scaffolding, not a producer gesture: it reaches the Edit behind the
    facade — there being no vocabulary for a plugin type that only the tests
    know — and writes with no undo manager, so the probe never enters the
    project's history.
*/
void addNativeRealtimeProbe (duet::model::Session&, duet::model::TrackRef);
} // namespace duet::testing
