#pragma once

#include <duet/collab/ProjectTools.h>

#include <functional>

namespace duet::app
{
/** Answers whether the calling thread is the message thread. */
using OnMessageThread = std::function<bool()>;

/** Posts work to the message thread and returns without waiting for it. */
using MessageThreadPoster = std::function<void (std::function<void()>)>;

/** A marshal that runs a project read on the message thread and waits for it,
    for no longer than `timeoutMs`.

    The bound is not optional: the caller is the Collaborator service's thread,
    and a read that never comes back is a run that never ends. A modal file
    chooser, a long save, a plugin scan or a slow render all park the message
    thread for longer than a read may wait.

    What makes the bound safe is that an expired wait *abandons* the read rather
    than merely stopping waiting for it. A read holds references into the frame
    that is waiting — that is how its answer gets back — so a posted call that
    ran after the wait expired would write into a frame that has returned. Under
    this protocol a read that has not started never starts, and one that is
    already running is waited for, which is the only part that cannot be
    abandoned safely.

    A read that was abandoned answers nothing, which the caller reads as the
    project not having been readable.
*/
[[nodiscard]] duet::collab::ProjectReadMarshal
    boundedMarshal (OnMessageThread onMessageThread, MessageThreadPoster post, int timeoutMs);
} // namespace duet::app
