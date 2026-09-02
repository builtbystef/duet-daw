#pragma once

#include <duet/gui/Browser.h>
#include <duet/realtime/Callback.h>

#include <functional>
#include <memory>
#include <string>

namespace duet::model
{
class Session;
}

namespace duet::app
{
/** The one Source audition player: decodes a Browser sample off the message
    and audio callbacks, and mixes prepared blocks into the open audio device
    at a fixed −6 dB safety gain.

    This is not Suggestion Audition. The Browser talks to it only through
    SourceAudition, which names no engine type. The mix() entry is the
    real-time callback: tests drive it by offline rendering, which is the
    existing RTSan probe pattern (ADR 0006).
*/
class SourceAuditionPlayer final : public duet::gui::SourceAudition
{
public:
    using Work = std::function<void()>;
    using Executor = std::function<void (Work)>;
    using Poster = std::function<void (Work)>;

    /** Owns a worker thread and posts results with the message-thread poster
        the host supplies — the same shape as SampleFolderScanner.
    */
    explicit SourceAuditionPlayer (Poster postToMessageThread);

    /** Test seam: the executor runs decode when the test says so, never on a
        sleep, and the poster is how a result reaches the message thread.
    */
    SourceAuditionPlayer (Executor execute, Poster postToMessageThread);

    ~SourceAuditionPlayer() override;

    SourceAuditionPlayer (const SourceAuditionPlayer&) = delete;
    SourceAuditionPlayer& operator= (const SourceAuditionPlayer&) = delete;

    void play (std::filesystem::path file, std::string identity) override;
    void stop() override;
    [[nodiscard]] duet::gui::SourceAuditionStatus status() const override;
    void onChanged (std::function<void()> callback) override;

    /** The rate and block the callback will be asked for. A test that mixes
        without a device says so here; the open audio device says so when it
        starts.
    */
    void prepare (double sampleRate, int maximumBlockSize);

    /** Mixes the prepared Source audition into the caller's buffer, adding, at
        sourceAuditionGain. No-op when nothing is playing. The real-time entry:
        noexcept, annotated, allocation-free.
    */
    void mix (float* const* outputs, int numChannels, int numSamples) noexcept DUET_NONBLOCKING;

    /** Sums this player's mix into the session's open audio device, after the
        engine, so Source audition is heard at Main Output and is never an
        input that can be recorded.
    */
    void attachToOutput (duet::model::Session& session);
    void detachFromOutput();

    /** Fixed safety gain: −6 dB. */
    static constexpr float sourceAuditionGain = 0.501187F;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::app
