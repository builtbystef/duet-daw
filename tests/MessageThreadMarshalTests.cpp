// The marshal a project read reaches the message thread through, and the one
// thing about it that is not obvious: its wait is bounded, so it must be able
// to say that a read will not happen — and say it before the frame the read
// would have written into has gone.

#include <duet/app/MessageThreadMarshal.h>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>
#include <thread>

using duet::app::boundedMarshal;

namespace
{
/** A message thread that runs nothing until it is told to. */
struct HeldQueue
{
    std::function<void()> posted;

    void run() const
    {
        if (posted)
            posted();
    }
};
} // namespace

TEST_CASE ("a read on the message thread runs where it stands", "[marshal]")
{
    HeldQueue queue;
    auto ran = 0;

    const auto marshal =
        boundedMarshal ([] { return true; },
                        [&queue] (std::function<void()> work) { queue.posted = std::move (work); },
                        50);

    marshal ([&ran] { ++ran; });

    // Nothing was posted: the caller was already where the work belongs.
    REQUIRE (ran == 1);
    REQUIRE_FALSE (static_cast<bool> (queue.posted));
}

TEST_CASE ("a read posted from another thread runs once and is waited for", "[marshal]")
{
    HeldQueue queue;
    std::string wrote;

    const auto marshal =
        boundedMarshal ([] { return false; },
                        [&queue] (std::function<void()> work) { queue.posted = std::move (work); },
                        5000);

    std::thread reader { [&marshal, &wrote] { marshal ([&wrote] { wrote = "the project"; }); } };

    // The reader is waiting on the queue, so running it is what lets it go.
    while (! queue.posted)
        std::this_thread::yield();

    queue.run();
    reader.join();

    REQUIRE (wrote == "the project");
}

TEST_CASE ("a read that did not run in time is abandoned, and never runs after", "[marshal]")
{
    HeldQueue queue;
    auto ran = 0;

    const auto marshal =
        boundedMarshal ([] { return false; },
                        [&queue] (std::function<void()> work) { queue.posted = std::move (work); },
                        50);

    // The work is never run while the reader waits, which is a message thread
    // parked in a modal dialog, a save, a scan or a render.
    std::thread reader { [&marshal, &ran] { marshal ([&ran] { ++ran; }); } };

    reader.join();

    // The wait expired and the read did not happen, which is what the caller
    // reads as a project it could not read.
    REQUIRE (ran == 0);

    // The message thread gets to the call afterwards, as it always would. The
    // frame that posted it has returned, so the one safe thing it can do is
    // nothing — and that is the whole point of the protocol.
    REQUIRE (static_cast<bool> (queue.posted));
    queue.run();

    REQUIRE (ran == 0);
}
