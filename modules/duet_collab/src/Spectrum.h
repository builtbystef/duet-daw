#pragma once

#include <duet/collab/Analysis.h>

#include <cstddef>
#include <vector>

/** The windowed power spectrum every spectral routine reads, measured once and
    shared, because a band energy and a pitch-class profile are two readings of
    the same transform.
*/
namespace duet::collab::analysis::spectrum
{
/** The window a waveform is long enough for: the longest that fits, down to the
    shortest worth transforming, and zero below that.
*/
[[nodiscard]] std::size_t windowFor (const Waveform& waveform);

/** The average power spectrum of a waveform, one entry per bin up to the
    Nyquist rate, scaled so that the entries of a stretch of the spectrum add up
    to the mean square of what that stretch holds.

    Every channel goes into the same average, so what a bin measures is what the
    whole waveform puts there. Empty when the waveform holds no whole window.
*/
[[nodiscard]] std::vector<double> power (const Waveform& waveform, std::size_t window);

/** The frequency a bin stands for, in hertz. */
[[nodiscard]] double binFrequency (std::size_t bin, std::size_t window, double sampleRate);
} // namespace duet::collab::analysis::spectrum
