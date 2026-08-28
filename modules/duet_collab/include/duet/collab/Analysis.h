#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/** The analysis layer's routines: what a rendered waveform is, and every
    routine that reads one (spec js437t, tiers 2 and 3).

    Each routine is a pure function of a waveform, which is what makes ground
    truth here true by construction: a test states a signal it built itself and
    the value that signal has by definition. Nothing in this file knows about
    the project, the socket, a track, or a thread — it links no engine, no JUCE
    and no JSON, so nothing it does can share a lock with the audio callback.

    Every value a routine answers with is a bare number, because a documented
    routine over real audio is a measurement and not a guess (ADR 0002). What
    the routine is, and the tolerance it holds to, is what each declaration
    below says.

    The last section is the exception that proves the rule. The harmony
    routines read notes out of a waveform rather than measuring a property of
    it, so each of them answers with a confidence beside its answer, and what
    they answer crosses the seam wrapped as an estimate and never bare.
*/
namespace duet::collab::analysis
{
/** The level at which a measurement gives up on saying how quiet something is.
    The same floor the model's meters use, so silence reads the same number
    wherever it is measured.
*/
inline constexpr double silenceDb = -100.0;

/** One stretch of rendered audio, in memory: the samples of each channel, and
    the rate they are to be read at.

    A waveform of no channels, or of a rate that is not positive, is nothing to
    measure, and every routine answers such a one with its own floor.
*/
struct Waveform
{
    double sampleRate = 0.0;
    std::vector<std::vector<float>> channels;

    /** Samples in one channel. Channels are always the same length here: a
        render writes them together.
    */
    [[nodiscard]] std::size_t length() const;
    [[nodiscard]] double lengthSeconds() const;
    [[nodiscard]] bool empty() const;

    /** The stretch between two times, in seconds from this waveform's start,
        clipped to what it holds. What a bar range is measured over.
    */
    [[nodiscard]] Waveform between (double fromSeconds, double toSeconds) const;
};

//==============================================================================
// Level.

/** The loudest sample of any channel, in decibels of full scale.

    Sample peak, not inter-sample peak: what the file holds. A full-scale sine
    measures 0 dBFS to within a hundredth of a decibel, the sampling itself
    being what keeps it from landing on the crest exactly.
*/
[[nodiscard]] double peakDb (const Waveform& waveform);

/** The loudest the waveform gets between its samples as well as on them, in
    decibels relative to full scale: the true peak of ITU-R BS.1770-4.

    A sampled waveform passes above its own samples — a sine whose crest falls
    between two of them is the plainest case — and a converter plays what is
    between them, so this reconstructs the signal at four times the rate it was
    written at and takes the loudest point of that. Never below the sample
    peak, and within a few tenths of a decibel of the crest a reconstruction
    would actually reach.
*/
[[nodiscard]] double truePeakDbtp (const Waveform& waveform);

/** The root mean square over every channel, in decibels of full scale. A
    full-scale sine measures −3.01 dB, which is what its crest factor is made
    of.
*/
[[nodiscard]] double rmsDb (const Waveform& waveform);

/** How far the peak stands above the RMS, in decibels: 3.01 for a sine, 0 for
    a square wave, and larger the more of a waveform is transient. Never
    negative, and zero for silence.
*/
[[nodiscard]] double crestFactorDb (const Waveform& waveform);

//==============================================================================
// Spectrum.

/** One of the fixed named bands the spectrum is reported in: what it is called,
    and where it begins and ends.
*/
struct SpectralBand
{
    std::string_view name;
    double fromHz;
    double toHz;
};

/** The seven bands, low to high, and their edges.

    Fixed and named rather than chosen per call, because what the Collaborator
    is told a band is has to mean the same thing in every project. These edges
    are also the ones `get_track_analysis` tells the model in its own
    description — the two are held together by a test, since a band whose edges
    the model has wrong is worse than no band at all.
*/
inline constexpr std::array<SpectralBand, 7> spectralBands { {
    { "sub", 20.0, 60.0 },
    { "low", 60.0, 250.0 },
    { "low-mid", 250.0, 500.0 },
    { "mid", 500.0, 2000.0 },
    { "high-mid", 2000.0, 4000.0 },
    { "high", 4000.0, 10000.0 },
    { "air", 10000.0, 20000.0 },
} };

/** How much energy each band holds, in decibels of full scale, in the order the
    table above is written.

    A level and not a share: a full-scale sine sitting inside one band puts
    −3.01 dB in that band, which is what the whole waveform measures by RMS, and
    at least 40 dB less in every other.
*/
[[nodiscard]] std::vector<double> spectralBandEnergiesDb (const Waveform& waveform);

/** Where the weight of the spectrum sits, in hertz: the magnitude-weighted mean
    frequency, which is the plainest number there is for how bright something
    sounds. Zero for silence.
*/
[[nodiscard]] double spectralCentroidHz (const Waveform& waveform);

/** How evenly the energy is spread across the spectrum, from 0 to 1: near zero
    for a single tone, near one for noise. The geometric mean of the spectrum
    over its arithmetic mean, which is what says tonal from noisy.
*/
[[nodiscard]] double spectralFlatness (const Waveform& waveform);

/** Where the waveform starts something, in seconds from its own start, in
    order.

    Spectral flux and not a level crossing: what marks an onset is the spectrum
    changing, so a note struck over a note already sounding is one of these and
    a fade that grows louder is not. Each one is then placed to within a few
    milliseconds by looking at where the sound itself rose, because an onset
    that reads late is the one thing this measurement may not do.
*/
[[nodiscard]] std::vector<double> onsetsSeconds (const Waveform& waveform);

//==============================================================================
// Stereo.

/** How alike the two channels are: 1.0 when they carry the same signal, −1.0
    when one is the other turned upside down, and around zero when they have
    little to do with each other.

    A waveform of one channel is perfectly correlated with itself and answers
    1.0. Silence has nothing to compare and answers 0.0.
*/
[[nodiscard]] double stereoCorrelation (const Waveform& waveform);

/** How much of the waveform is in what the channels differ by rather than in
    what they share: 0.0 for a signal that is the same in both, rising towards
    1.0 as more of it is difference.

    Mid and side, in other words, and the share of the energy that is side. A
    signal summed to mono has no side at all and measures zero.
*/
[[nodiscard]] double stereoWidth (const Waveform& waveform);

//==============================================================================
// Pitch.

/** The pitch of the waveform in hertz by the YIN difference function, and
    nothing at all when there is no one pitch to name.

    Monophonic: it answers about a waveform carrying a single line, and a chord
    or a mix is not something it is asked. A steady tone measures to within a
    cent — a hundredth of a semitone — and silence answers nothing rather than
    the wrong number, because a routine that cannot tell is worth less than one
    that says so.
*/
[[nodiscard]] std::optional<double> pitchHz (const Waveform& waveform);

//==============================================================================
// Loudness, per ITU-R BS.1770-4.

/** The absolute gate of BS.1770, and so the quietest loudness that can be
    stated: a waveform with nothing above it has no loudness to integrate, and
    answers with this.
*/
inline constexpr double inaudibleLoudness = -70.0;

/** Integrated loudness in LUFS: the K-weighted mean energy of the 400 ms
    blocks that pass BS.1770's two gates — the absolute one at −70 LKFS and the
    relative one 10 LU below what the rest of them measure.

    The EBU Tech 3341 test signals measure their published values to within
    0.1 LU, which is the tolerance the standard itself states.
*/
[[nodiscard]] double lufsIntegrated (const Waveform& waveform);

/** The loudest three seconds of the waveform, in LUFS: BS.1770's short-term
    loudness, taken over a sliding 3 s window stepped ten times a second, and
    answered with the largest reading it made. A waveform shorter than the
    window has no short-term loudness and answers the floor.
*/
[[nodiscard]] double lufsShortTermMax (const Waveform& waveform);

//==============================================================================
// Harmony, estimated.

/** What one of the harmony routines made of a waveform: what it would call what
    it heard, and how well what it heard fits that name, from 0 to 1.

    A name and a confidence together, because these are the tier-3 routines
    (spec js437t): a key is a reading of the notes and not a property of the
    waveform, so what comes out of them is a guess with its strength attached
    and crosses the seam wrapped (ADR 0002).

    The name is empty, and the confidence zero, when the waveform gives nothing
    to read: silence, or less signal than a transform needs. A routine that
    cannot tell says so rather than naming the key noise fits least badly.
*/
struct Estimated
{
    std::string name;
    double confidence = 0.0;
};

/** How the two routines below describe themselves, which is what the `method`
    of a wrapped value says: an estimate names the routine that made it, so that
    a producer inspecting the mark reads what was done rather than that
    something was.
*/
inline constexpr std::string_view keyMethod =
    "pitch-class profile scored against the Krumhansl-Schmuckler key profiles";
inline constexpr std::string_view chordMethod =
    "pitch-class profile matched against major and minor triad templates";

/** Which of the twenty-four keys the waveform's pitch classes fit best, named
    as the producer writes it — "C major", "A minor".

    Krumhansl-Schmuckler: how much of each pitch class the waveform holds is
    correlated with each key's published profile, and the best correlation is
    both the key and the confidence. A progression that belongs to one key
    scores far above noise, which holds every pitch class and therefore fits
    every key badly — which is what the confidence is for.

    Voicing and octave say nothing here: what is read is pitch classes, so a
    chord played high and a chord played low are the same evidence.
*/
[[nodiscard]] Estimated estimatedKey (const Waveform& waveform);

/** Which triad the waveform's pitch classes fit best, named the same way —
    "C major", "A minor".

    Major and minor triads and no other chord: what the routine can name is what
    it has templates for, so a seventh reads as the triad inside it and the
    confidence says how much was left over. One chord for the whole waveform,
    which is why the tool that uses this hands it one bar at a time.
*/
[[nodiscard]] Estimated estimatedChord (const Waveform& waveform);
} // namespace duet::collab::analysis
