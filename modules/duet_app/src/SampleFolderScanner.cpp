#include <duet/app/SampleFolderScanner.h>

#include <duet/gui/Browser.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace duet::app
{
namespace
{
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
} // namespace

struct SampleFolderScanner::Impl
{
    struct Binding
    {
        duet::gui::Browser* browser = nullptr;
    };

    Impl (Executor executeOnWorker, Poster postToMessageThread)
        : execute (std::move (executeOnWorker)), post (std::move (postToMessageThread))
    {
    }

    void attach (duet::gui::Browser& browser)
    {
        detach();
        binding->browser = &browser;
        browser.setScanWorker ([this] (const duet::gui::SampleFolderScanRequest& request)
                               { start (request); });
    }

    void detach()
    {
        currentGeneration.fetch_add (1, std::memory_order_relaxed);

        if (binding->browser == nullptr)
            return;

        binding->browser->setScanWorker ({});
        binding->browser = nullptr;
    }

    [[nodiscard]] bool stale (std::uint64_t generation) const
    {
        return stopping.load (std::memory_order_relaxed)
               || currentGeneration.load (std::memory_order_relaxed) != generation;
    }

    void start (const duet::gui::SampleFolderScanRequest& request)
    {
        currentGeneration.store (request.generation, std::memory_order_relaxed);
        const auto generation = request.generation;
        const auto folders = request.folders;
        auto bound = binding;
        auto poster = post;

        execute (
            [this, generation, folders, bound, poster]
            {
                if (stale (generation))
                    return;

                duet::gui::SampleFolderScanOutcome outcome;
                outcome.generation = generation;

                const auto known = folders.size();
                poster (
                    [bound, generation, known]
                    {
                        if (bound->browser != nullptr)
                            bound->browser->applyScanProgress ({ generation, 0, known });
                    });

                std::size_t completed = 0;

                for (const auto& folder : folders)
                {
                    if (stale (generation))
                        return;

                    outcome.folders.push_back (duet::gui::scanSampleFolder (
                        folder, [this, generation] { return stale (generation); }));
                    ++completed;

                    poster (
                        [bound, generation, completed, known]
                        {
                            if (bound->browser != nullptr)
                                bound->browser->applyScanProgress (
                                    { generation, completed, known });
                        });
                }

                if (stale (generation))
                    return;

                poster (
                    [bound, outcome = std::move (outcome)]
                    {
                        if (bound->browser != nullptr)
                            bound->browser->applyScanOutcome (outcome);
                    });
            });
    }

    std::shared_ptr<Binding> binding = std::make_shared<Binding>();
    std::atomic<std::uint64_t> currentGeneration { 0 };
    std::atomic<bool> stopping { false };
    Poster post;
    Executor execute;
    std::unique_ptr<ThreadQueue> owned;
};

SampleFolderScanner::SampleFolderScanner (Poster postToMessageThread)
{
    auto queue = std::make_unique<ThreadQueue>();
    auto* queuePtr = queue.get();
    impl = std::make_unique<Impl> ([queuePtr] (Work work) { queuePtr->post (std::move (work)); },
                                   std::move (postToMessageThread));
    impl->owned = std::move (queue);
}

SampleFolderScanner::SampleFolderScanner (Executor execute, Poster postToMessageThread)
    : impl (std::make_unique<Impl> (std::move (execute), std::move (postToMessageThread)))
{
}

SampleFolderScanner::~SampleFolderScanner()
{
    if (impl == nullptr)
        return;

    impl->stopping.store (true, std::memory_order_relaxed);
    impl->detach();
    impl->owned.reset();
}

void SampleFolderScanner::attach (duet::gui::Browser& browser) { impl->attach (browser); }

void SampleFolderScanner::detach() { impl->detach(); }
} // namespace duet::app
