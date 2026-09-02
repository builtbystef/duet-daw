#include <duet/app/SourceAuditionPlayer.h>
#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::app::SourceAuditionPlayer;
using duet::gui::SourceAuditionState;
using duet::model::Session;
using duet::testing::TempProject;

namespace
{
/** Queues work until the test runs it. Never sleeps. */
class CommandableExecutor
{
public:
    void operator() (std::function<void()> work) { queue.push_back (std::move (work)); }

    [[nodiscard]] std::size_t pending() const { return queue.size(); }

    void runNext()
    {
        REQUIRE_FALSE (queue.empty());
        auto work = std::move (queue.front());
        queue.erase (queue.begin());
        work();
    }

private:
    std::vector<std::function<void()>> queue;
};

constexpr double toneHz = 440.0;
constexpr double toneSeconds = 0.5;
constexpr int mixChannels = 2;
constexpr int mixBlock = 512;
constexpr double mixRate = 44100.0;

/** Peak of writeTone's sine after the −6 dB safety gain. */
const float expectedPeak = 0.5F * SourceAuditionPlayer::sourceAuditionGain;

struct CommandablePlayer
{
    CommandablePlayer()
        : player ([this] (auto work) { worker (std::move (work)); }, [] (auto work) { work(); })
    {
        player.prepare (mixRate, mixBlock);
    }

    void load (const std::filesystem::path& file, std::string identity)
    {
        player.play (file, std::move (identity));
        REQUIRE (player.status().state == SourceAuditionState::loading);
        REQUIRE (worker.pending() == 1);
        worker.runNext();
    }

    CommandableExecutor worker;
    SourceAuditionPlayer player;
};

/** Mixes silence-filled blocks through the player and keeps the left channel. */
[[nodiscard]] std::vector<float> capture (SourceAuditionPlayer& player, int frames)
{
    std::vector<float> left (static_cast<std::size_t> (frames), 0.0F);
    std::vector<float> right (static_cast<std::size_t> (frames), 0.0F);
    auto remaining = frames;
    auto offset = 0;

    while (remaining > 0)
    {
        const auto count = std::min (remaining, mixBlock);
        std::array<float*, mixChannels> channels { std::next (left.data(), offset),
                                                   std::next (right.data(), offset) };
        player.mix (channels.data(), mixChannels, count);
        offset += count;
        remaining -= count;
    }

    return left;
}

[[nodiscard]] float peakOf (const std::vector<float>& samples)
{
    auto peak = 0.0F;

    for (const auto sample : samples)
        peak = std::max (peak, std::abs (sample));

    return peak;
}

[[nodiscard]] double pitchHzOf (const std::vector<float>& samples)
{
    if (samples.size() < 2)
        return 0.0;

    int crossings = 0;
    auto previous = samples.front();

    for (std::size_t index = 1; index < samples.size(); ++index)
    {
        const auto current = samples[index];

        if (previous <= 0.0F && current > 0.0F)
            ++crossings;

        previous = current;
    }

    const auto seconds = static_cast<double> (samples.size()) / mixRate;
    return seconds > 0.0 ? static_cast<double> (crossings) / seconds : 0.0;
}
} // namespace

TEST_CASE (
    "Source audition loads, plays, reports progress, and stops without touching transport or undo")
{
    const TempProject project;
    const Session session { project.editFile() };
    const auto tone = project.writeTone ("tone.wav", toneSeconds, toneHz);
    CommandablePlayer loaded;
    const auto actionsBefore = session.undoNames().size();
    const auto playingBefore = session.isPlaying();
    const auto positionBefore = session.playbackPositionSeconds();

    loaded.load (tone, "sample:tone");

    REQUIRE (loaded.player.status().state == SourceAuditionState::playing);
    REQUIRE (loaded.player.status().identity == "sample:tone");
    REQUIRE_THAT (loaded.player.status().progress, WithinAbs (0.0, 0.001));

    const auto heard = capture (loaded.player, mixBlock);
    REQUIRE (loaded.player.status().state == SourceAuditionState::playing);
    REQUIRE (loaded.player.status().progress > 0.0);
    REQUIRE (loaded.player.status().progress < 1.0);
    REQUIRE (peakOf (heard) > 0.0F);

    loaded.player.stop();
    REQUIRE (loaded.player.status().state == SourceAuditionState::stopped);
    REQUIRE (loaded.player.status().identity.empty());
    REQUIRE_THAT (loaded.player.status().progress, WithinAbs (0.0, 0.001));

    REQUIRE (session.undoNames().size() == actionsBefore);
    REQUIRE (session.isPlaying() == playingBefore);
    REQUIRE_THAT (session.playbackPositionSeconds(), WithinAbs (positionBefore, 0.0001));
}

TEST_CASE ("only one Source audition plays: a second play replaces the first")
{
    const TempProject project;
    const Session session { project.editFile() };
    REQUIRE_FALSE (session.isPlaying());
    const auto first = project.writeTone ("first.wav", toneSeconds, toneHz);
    const auto second = project.writeTone ("second.wav", toneSeconds, 880.0);
    CommandablePlayer loaded;

    loaded.load (first, "sample:first");
    loaded.load (second, "sample:second");

    REQUIRE (loaded.player.status().identity == "sample:second");
    REQUIRE (loaded.player.status().state == SourceAuditionState::playing);

    const auto heard = capture (loaded.player, static_cast<int> (mixRate * 0.2));
    REQUIRE_THAT (pitchHzOf (heard), WithinAbs (880.0, 8.0));
}

TEST_CASE ("an unreadable file is a row-local error and never disturbs project audio")
{
    const TempProject project;
    const Session session { project.editFile() };
    CommandablePlayer loaded;
    const auto missing = project.folder() / "missing.wav";
    const auto actionsBefore = session.undoNames().size();

    loaded.load (missing, "sample:missing");

    REQUIRE (loaded.player.status().state == SourceAuditionState::error);
    REQUIRE (loaded.player.status().error == "Could not play this file");
    REQUIRE (loaded.player.status().identity == "sample:missing");

    const auto heard = capture (loaded.player, mixBlock);
    REQUIRE (peakOf (heard) == 0.0F);
    REQUIRE (session.undoNames().size() == actionsBefore);
    REQUIRE_FALSE (session.isPlaying());
}

/** The real-time backstop, entered from outside.

    mix() is the DUET_NONBLOCKING entry. These cases drive it by offline
    rendering — filling buffers on this thread — which is the shape the
    linux-rtsan nightly needs (ADR 0006). A violation inside mix ends the run
    rather than the assertion; a mix that is never entered would look like a
    green nightly, so the feature assertions below are also the proof it ran.
*/
TEST_CASE ("Source audition hears the known tone at -6 dB, then silence after every stop path")
{
    const TempProject project;
    const Session session { project.editFile() };
    REQUIRE_FALSE (session.isPlaying());
    const auto tone = project.writeTone ("tone.wav", 1.0, toneHz);
    const auto other = project.writeTone ("other.wav", 1.0, 880.0);
    CommandablePlayer loaded;

    loaded.load (tone, "sample:tone");

    const auto playing = capture (loaded.player, static_cast<int> (mixRate * 0.25));
    REQUIRE_THAT (pitchHzOf (playing), WithinAbs (toneHz, 4.0));
    REQUIRE_THAT (static_cast<double> (peakOf (playing)),
                  WithinAbs (static_cast<double> (expectedPeak), 0.02));

    loaded.player.stop();
    auto afterStop = capture (loaded.player, mixBlock);
    REQUIRE (peakOf (afterStop) == 0.0F);

    loaded.load (tone, "sample:tone");
    static_cast<void> (capture (loaded.player, mixBlock));
    loaded.load (other, "sample:other");
    const auto replaced = capture (loaded.player, static_cast<int> (mixRate * 0.25));
    REQUIRE_THAT (pitchHzOf (replaced), WithinAbs (880.0, 8.0));

    loaded.player.stop();
    afterStop = capture (loaded.player, mixBlock);
    REQUIRE (peakOf (afterStop) == 0.0F);

    loaded.load (tone, "sample:tone");
    const auto frames = static_cast<int> (mixRate * 1.0) + mixBlock;
    const auto throughEnd = capture (loaded.player, frames);
    REQUIRE (loaded.player.status().state == SourceAuditionState::stopped);
    const auto tail = std::vector<float> (throughEnd.end() - mixBlock, throughEnd.end());
    REQUIRE (peakOf (tail) == 0.0F);
}
