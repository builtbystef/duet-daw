#include <duet/app/MessageThreadMarshal.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>

namespace duet::app
{
namespace
{
    /** What one posted read needs, owned by the post and by the frame waiting
        for it together, so that neither reaches what the other has destroyed.
    */
    struct Posted
    {
        std::mutex lock;
        std::condition_variable ran;
        bool done = false;
        bool abandoned = false;
    };
} // namespace

duet::collab::ProjectReadMarshal
    boundedMarshal (OnMessageThread onMessageThread, MessageThreadPoster post, int timeoutMs)
{
    return [onMessageThread = std::move (onMessageThread), post = std::move (post), timeoutMs] (
               const std::function<void()>& work)
    {
        if (onMessageThread())
        {
            work();

            return;
        }

        const auto posted = std::make_shared<Posted>();

        // `work` is held by reference and not copied, because copying it would
        // not help: what it refers to is the waiting frame either way. The lock
        // and the flag are what keep it from being read after that frame goes.
        post (
            [posted, &work]
            {
                const std::lock_guard<std::mutex> held { posted->lock };

                if (posted->abandoned)
                    return;

                work();
                posted->done = true;
                posted->ran.notify_all();
            });

        std::unique_lock<std::mutex> waiting { posted->lock };

        if (posted->ran.wait_for (waiting,
                                  std::chrono::milliseconds { timeoutMs },
                                  [&posted] { return posted->done; }))
            return;

        posted->abandoned = true;
    };
}
} // namespace duet::app
