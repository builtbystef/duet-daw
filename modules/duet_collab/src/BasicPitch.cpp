#include <duet/collab/Transcription.h>

#include <onnxruntime_cxx_api.h>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

/** Basic Pitch, as Spotify serialised it to ONNX, run over a waveform.

    The model is the whole of the signal processing as well as the network: its
    graph begins with the constant-Q transform, so what it is handed is one
    channel of audio at 22050 Hz and what comes back is three posteriorgrams —
    how strongly each of the 88 piano pitches is sounding in each 11.6 ms frame,
    how strongly each is being struck, and a finer-grained contour this routine
    does not read, because it is what pitch bends are made of and Duet asks for
    none.

    Turning those into notes is Spotify's own algorithm and is reproduced here
    faithfully, including the thresholds it publishes as its defaults: a note
    begins at a peak in the onset posteriorgram, runs until its pitch stops
    sounding, and takes its energy out of what remains, so that a note already
    named cannot be named twice. What is left over afterwards is swept once
    more, for the notes that were held rather than struck — the "melodia trick",
    which is what makes a legato line come out as a line rather than as its
    first note.

    This file exists only in a build that has the ML runtime.
    `NoBasicPitch.cpp` is what stands in its place otherwise, and the two
    answer the same header.
*/
namespace duet::collab::transcription
{
namespace
{
    //==========================================================================
    // What the model is. Every number here is Basic Pitch's own, and changing
    // one of them without changing the weights makes the answers wrong rather
    // than different.

    /** The rate the graph's constant-Q transform was built for. */
    constexpr double modelRate = 22050.0;

    /** How far apart two frames of the answer are, in samples of that rate:
        86.13 frames a second.
    */
    constexpr int hopSamples = 256;

    /** How much audio one call to the model reads: two seconds, less one hop. */
    constexpr int windowSamples = 43844;

    /** How many frames it answers with for that window. */
    constexpr int framesPerWindow = 172;

    /** How many frames of the answer one second of audio is worth. Basic Pitch
        computes this by integer division and then counts frames with it, so a
        frame every 256 samples and 86 frames a second are two slightly
        different numbers here, exactly as they are there.
    */
    constexpr int framesPerSecond = 86;

    /** How many pitches it names: the 88 of a piano, the lowest of them A0. */
    constexpr int noteBins = 88;
    constexpr int lowestPitch = 21;

    /** How much of each window is also read by the window before or after it.
        Half of it is dropped from each end of a window's answer, which is what
        keeps the seam between two windows from reading as a note beginning.
    */
    constexpr int overlapFrames = 30;
    constexpr int overlapSamples = overlapFrames * hopSamples;
    constexpr int trimmedFrames = overlapFrames / 2;
    constexpr int framesPerStep = framesPerWindow - overlapFrames;
    constexpr int windowStep = windowSamples - overlapSamples;

    /** The names the graph gives its own tensors. The weights are pinned by
        hash, so these are facts about the file rather than a guess about it —
        and a file that does not carry them is not the file this routine reads,
        which is why the load checks and gives up rather than working from
        whichever tensor came first.
    */
    constexpr const char* audioInput = "serving_default_input_2:0";
    constexpr const char* onsetOutput = "StatefulPartitionedCall:2";
    constexpr const char* noteOutput = "StatefulPartitionedCall:1";

    /** Where the weights are, once the application is installed: beside the
        binary, the way the sidecar is (ADR 0003).
    */
    constexpr const char* modelFileName = "nmp.onnx";

    //==========================================================================
    // Basic Pitch's published defaults for turning posteriorgrams into notes.

    /** How strong a peak in the onset posteriorgram has to be to start a note. */
    constexpr float onsetThreshold = 0.5F;

    /** How strongly a pitch has to be sounding for its note to still be
        running.
    */
    constexpr float frameThreshold = 0.3F;

    /** How many frames a note has to last to be worth naming — about 128 ms —
        and how many quiet frames in a row end one. A note is not ended by the
        first frame that falls below the threshold, because a real one wavers.
    */
    constexpr int minimumNoteFrames = 11;
    constexpr int energyTolerance = 11;

    /** How many frames apart the two differences are that stand in for an onset
        where the model reported none: a pitch that grows louder over one or two
        frames was struck, whether or not the onset head noticed.
    */
    constexpr int inferredOnsetSpans = 2;

    //==========================================================================
    /** One channel of the waveform at the model's own rate.

        The channels are summed before the rate is changed, because what the
        model reads is one signal and the cheapest place to become one is before
        the expensive part. The rate is changed by a windowed sinc rather than
        by dropping samples: a track rendered at 44.1 kHz holds sound above 11
        kHz, and dropping every other sample would fold that down onto the
        pitches this routine is trying to read.
    */
    std::vector<float> monoAtModelRate (const analysis::Waveform& waveform)
    {
        if (waveform.empty() || waveform.sampleRate <= 0.0)
            return {};

        const auto length = waveform.length();
        std::vector<float> mono (length, 0.0F);

        for (const auto& channel : waveform.channels)
            for (std::size_t sample = 0; sample < length; ++sample)
                mono[sample] += channel[sample];

        const auto scale = 1.0F / static_cast<float> (waveform.channels.size());

        for (auto& sample : mono)
            sample *= scale;

        const auto ratio = modelRate / waveform.sampleRate;

        if (std::abs (ratio - 1.0) < 1.0e-9)
            return mono;

        // The ideal low-pass, cut off at whichever of the two rates is lower,
        // windowed to a finite length. Sixteen zero crossings each side is
        // where a Blackman window stops buying anything a transcription can
        // hear.
        constexpr double zeroCrossings = 16.0;
        const auto cutoff = 0.5 * std::min (1.0, ratio);
        const auto halfWidth = zeroCrossings / (2.0 * cutoff);
        const auto outputLength =
            static_cast<std::size_t> (std::floor (static_cast<double> (length) * ratio));

        std::vector<float> resampled (outputLength, 0.0F);

        for (std::size_t out = 0; out < outputLength; ++out)
        {
            const auto centre = static_cast<double> (out) / ratio;
            const auto from = static_cast<std::int64_t> (std::ceil (centre - halfWidth));
            const auto to = static_cast<std::int64_t> (std::floor (centre + halfWidth));
            const auto first = std::max<std::int64_t> (0, from);
            const auto last = std::min<std::int64_t> (static_cast<std::int64_t> (length) - 1, to);

            double sum = 0.0;

            for (auto in = first; in <= last; ++in)
            {
                const auto offset = centre - static_cast<double> (in);
                const auto argument = 2.0 * cutoff * offset;
                const auto sinc =
                    std::abs (argument) < 1.0e-12
                        ? 1.0
                        : std::sin (std::numbers::pi * argument) / (std::numbers::pi * argument);
                const auto position = 0.5 * (1.0 + offset / halfWidth);
                const auto window = 0.42 - 0.5 * std::cos (2.0 * std::numbers::pi * position)
                                    + 0.08 * std::cos (4.0 * std::numbers::pi * position);

                sum += 2.0 * cutoff * sinc * window
                       * static_cast<double> (mono[static_cast<std::size_t> (in)]);
            }

            resampled[out] = static_cast<float> (sum);
        }

        return resampled;
    }

    //==========================================================================
    /** The model, loaded once and kept, because loading it costs more than
        reading a whole track through it.

        Kept behind a function-local static, so that a build that never
        transcribes never pays for it, and so that the load happens once however
        many threads ask. The Collaborator service calls tools one at a time on
        its own thread, which is what lets the session itself hold no lock.
    */
    struct Model
    {
        std::optional<Ort::Env> environment;
        std::optional<Ort::Session> session;
        bool usable = false;
    };

    std::filesystem::path modelFile()
    {
        std::array<char, 4096> buffer {};
        const auto written = ::readlink ("/proc/self/exe", buffer.data(), buffer.size() - 1);

        if (written > 0)
        {
            const std::filesystem::path running { std::string {
                buffer.data(), static_cast<std::size_t> (written) } };
            const auto beside = running.parent_path() / modelFileName;

            std::error_code ignored;

            if (std::filesystem::exists (beside, ignored))
                return beside;
        }

        return std::filesystem::path { DUET_BASIC_PITCH_MODEL };
    }

    /** Whether the graph is the one this routine knows how to read: three
        tensors, named what Spotify's serialisation names them.
    */
    bool graphIsKnown (Ort::Session& session)
    {
        if (session.GetInputCount() != 1 || session.GetOutputCount() != 3)
            return false;

        const Ort::AllocatorWithDefaultOptions allocator;

        const auto named = [&] (const auto& owned, const char* expected)
        { return std::string { owned.get() } == expected; };

        return named (session.GetInputNameAllocated (0, allocator), audioInput)
               && named (session.GetOutputNameAllocated (0, allocator), onsetOutput)
               && named (session.GetOutputNameAllocated (1, allocator), noteOutput);
    }

    Model loadModel()
    {
        Model model;

        std::error_code ignored;
        const auto file = modelFile();

        if (! std::filesystem::exists (file, ignored))
            return model;

        try
        {
            model.environment.emplace (ORT_LOGGING_LEVEL_ERROR, "duet");

            // One thread and no pool of its own: this runs on the Collaborator
            // service's thread and the spec's real-time rule is that no
            // AI-related code shares anything with the audio callback. A
            // runtime that started threads of its own would be a second thing
            // to reason about for no speed a multi-second call notices.
            Ort::SessionOptions options;
            options.SetIntraOpNumThreads (1);
            options.SetInterOpNumThreads (1);
            options.SetExecutionMode (ORT_SEQUENTIAL);
            options.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);

            model.session.emplace (*model.environment, file.c_str(), options);
            model.usable = graphIsKnown (*model.session);
        }
        catch (const Ort::Exception&)
        {
            model.usable = false;
        }

        return model;
    }

    Model& model()
    {
        static Model loaded = loadModel();

        return loaded;
    }

    //==========================================================================
    /** Two posteriorgrams over the same frames: how strongly each pitch is
        sounding, and how strongly each is being struck. Row-major, one row to a
        frame.
    */
    struct Posteriorgrams
    {
        std::vector<float> notes;
        std::vector<float> onsets;
        std::size_t frames = 0;

        [[nodiscard]] float note (std::size_t frame, std::size_t bin) const
        {
            return notes[frame * noteBins + bin];
        }

        [[nodiscard]] float& remaining (std::size_t frame, std::size_t bin)
        {
            return notes[frame * noteBins + bin];
        }
    };

    /** Every window of the audio, read through the model and joined back into
        one run of frames.

        The audio is offset by half the overlap before it is cut up, and half
        the overlap is dropped from each end of every answer, which is Basic
        Pitch's own arrangement: what a window says about its own edges is what
        it says least well, and there is always another window that saw the same
        moment in its middle.
    */
    std::optional<Posteriorgrams> readThrough (const std::vector<float>& mono,
                                               const StillWanted& keepGoing)
    {
        auto& loaded = model();

        if (! loaded.usable || ! loaded.session.has_value())
            return {};

        auto& session = *loaded.session;

        std::vector<float> padded (static_cast<std::size_t> (overlapSamples / 2), 0.0F);
        padded.insert (padded.end(), mono.begin(), mono.end());

        const auto windows = static_cast<std::size_t> (std::max<std::int64_t> (
            1, (static_cast<std::int64_t> (padded.size()) - 1) / windowStep + 1));

        Posteriorgrams heard;
        heard.notes.reserve (windows * framesPerStep * noteBins);
        heard.onsets.reserve (windows * framesPerStep * noteBins);

        std::vector<float> window (static_cast<std::size_t> (windowSamples), 0.0F);
        const std::array<std::int64_t, 3> shape { 1, windowSamples, 1 };
        const auto memory = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<const char*, 1> inputNames { audioInput };
        const std::array<const char*, 2> outputNames { onsetOutput, noteOutput };

        for (std::size_t index = 0; index < windows; ++index)
        {
            if (keepGoing && ! keepGoing())
                return {};

            std::fill (window.begin(), window.end(), 0.0F);

            const auto from = index * static_cast<std::size_t> (windowStep);
            const auto available = from < padded.size() ? padded.size() - from : 0;
            const auto taken = std::min (available, static_cast<std::size_t> (windowSamples));

            std::copy_n (
                padded.begin() + static_cast<std::ptrdiff_t> (from), taken, window.begin());

            try
            {
                auto tensor = Ort::Value::CreateTensor<float> (
                    memory, window.data(), window.size(), shape.data(), shape.size());

                auto answered = session.Run (Ort::RunOptions { nullptr },
                                             inputNames.data(),
                                             &tensor,
                                             inputNames.size(),
                                             outputNames.data(),
                                             outputNames.size());

                constexpr auto perWindow = static_cast<std::size_t> (framesPerWindow) * noteBins;
                const std::span<const float> onsets { answered[0].GetTensorData<float>(),
                                                      perWindow };
                const std::span<const float> notes { answered[1].GetTensorData<float>(),
                                                     perWindow };

                for (int frame = trimmedFrames; frame < framesPerWindow - trimmedFrames; ++frame)
                {
                    const auto offset = static_cast<std::size_t> (frame) * noteBins;
                    const auto struck = onsets.subspan (offset, noteBins);
                    const auto sounding = notes.subspan (offset, noteBins);

                    heard.onsets.insert (heard.onsets.end(), struck.begin(), struck.end());
                    heard.notes.insert (heard.notes.end(), sounding.begin(), sounding.end());
                }
            }
            catch (const Ort::Exception&)
            {
                return {};
            }
        }

        // Only the frames the audio itself covers: the last window was filled
        // out with silence to make the model's length, and silence has no notes
        // in it.
        const auto covered = static_cast<std::size_t> (
            std::floor (static_cast<double> (mono.size()) * framesPerSecond / modelRate));

        heard.frames = std::min (covered, heard.notes.size() / noteBins);
        heard.notes.resize (heard.frames * noteBins);
        heard.onsets.resize (heard.frames * noteBins);

        return heard;
    }

    //==========================================================================
    /** The onset posteriorgram, with a struck pitch the onset head missed
        standing in for itself.

        Where a pitch grows louder from one frame to the next by more than it
        does anywhere else, something was struck. The rise is scaled to the
        onsets' own range and taken wherever it is the larger of the two, so a
        note the model heard as an onset keeps the onset it heard.
    */
    void inferOnsets (Posteriorgrams& heard)
    {
        if (heard.frames <= static_cast<std::size_t> (inferredOnsetSpans))
            return;

        std::vector<float> rise (heard.onsets.size(), 0.0F);
        float loudestRise = 0.0F;
        float loudestOnset = 0.0F;

        for (const auto onset : heard.onsets)
            loudestOnset = std::max (loudestOnset, onset);

        for (auto frame = static_cast<std::size_t> (inferredOnsetSpans); frame < heard.frames;
             ++frame)
        {
            for (std::size_t bin = 0; bin < noteBins; ++bin)
            {
                auto smallest = std::numeric_limits<float>::max();

                for (int span = 1; span <= inferredOnsetSpans; ++span)
                    smallest =
                        std::min (smallest,
                                  heard.note (frame, bin)
                                      - heard.note (frame - static_cast<std::size_t> (span), bin));

                const auto grew = std::max (0.0F, smallest);

                rise[frame * noteBins + bin] = grew;
                loudestRise = std::max (loudestRise, grew);
            }
        }

        if (loudestRise <= 0.0F)
            return;

        const auto scale = loudestOnset / loudestRise;

        for (std::size_t index = 0; index < heard.onsets.size(); ++index)
            heard.onsets[index] = std::max (heard.onsets[index], rise[index] * scale);
    }

    /** Where in the waveform a frame of the answer falls, in seconds.

        Not simply the frame times the hop: a window answers with a few more
        frames than the audio it was given holds, so every window after the
        first would otherwise read a little late, by more and more of it. The
        correction is Basic Pitch's, including the last two milliseconds of it,
        which its own source calls a magic number.
    */
    double timeOfFrame (std::size_t frame)
    {
        constexpr auto perFrame = hopSamples / modelRate;
        constexpr auto perWindow =
            perFrame * (framesPerWindow - (static_cast<double> (windowSamples) / hopSamples))
            + 0.0018;

        const auto original = static_cast<double> (frame) * perFrame;
        const auto windows = std::floor (static_cast<double> (frame) / framesPerWindow);

        return original - perWindow * windows;
    }

    /** One note as the sweep found it, in frames. */
    struct Found
    {
        std::size_t from = 0;
        std::size_t to = 0;
        std::size_t bin = 0;
        double strength = 0.0;
    };

    /** How strongly that pitch was sounding over those frames, which is what
        becomes the note's velocity.
    */
    double strengthOf (const std::vector<float>& notes,
                       std::size_t from,
                       std::size_t to,
                       std::size_t bin)
    {
        if (to <= from)
            return 0.0;

        double sum = 0.0;

        for (auto frame = from; frame < to; ++frame)
            sum += static_cast<double> (notes[frame * noteBins + bin]);

        return sum / static_cast<double> (to - from);
    }

    /** What the loudest pitch in a frame is now, after something was taken out
        of it. Kept beside the posteriorgram so that the sweep for held notes
        can find the loudest frame left without reading the whole of it again
        for every note it names.
    */
    void refreshLoudest (const Posteriorgrams& heard,
                         std::size_t frame,
                         std::vector<float>& loudestInFrame)
    {
        auto loudest = 0.0F;

        for (std::size_t bin = 0; bin < noteBins; ++bin)
            loudest = std::max (loudest, heard.note (frame, bin));

        loudestInFrame[frame] = loudest;
    }

    /** Takes one note's energy out of what is left, so that the sweep for held
        notes cannot find the same note again. The neighbouring pitches go with
        it: a note sounding at one bin leaks into the two beside it.
    */
    void takeEnergy (Posteriorgrams& heard,
                     std::size_t frame,
                     std::size_t bin,
                     std::vector<float>& loudestInFrame)
    {
        heard.remaining (frame, bin) = 0.0F;

        if (bin + 1 < noteBins)
            heard.remaining (frame, bin + 1) = 0.0F;

        if (bin > 0)
            heard.remaining (frame, bin - 1) = 0.0F;

        refreshLoudest (heard, frame, loudestInFrame);
    }

    /** Where a note that began at `from` in that pitch stops sounding, and how
        many quiet frames were counted getting there. Quiet frames do not end a
        note until there have been a run of them, because a real note wavers.
    */
    struct Ran
    {
        std::size_t to = 0;
        int quiet = 0;
    };

    Ran runOf (const Posteriorgrams& heard, std::size_t from, std::size_t bin)
    {
        auto frame = from + 1;
        auto quiet = 0;

        while (frame + 1 < heard.frames && quiet < energyTolerance)
        {
            quiet = heard.note (frame, bin) < frameThreshold ? quiet + 1 : 0;
            ++frame;
        }

        return { frame, quiet };
    }

    /** Every peak in the onset posteriorgram strong enough to begin a note, in
        the order they happen.
    */
    std::vector<std::pair<std::size_t, std::size_t>> struckPeaks (const Posteriorgrams& heard)
    {
        std::vector<std::pair<std::size_t, std::size_t>> struck;

        for (std::size_t frame = 1; frame + 1 < heard.frames; ++frame)
            for (std::size_t bin = 0; bin < noteBins; ++bin)
            {
                const auto here = heard.onsets[frame * noteBins + bin];

                if (here >= onsetThreshold && here > heard.onsets[(frame - 1) * noteBins + bin]
                    && here > heard.onsets[(frame + 1) * noteBins + bin])
                    struck.emplace_back (frame, bin);
            }

        return struck;
    }

    /** The notes that were struck: each onset peak, latest first, so that a
        note takes its energy out of what is left before the notes struck under
        it are looked at.
    */
    void nameWhatWasStruck (Posteriorgrams& heard,
                            const std::vector<float>& whole,
                            std::vector<float>& loudestInFrame,
                            std::vector<Found>& found)
    {
        const auto struck = struckPeaks (heard);

        for (auto candidate = struck.size(); candidate-- > 0;)
        {
            const auto start = struck[candidate].first;
            const auto bin = struck[candidate].second;

            if (start + 1 >= heard.frames)
                continue;

            const auto ran = runOf (heard, start, bin);
            const auto end = ran.to - static_cast<std::size_t> (ran.quiet);

            if (end <= start + minimumNoteFrames)
                continue;

            for (auto frame = start; frame < end; ++frame)
                takeEnergy (heard, frame, bin, loudestInFrame);

            found.push_back ({ start, end, bin, strengthOf (whole, start, end, bin) });
        }
    }

    /** Where the loudest pitch left in the posteriorgram is: the frame first
        and the lowest bin of that frame holding it, which is the order Basic
        Pitch's own search takes them in.
    */
    std::pair<std::size_t, std::size_t> loudestLeft (const Posteriorgrams& heard,
                                                     const std::vector<float>& loudestInFrame)
    {
        auto loudest = 0.0F;
        std::size_t at = 0;

        for (std::size_t frame = 0; frame < heard.frames; ++frame)
            if (loudestInFrame[frame] > loudest)
            {
                loudest = loudestInFrame[frame];
                at = frame;
            }

        for (std::size_t bin = 0; bin < noteBins; ++bin)
            if (heard.note (at, bin) >= loudest)
                return { at, bin };

        return { at, 0 };
    }

    /** The notes that were held rather than struck — the "melodia trick".

        What is still loud after every onset has taken its own energy out was
        sounding without ever beginning, which is what a legato line looks like
        from here. Each one is grown forwards and backwards from the loudest
        frame left, and taken out of what remains whether or not it turned out
        long enough to name, which is what ends this.
    */
    void nameWhatWasHeld (Posteriorgrams& heard,
                          const std::vector<float>& whole,
                          std::vector<float>& loudestInFrame,
                          std::vector<Found>& found)
    {
        const auto frames = heard.frames;

        while (*std::max_element (loudestInFrame.begin(), loudestInFrame.end()) > frameThreshold)
        {
            const auto [at, bin] = loudestLeft (heard, loudestInFrame);

            heard.remaining (at, bin) = 0.0F;
            refreshLoudest (heard, at, loudestInFrame);

            auto frame = at + 1;
            auto quiet = 0;

            while (frame + 1 < frames && quiet < energyTolerance)
            {
                quiet = heard.note (frame, bin) < frameThreshold ? quiet + 1 : 0;
                takeEnergy (heard, frame, bin, loudestInFrame);
                ++frame;
            }

            const auto end = frame - 1 - static_cast<std::size_t> (quiet);

            auto backward = static_cast<std::int64_t> (at) - 1;
            quiet = 0;

            while (backward > 0 && quiet < energyTolerance)
            {
                const auto index = static_cast<std::size_t> (backward);

                quiet = heard.note (index, bin) < frameThreshold ? quiet + 1 : 0;
                takeEnergy (heard, index, bin, loudestInFrame);
                --backward;
            }

            const auto start =
                static_cast<std::size_t> (backward + 1 + static_cast<std::int64_t> (quiet));

            if (end <= start + minimumNoteFrames || end > frames)
                continue;

            found.push_back ({ start, end, bin, strengthOf (whole, start, end, bin) });
        }
    }

    /** Basic Pitch's sweep: every onset peak, and then what is left over. */
    std::vector<Found> sweep (Posteriorgrams heard)
    {
        if (heard.frames < 2)
            return {};

        // What the notes were before anything was taken out of them, which is
        // what a note's strength is read off: the sweep empties the working
        // copy as it goes.
        const auto whole = heard.notes;

        inferOnsets (heard);

        std::vector<float> loudestInFrame (heard.frames, 0.0F);

        for (std::size_t frame = 0; frame < heard.frames; ++frame)
            refreshLoudest (heard, frame, loudestInFrame);

        std::vector<Found> found;

        nameWhatWasStruck (heard, whole, loudestInFrame, found);
        nameWhatWasHeld (heard, whole, loudestInFrame, found);

        return found;
    }
} // namespace

//==============================================================================
bool available() { return model().usable; }

std::optional<Transcribed> transcribed (const analysis::Waveform& waveform,
                                        const StillWanted& keepGoing)
{
    if (! available())
        return {};

    const auto mono = monoAtModelRate (waveform);

    if (mono.empty())
        return {};

    const auto heard = readThrough (mono, keepGoing);

    if (! heard.has_value())
        return {};

    Transcribed answer;
    double strengths = 0.0;

    for (const auto& note : sweep (*heard))
    {
        const auto start = timeOfFrame (note.from);
        const auto end = timeOfFrame (note.to);

        answer.notes.push_back ({ static_cast<int> (note.bin) + lowestPitch,
                                  start,
                                  std::max (0.0, end - start),
                                  std::clamp (note.strength, 0.0, 1.0) });
        strengths += std::clamp (note.strength, 0.0, 1.0);
    }

    if (answer.notes.empty())
        return answer;

    std::sort (answer.notes.begin(),
               answer.notes.end(),
               [] (const Note& first, const Note& second)
               {
                   return first.startSeconds != second.startSeconds
                              ? first.startSeconds < second.startSeconds
                              : first.pitch < second.pitch;
               });

    // One confidence for the whole reading, because the reading is one
    // estimate: how strongly the notes it named were sounding, on average.
    answer.confidence = strengths / static_cast<double> (answer.notes.size());

    return answer;
}
} // namespace duet::collab::transcription
