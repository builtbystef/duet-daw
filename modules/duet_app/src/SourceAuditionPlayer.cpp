#include <duet/app/SourceAuditionPlayer.h>

#include <duet/model/AudioFile.h>
#include <duet/model/EngineAccess.h>

#include <juce_audio_devices/juce_audio_devices.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace duet::app
{
namespace
{
    constexpr int outputChannels = 2;
    constexpr int defaultSampleRateHz = 44100;

    static_assert (std::atomic<int>::is_always_lock_free);
    static_assert (std::atomic<std::uint64_t>::is_always_lock_free);
    static_assert (std::atomic<bool>::is_always_lock_free);

    class ThreadQueue
    {
    public:
        ThreadQueue() : worker ([this] { run(); }) {}

        ~ThreadQueue()
        {
            {
                const std::lock_guard lock { mutex };
                stopping = true;
            }
            ready.notify_one();
            worker.join();
        }

        ThreadQueue (const ThreadQueue&) = delete;
        ThreadQueue& operator= (const ThreadQueue&) = delete;

        void post (std::function<void()> work)
        {
            {
                const std::lock_guard lock { mutex };

                if (stopping)
                    return;

                jobs.push_back (std::move (work));
            }
            ready.notify_one();
        }

    private:
        void run()
        {
            for (;;)
            {
                std::function<void()> job;

                {
                    std::unique_lock lock { mutex };
                    ready.wait (lock, [this] { return stopping || ! jobs.empty(); });

                    if (stopping && jobs.empty())
                        return;

                    job = std::move (jobs.front());
                    jobs.pop_front();
                }

                job();
            }
        }

        std::mutex mutex;
        std::condition_variable ready;
        std::deque<std::function<void()>> jobs;
        bool stopping = false;
        std::thread worker;
    };

    struct Prepared
    {
        std::vector<float> interleaved;
        int channels = 0;
        int frames = 0;
    };

    /** Linear-interpolated stereo at the callback rate, from whatever the file
        held. Mono is duplicated; extra file channels are ignored. Worker-only.
    */
    [[nodiscard]] Prepared resample (const duet::model::AudioSamples& samples, double rateHz)
    {
        Prepared prepared;
        prepared.channels = outputChannels;

        if (! samples.readable() || rateHz <= 0.0)
            return prepared;

        const auto inFrames = samples.channels.front().size();
        const auto inRate = samples.sampleRate;
        const auto outFrames =
            static_cast<int> (std::llround (static_cast<double> (inFrames) * rateHz / inRate));
        prepared.frames = std::max (0, outFrames);
        prepared.interleaved.resize (static_cast<std::size_t> (prepared.frames)
                                     * static_cast<std::size_t> (outputChannels));

        const auto last = inFrames == 0 ? 0 : inFrames - 1;

        for (int frame = 0; frame < prepared.frames; ++frame)
        {
            const auto source = static_cast<double> (frame) * inRate / rateHz;
            const auto index = static_cast<std::size_t> (source);
            const auto fraction = static_cast<float> (source - static_cast<double> (index));
            const auto next = std::min (index + 1, last);

            for (int channel = 0; channel < outputChannels; ++channel)
            {
                const auto& in = samples.channels[std::min (static_cast<std::size_t> (channel),
                                                            samples.channels.size() - 1)];
                const auto a = in[index];
                const auto b = in[next];
                const auto out =
                    static_cast<std::size_t> (frame) * static_cast<std::size_t> (outputChannels)
                    + static_cast<std::size_t> (channel);
                prepared.interleaved[out] = a + (b - a) * fraction;
            }
        }

        return prepared;
    }
} // namespace

struct SourceAuditionPlayer::Impl
{
    class Callback final : public juce::AudioIODeviceCallback
    {
    public:
        Callback (SourceAuditionPlayer& owner, Impl& state) : player (&owner), impl (&state) {}

        void audioDeviceIOCallbackWithContext (
            const float* const* /*inputs*/,
            int /*numInputs*/,
            float* const* outputs,
            int numChannels,
            int numSamples,
            const juce::AudioIODeviceCallbackContext& /*context*/) override
        {
            if (outputs == nullptr || numChannels <= 0 || numSamples <= 0)
                return;

            for (auto* lane :
                 std::span<float* const> { outputs, static_cast<std::size_t> (numChannels) })
                if (lane != nullptr)
                    juce::FloatVectorOperations::clear (lane, numSamples);

            player->mix (outputs, numChannels, numSamples);
        }

        void audioDeviceAboutToStart (juce::AudioIODevice* device) override
        {
            if (device == nullptr)
                return;

            player->prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
        }

        /** Device loss is the one stop path that can arrive off the message
            thread, so it touches only atomics: the next mix() finds nothing to
            play, and the surface reads stopped on its next poll. The strings
            and the change callback are the message thread's.
        */
        void audioDeviceStopped() override
        {
            impl->generation.fetch_add (1, std::memory_order_relaxed);
            impl->loading.store (false, std::memory_order_release);
            impl->active.store (-1, std::memory_order_release);
            impl->frame.store (0, std::memory_order_relaxed);
        }

    private:
        SourceAuditionPlayer* player;
        Impl* impl;
    };

    Impl (SourceAuditionPlayer& owner, Executor executeOnWorker, Poster postToMessageThread)
        : callback (owner, *this), execute (std::move (executeOnWorker)),
          post (std::move (postToMessageThread))
    {
    }

    Callback callback;
    Executor execute;
    Poster post;
    std::function<void()> changed;
    juce::AudioDeviceManager* devices = nullptr;
    Prepared firstSlot;
    Prepared secondSlot;

    /** The two prepared slots alternate, so the one the callback reads is never
        the one the message thread is filling. Index is 0 or 1.
    */
    [[nodiscard]] Prepared& slotAt (int index) noexcept
    {
        return index == 0 ? firstSlot : secondSlot;
    }

    [[nodiscard]] const Prepared& slotAt (int index) const noexcept
    {
        return index == 0 ? firstSlot : secondSlot;
    }

    std::atomic<int> active { -1 };
    std::atomic<int> frame { 0 };
    std::atomic<int> outputRateHz { defaultSampleRateHz };
    std::atomic<std::uint64_t> generation { 0 };
    std::atomic<bool> loading { false };
    std::atomic<bool> stopping { false };
    int nextSlot = 0;
    std::string identity;
    std::string error;
    std::unique_ptr<ThreadQueue> owned;
};

SourceAuditionPlayer::SourceAuditionPlayer (Poster postToMessageThread)
{
    auto queue = std::make_unique<ThreadQueue>();
    auto* queuePtr = queue.get();
    impl = std::make_unique<Impl> (
        *this,
        [queuePtr] (Work work) { queuePtr->post (std::move (work)); },
        std::move (postToMessageThread));
    impl->owned = std::move (queue);
}

SourceAuditionPlayer::SourceAuditionPlayer (Executor execute, Poster postToMessageThread)
    : impl (std::make_unique<Impl> (*this, std::move (execute), std::move (postToMessageThread)))
{
}

SourceAuditionPlayer::~SourceAuditionPlayer()
{
    if (impl == nullptr)
        return;

    impl->stopping.store (true, std::memory_order_relaxed);
    detachFromOutput();
    impl->generation.fetch_add (1, std::memory_order_relaxed);
    impl->active.store (-1, std::memory_order_release);
    impl->owned.reset();
}

void SourceAuditionPlayer::onChanged (std::function<void()> callback)
{
    impl->changed = std::move (callback);
}

void SourceAuditionPlayer::prepare (double sampleRate, int /*maximumBlockSize*/)
{
    const auto hz =
        sampleRate > 0.0 ? static_cast<int> (std::llround (sampleRate)) : defaultSampleRateHz;
    impl->outputRateHz.store (std::max (1, hz), std::memory_order_relaxed);
}

void SourceAuditionPlayer::play (std::filesystem::path file, std::string identity)
{
    const auto generation = impl->generation.fetch_add (1, std::memory_order_relaxed) + 1;
    impl->identity = std::move (identity);
    impl->error.clear();
    impl->loading.store (true, std::memory_order_release);
    impl->active.store (-1, std::memory_order_release);
    impl->frame.store (0, std::memory_order_relaxed);

    if (impl->changed)
        impl->changed();

    const auto rateHz = impl->outputRateHz.load (std::memory_order_relaxed);
    auto execute = impl->execute;
    auto post = impl->post;
    auto* state = impl.get();

    execute (
        [state, generation, file = std::move (file), rateHz, post = std::move (post)]
        {
            if (state->stopping.load (std::memory_order_relaxed)
                || state->generation.load (std::memory_order_relaxed) != generation)
                return;

            const auto samples = duet::model::readAudioFile (file);
            auto prepared = resample (samples, static_cast<double> (rateHz));
            const auto readable = samples.readable() && prepared.frames > 0;

            post (
                [state, generation, readable, prepared = std::move (prepared)]() mutable
                {
                    if (state->stopping.load (std::memory_order_relaxed)
                        || state->generation.load (std::memory_order_relaxed) != generation)
                        return;

                    state->loading.store (false, std::memory_order_release);

                    if (! readable)
                    {
                        state->active.store (-1, std::memory_order_release);
                        state->error = "Could not play this file";
                    }
                    else
                    {
                        state->error.clear();
                        const auto slot = state->nextSlot;
                        state->slotAt (slot) = std::move (prepared);
                        state->frame.store (0, std::memory_order_relaxed);
                        state->active.store (slot, std::memory_order_release);
                        state->nextSlot = 1 - slot;
                    }

                    if (state->changed)
                        state->changed();
                });
        });
}

void SourceAuditionPlayer::stop()
{
    impl->generation.fetch_add (1, std::memory_order_relaxed);
    impl->loading.store (false, std::memory_order_release);
    impl->active.store (-1, std::memory_order_release);
    impl->frame.store (0, std::memory_order_relaxed);
    impl->error.clear();
    impl->identity.clear();

    if (impl->changed)
        impl->changed();
}

duet::gui::SourceAuditionStatus SourceAuditionPlayer::status() const
{
    duet::gui::SourceAuditionStatus snapshot;
    snapshot.identity = impl->identity;
    snapshot.error = impl->error;

    if (! impl->error.empty())
    {
        snapshot.state = duet::gui::SourceAuditionState::error;
        return snapshot;
    }

    if (impl->loading.load (std::memory_order_acquire))
    {
        snapshot.state = duet::gui::SourceAuditionState::loading;
        return snapshot;
    }

    const auto slot = impl->active.load (std::memory_order_acquire);

    if (slot < 0)
        return snapshot;

    const auto& prepared = impl->slotAt (slot);
    const auto at = impl->frame.load (std::memory_order_relaxed);
    snapshot.state = duet::gui::SourceAuditionState::playing;
    snapshot.progress = prepared.frames > 0
                            ? std::clamp (static_cast<double> (at) / prepared.frames, 0.0, 1.0)
                            : 0.0;
    return snapshot;
}

void SourceAuditionPlayer::mix (float* const* outputs,
                                int numChannels,
                                int numSamples) noexcept DUET_NONBLOCKING
{
    if (outputs == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    const auto slot = impl->active.load (std::memory_order_acquire);

    if (slot < 0)
        return;

    const auto& prepared = impl->slotAt (slot);
    const auto* data = prepared.interleaved.data();
    const auto frames = prepared.frames;
    const auto channels = prepared.channels;
    auto at = impl->frame.load (std::memory_order_relaxed);

    if (data == nullptr || frames <= 0 || channels <= 0 || at >= frames)
    {
        impl->active.store (-1, std::memory_order_release);
        return;
    }

    const auto remaining = frames - at;
    const auto count = numSamples < remaining ? numSamples : remaining;
    constexpr auto gain = sourceAuditionGain;
    const std::span<float* const> lanes { outputs, static_cast<std::size_t> (numChannels) };
    const std::span<const float> source {
        data, static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels)
    };
    const auto stride = static_cast<std::size_t> (channels);
    const auto from = static_cast<std::size_t> (at);

    for (std::size_t channel = 0; channel < lanes.size(); ++channel)
    {
        if (lanes[channel] == nullptr)
            continue;

        const std::span<float> out { lanes[channel], static_cast<std::size_t> (count) };
        const auto sourceChannel = channel < stride ? channel : std::size_t { 0 };

        for (std::size_t sample = 0; sample < out.size(); ++sample)
            out[sample] += source[(from + sample) * stride + sourceChannel] * gain;
    }

    at += count;
    impl->frame.store (at, std::memory_order_relaxed);

    if (at >= frames)
        impl->active.store (-1, std::memory_order_release);
}

void SourceAuditionPlayer::attachToOutput (duet::model::Session& session)
{
    detachFromOutput();
    impl->devices = &duet::model::EngineAccess::audioDevicesOf (session);
    impl->devices->addAudioCallback (&impl->callback);
}

void SourceAuditionPlayer::detachFromOutput()
{
    if (impl->devices == nullptr)
        return;

    impl->devices->removeAudioCallback (&impl->callback);
    impl->devices = nullptr;
}
} // namespace duet::app
