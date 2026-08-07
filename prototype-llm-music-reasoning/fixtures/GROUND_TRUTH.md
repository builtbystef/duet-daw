# Ground truth — NEVER served to the model

DRAFT, written by Claude from the issue's suggested spread. To be corrected by
the producer against real sessions before verdicts are recorded. The point of
the experiment is judging responses as the person who already knows the answer —
so where a draft doesn't match a situation you recognise, rewrite it.

Judging buckets per response: **acted on it** / **true but useless** /
**generic advice any forum post would give** / **wrong**. Watch for generic
advice everywhere, and for invented problems on fixture-f.

## fixture-a — kick/bass masking (`warehouse_jam_v3`)

The sub bass (saw, F1/Ab1/C2) sustains legato through every kick hit; both
tracks stack their energy in 20–150 Hz (bass: sub −7.9 / bass −10.8; kick: sub
−10.4 / bass −13.2), there is no sidechain and no EQ carve anywhere in the bass
chain, and the master's low bands (−7.2 / −9.0) dominate its mids by ~8 dB.
Accepted fix, either or both: (1) sidechain compressor on Sub Bass keyed from
Kick (≈4:1, threshold ≈ −28 dB, fast attack, release 60–90 ms, 4–6 dB GR);
(2) shorten/retrigger the bass notes so downbeats leave a gap for the kick
transient. Bonus credit: noticing the F1 fundamental (~43 Hz) sits below the
kick's ~50–60 Hz center and proposing a small EQ dip in one of them rather
than "cut lows" generically.

## fixture-b — static loop, arrangement needed (`rolling_idea_7`)

Nothing is wrong with the mix. Every track is one looped clip spanning all 64
bars, all patterns identical on every repeat, hats at constant velocity 96,
zero automation, a single 64-bar section. The track has no arrangement.
Accepted fix: concrete arrangement moves — e.g. strip to kick/bass/hats for an
8–16 bar intro, pull the kick for a break, delay the arp's entrance, add a
filter or send automation ramp, vary the last bar of 8 (fill / dropped hit).
Must be specific clip/section operations, not "add variation".

## fixture-c — progression wants a turnaround (`night_drive_idea2`)

Am7 → Fmaj7 → Cmaj7 → G, two bars each, looping with no lead-back into Am —
bar 8 sits on G and the loop restarts cold. Accepted fix: put a turnaround in
bar 8 (or its second half): E7 (E–G♯–B–D) as V of Am, or G♯dim passing chord,
with the bass moving to E; melody optionally touching G♯. Any dominant-function
device that pulls back to Am counts. Wrong: re-voicing advice that ignores the
functional gap, or "add a 2-5-1" without placing it.

## fixture-d — over-compressed drum bus (`crush_test_2`)

The Drum Bus group chain is OTT at 100% depth into a 20:1 compressor, 0.1 ms
attack, −35 dB threshold, +12 makeup, averaging ~12.8 dB GR: bus crest factor
3.6 dB while the individual drum tracks measure 10–11.5 dB before the group.
Accepted fix, on the group chain: ratio to ~4:1, threshold up to ≈ −18 dB,
attack 15–30 ms so transients pass, OTT depth to ~15–25%, makeup down to match
— target 3–5 dB GR and bus crest back toward 9–10 dB. The tell distinguishing
real understanding: citing the before/after crest factors, not just "less
compression".

## fixture-e — busy drop, something should be cut (`everything_louder_v5`)

Drop 1 has 12 tracks active. Lead 2's pattern is note-for-note Lead 1
transposed +12 with identical rhythm and both are full-width supersaws; the pad
sustains whole-note chords under everything at −6.5 dB fader with mids at −13;
master mid band (−8.8) is the loudest band and master crest is 5.4 dB.
Accepted fix: cut, don't EQ — e.g. remove Lead 2 from drop 1 (save the octave
double for drop 2), shorten or remove the pad during drops, thin vox chops to
fills. The money finding is discovering the Lead 2 ≡ Lead 1 + 12 duplication
by actually diffing the MIDI.

## fixture-f — control: nothing is wrong (`sunset_loop_final`)

Deliberately healthy: varied arrangement with real sections, bass sidechained
sensibly (3.2 dB GR at 4:1), pads/keys high-passed, humanized hat velocities,
filter automation in the break, master at −12.6 LUFS with 9.8 dB crest and a
balanced spectrum. Pass: the model says it's solid, at most taste-level
optional suggestions clearly framed as such. Fail: inventing problems to look
useful (any confident "your mix is muddy/over-compressed/cluttered" claim).

## fixture-g — buried hook (`callsign_v9`)

The chorus hook (Lead) sits at fader −9.5 dB, RMS −20.6, while two stacked pads
run at 0 / −1.5 dB faders with RMS −10.8 / −11.5 and dominate exactly the
lead's 400–2k/2k–6k bands; pads have no EQ carve and 0.75–0.85 stereo width;
master mids are the loudest band. The hook is ~9 dB under the pads in its own
register. Accepted fix: raise Lead ~+6–8 dB and/or carve the pads (bell cut
≈1–3 kHz, a few dB) or duck pads under the lead; narrowing/HP'ing the pad stack
also counts. Wrong: anything that treats the low end or drums as the problem.
