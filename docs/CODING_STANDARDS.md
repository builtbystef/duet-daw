# Coding standards

The conventions that this project holds, beyond what linters and formatters enforce. Reviews check diffs against this file. Keep each rule current, or delete it.

## Dependencies

- Prefer what the project already has: an installed library, or the standard library, before a new dependency.
- A new production dependency needs a stated reason, in the issue that adds it. A new dependency is never the default answer to a small problem.

## Real-time audio

These rules apply to Duet's audio callbacks and every function they call. Tracktion Engine is trusted outside that boundary; Duet's built-in instruments and effects are not.

- Audio-callback work must be bounded and use memory prepared before playback. Do not allocate, free, resize a dynamic container, construct an owning object that may allocate, or let such an object be destroyed in the callback.
- Do not acquire a lock, wait on a condition or semaphore, sleep, yield, perform a blocking atomic retry loop, or call code that may do any of those things. Cross-thread atomics must be always lock-free; enforce that property with `static_assert`.
- Do not perform file, network, console, environment, process, device, or other operating-system I/O. Logging and error reporting must leave the callback through a preallocated, bounded, lock-free queue.
- Do not touch the project model, `juce::ValueTree`, undo state, UI objects, or listener/message dispatch from the callback. Exchange only fixed-size data through preallocated, bounded, lock-free structures; define an explicit overflow policy that never waits.
- Callback entry points must be `noexcept` and carry `[[clang::nonblocking]]` in supporting Clang builds through a feature-tested project macro. The RealtimeSanitizer test must enter through that annotation and exercise the callback by offline rendering.
- RealtimeSanitizer is a dynamic backstop, not a waiver from review: it checks only executed paths and its intercepted or annotated blocking operations. Do not add a suppression for Duet code or a broad vendored-tree/frame pattern. An exact upstream-function suppression needs a documented false positive and must not hide a violation reached from Duet's callback.
