#pragma once

/** The audio-callback annotation, in the one place it is feature-tested.

    An audio callback may not allocate, lock, wait, or reach the operating
    system — the rules are the Real-time audio section of
    `docs/CODING_STANDARDS.md`, and review is what enforces them.
    RealtimeSanitizer is the dynamic backstop behind that review, and this macro
    is how a callback tells it where the real-time context begins: the
    instrumentation the sanitizer inserts goes into the annotated function
    itself, so an entry point that carries the annotation puts every frame below
    it under the rule, however far down the engine's uninstrumented call comes
    from.

    Only Clang 20.1 and newer know the attribute; on every other compiler this
    expands to nothing, which is what keeps the same declaration compiling on
    the GCC 13 floor and on Clang 18. The sanitizer itself arrives through the
    `duet::realtime` target, in the `linux-rtsan` configuration only.

    The attribute appertains to the function type, so it goes after the
    parameter list and its `noexcept`, and before `override`:

    @code
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) noexcept
        DUET_NONBLOCKING override;
    @endcode

    It must be repeated on every redeclaration of the same function, a
    definition written apart from its declaration included. `nonblocking` does
    not imply `noexcept`, which is why the standard asks for both.
*/

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(clang::nonblocking)
#define DUET_NONBLOCKING [[clang::nonblocking]]
#endif
#endif

#ifndef DUET_NONBLOCKING
#define DUET_NONBLOCKING
#endif
