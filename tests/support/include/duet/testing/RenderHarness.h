#pragma once

#include <duet/model/Session.h>

#include <cstddef>
#include <filesystem>
#include <vector>

/** The audio-correctness harness: offline renders, and what a test may ask of
    one.

    ADR 0006 decides what an audio test is allowed to say. A render is asserted
    on its measured features — pitch, where a sound starts, how the level and the
    spectrum change — each with a tolerance the domain gives it, and never
    against a golden file, a stored fingerprint, or samples kept from an earlier
    run. Every reference signal a test renders is synthetic, so what the render
    should hold is true by construction.

    The one sample comparison the ADR allows is the determinism canary:
    `isBitIdenticalTo` between two renders made in the same process, which is the
    only place bit-exactness is a fact rather than a coincidence of this
    machine.

    The measurements here are the test's own, deliberately simple, and they are
    not the analysis layer: the Collaborator's measured analysis (issue 3bgymu)
    is production DSP with its own conformance tests, and nothing in this file is
    a step towards it.

    This header follows the same engine-free rule as the facades it supports.
*/
namespace duet::testing
{
/** The shape the model gives every offline render, restated here because a
    measurement needs it: the rate a sample position is in, and the block the
    engine cuts the timeline into.
*/
inline constexpr double renderSampleRate = 44100.0;
inline constexpr int renderBlockSize = 512;

/** How early a rendered sound may start: the engine puts a note at the
    beginning of the block that contains it, so an onset assertion allows one
    render block early and nothing at all late (ADR 0006).
*/
inline constexpr double oneRenderBlockSeconds = renderBlockSize / renderSampleRate;

/** Below this a stretch of a render carries no signal — the same −100 dB the
    model's meters call silence.
*/
inline constexpr double silenceLevel = 1.0e-5;

class Render;

/** Renders the whole project offline and reads the result back.

    The render runs on a worker thread — where the thread model puts an offline
    render — while the message loop runs here, which is what the engine needs to
    build the render graph. The destination is a file of this folder that no
    render has used before: the engine answers a repeated render out of its
    audio-file cache, keyed on the destination, so a render that must be a render
    asks for a name of its own.
*/
[[nodiscard]] Render renderProject (duet::model::Session& session,
                                    const std::filesystem::path& folder);

/** The same for one track of the project on its own. */
[[nodiscard]] Render renderTrack (duet::model::Session& session,
                                  duet::model::TrackRef track,
                                  const std::filesystem::path& folder);

/** Exports the project the way the interface does — on a worker thread, with
    the message loop running here — and reads back whatever it wrote.

    An export is the producer's own render, so this takes the options rather
    than making them, and it takes the destination from them: a Render over a
    file that was never written answers `readable` false, which is what a
    cancelled export leaves behind.
*/
[[nodiscard]] Render exportProject (duet::model::Session& session,
                                    const duet::model::ExportOptions& options,
                                    const duet::model::ExportProgress& progress = {});

/** One offline render, read back into memory.

    Every stretch is given in seconds from the render's start, and a stretch that
    reaches past either end is measured over what is there.
*/
class Render
{
public:
    /** Reads a rendered audio file. One that cannot be read holds nothing, which
        is what `readable` answers.
    */
    explicit Render (std::filesystem::path audioFile);

    [[nodiscard]] bool readable() const { return ! channels.empty(); }
    [[nodiscard]] double lengthSeconds() const;
    [[nodiscard]] const std::filesystem::path& file() const { return audio; }
    [[nodiscard]] int channelCount() const { return static_cast<int> (channels.size()); }

    /** Whether the render that made this ran on a worker thread, which is where
        the thread model puts an offline render. A Render read straight from a
        file was not made here and answers false.
    */
    [[nodiscard]] bool ranOffTheMessageThread() const { return offTheMessageThread; }

    /** The loudest sample over a stretch, across every channel. */
    [[nodiscard]] double peakBetween (double fromSeconds, double toSeconds) const;

    /** What a stretch measures rather than what its loudest sample was: the root
        mean square over every channel.
    */
    [[nodiscard]] double rmsBetween (double fromSeconds, double toSeconds) const;

    /** True when a stretch measures below the silence level. */
    [[nodiscard]] bool isSilentBetween (double fromSeconds, double toSeconds) const;

    /** The pitch of a stretch in hertz, counted from the rising zero crossings
        of the first channel, which is what a single steady tone can be measured
        by. Zero when there is no tone there to measure.

        The tolerance is the crossing spacing: a stretch of a few dozen cycles
        measures a steady tone to within a hertz or so, and a test states the
        margin it asks for.
    */
    [[nodiscard]] double pitchHzBetween (double fromSeconds, double toSeconds) const;

    /** Where the render starts making a sound, in seconds — one entry for each
        stretch of signal that follows silence.
    */
    [[nodiscard]] std::vector<double> onsetsSeconds() const;

    /** How much the level changed from one stretch of the render to another, in
        decibels: negative for the quieter second stretch. Zero when nothing
        changed.
    */
    [[nodiscard]] double levelChangeDb (double firstFromSeconds,
                                        double firstToSeconds,
                                        double secondFromSeconds,
                                        double secondToSeconds) const;

    /** How much of one frequency a stretch holds, in decibels of full scale.

        The spectral half of the same question, and the half an overall level
        cannot answer: a filter that takes a band away leaves the render's RMS
        almost where it was.
    */
    [[nodiscard]] double
        toneLevelDbBetween (double frequencyHz, double fromSeconds, double toSeconds) const;

    /** Whether two renders hold the same samples, bit for bit.

        The determinism canary, and the only sample comparison ADR 0006 allows:
        both renders must have been made by this process. A rendered file kept
        from an earlier run is a golden file, whatever it is called.

        The samples and not the files: the engine stamps the wall clock into
        every rendered file's broadcast-wave header, so two renders of one edit
        differ in a header byte a second apart and in nothing else.
    */
    [[nodiscard]] bool isBitIdenticalTo (const Render& other) const;

private:
    friend Render renderProject (duet::model::Session&, const std::filesystem::path&);
    friend Render
        renderTrack (duet::model::Session&, duet::model::TrackRef, const std::filesystem::path&);
    friend Render exportProject (duet::model::Session&,
                                 const duet::model::ExportOptions&,
                                 const duet::model::ExportProgress&);

    std::filesystem::path audio;
    double sampleRate = renderSampleRate;
    std::vector<std::vector<float>> channels;
    bool offTheMessageThread = false;
};
} // namespace duet::testing
