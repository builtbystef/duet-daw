#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

/** The one transform the spectral routines are made of.

    Radix-2 and in place, which is all a power-of-two window needs, and written
    here because this target links nothing: a Fourier transform is forty lines
    and the alternative is a dependency the project would carry for them.
*/
namespace duet::collab::analysis::fft
{
/** Transforms in place. Both halves must be the same length and a power of two.
 */
inline void forward (std::vector<double>& real, std::vector<double>& imaginary)
{
    const auto size = real.size();

    if (size < 2)
        return;

    // Bit reversal: the transform below reads its input in the order the
    // butterflies want it rather than the order it was written in.
    for (std::size_t index = 1, reversed = 0; index < size; ++index)
    {
        auto bit = size >> 1;

        for (; (reversed & bit) != 0; bit >>= 1)
            reversed ^= bit;

        reversed ^= bit;

        if (index < reversed)
        {
            std::swap (real[index], real[reversed]);
            std::swap (imaginary[index], imaginary[reversed]);
        }
    }

    for (std::size_t span = 2; span <= size; span <<= 1)
    {
        const auto angle = -2.0 * std::numbers::pi / static_cast<double> (span);
        const auto stepReal = std::cos (angle);
        const auto stepImaginary = std::sin (angle);

        for (std::size_t start = 0; start < size; start += span)
        {
            double turnReal = 1.0;
            double turnImaginary = 0.0;

            for (std::size_t offset = 0; offset < span / 2; ++offset)
            {
                const auto here = start + offset;
                const auto there = here + span / 2;

                const auto oddReal = real[there] * turnReal - imaginary[there] * turnImaginary;
                const auto oddImaginary = real[there] * turnImaginary + imaginary[there] * turnReal;

                real[there] = real[here] - oddReal;
                imaginary[there] = imaginary[here] - oddImaginary;
                real[here] += oddReal;
                imaginary[here] += oddImaginary;

                const auto turned = turnReal * stepReal - turnImaginary * stepImaginary;
                turnImaginary = turnReal * stepImaginary + turnImaginary * stepReal;
                turnReal = turned;
            }
        }
    }
}
} // namespace duet::collab::analysis::fft
