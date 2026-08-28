#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

namespace duet::model
{
/** An audio file read into memory: the samples of each channel, and the rate
    they are to be read at.

    The read-back half of the offline render path. A render writes a file and
    what measures it needs the samples, and this module is the only one that can
    open an audio file, so this is where the crossing is — in the model's own
    engine-free vocabulary, which is plain floats and a rate.

    A file that cannot be read comes back with no channels at all.
*/
struct AudioSamples
{
    double sampleRate = 0.0;
    std::vector<std::vector<float>> channels;

    [[nodiscard]] bool readable() const { return sampleRate > 0.0 && ! channels.empty(); }
};

/** Reads an audio file into memory. */
[[nodiscard]] AudioSamples readAudioFile (const std::filesystem::path& file);
} // namespace duet::model
