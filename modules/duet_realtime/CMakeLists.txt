# duet_realtime — the audio-callback seam, and the marker for what is under the
# real-time rule.
#
# Two things travel together here, because they are the same decision. The
# header carries DUET_NONBLOCKING, the feature-tested annotation a callback
# entry point wears; the target carries -fsanitize=realtime in the linux-rtsan
# configuration, which is what makes that annotation mean something at run time.
#
# So a Duet target that holds audio-callback code links duet::realtime, and
# linking it is what puts the target under the sanitizer. The list of callback
# targets is therefore not written down anywhere to fall out of date: it is
# every target that links this one, and `git grep duet::realtime` is the list.
add_library(duet_realtime INTERFACE)
add_library(duet::realtime ALIAS duet_realtime)

target_include_directories(duet_realtime INTERFACE include)

if(DUET_RTSAN)
    # Compile and link both: the instrumentation goes into the annotated
    # translation unit, and the runtime it calls into the executable.
    # Frame pointers are what turn a report into a stack that names the
    # callback that broke the rule.
    target_compile_options(duet_realtime INTERFACE -fsanitize=realtime -fno-omit-frame-pointer)
    target_link_options(duet_realtime INTERFACE -fsanitize=realtime)
endif()
