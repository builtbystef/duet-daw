#pragma once

#include <functional>
#include <memory>

namespace duet::gui
{
class Browser;
}

namespace duet::app
{
/** Walks the Browser's sample folders off the message thread.

    A refresh asks this to scan; progress and the finished walk are posted back
    onto the message thread and installed only if their generation is still the
    current one. Destruction cancels in-flight work and delivers nothing into a
    Browser that has already gone.
*/
class SampleFolderScanner
{
public:
    using Work = std::function<void()>;
    using Executor = std::function<void (Work)>;
    using Poster = std::function<void (Work)>;

    /** Owns a worker thread and posts results with `postToMessageThread`. */
    explicit SampleFolderScanner (Poster postToMessageThread);

    /** Test seam: the executor runs scan work when the test says so, never on a
        sleep, and the poster is how a result reaches the message thread.
    */
    SampleFolderScanner (Executor execute, Poster postToMessageThread);

    ~SampleFolderScanner();

    SampleFolderScanner (const SampleFolderScanner&) = delete;
    SampleFolderScanner& operator= (const SampleFolderScanner&) = delete;

    void attach (duet::gui::Browser& browser);
    void detach();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::app
