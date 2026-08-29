#include <catch2/catch_test_macros.hpp>
#include <catch2/interfaces/catch_interfaces_reporter.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

#include <unistd.h>

namespace
{
/** Where this suite's app-global settings go, for the run of the suite.

    A component test that opens a project takes the engine with it, and the
    engine keeps app-global settings in the user's configuration directory.
    Without this the suite would write into the producer's own settings — so the
    run is pointed at a directory of its own, and takes it away afterwards. JUCE
    resolves the user's application data directory through XDG.
*/
class SettingsHome final : public Catch::EventListenerBase
{
public:
    using EventListenerBase::EventListenerBase;

    void testRunStarting (const Catch::TestRunInfo& run) override
    {
        static_cast<void> (run);
        directory = std::filesystem::temp_directory_path()
                    / ("duet-gui-test-settings-" + std::to_string (getpid()));
        std::filesystem::remove_all (directory);
        std::filesystem::create_directories (directory);

        // Catch2 runs this before any test, and so before any thread of the
        // suite's own exists.
        setenv ("XDG_CONFIG_HOME", directory.c_str(), 1); // NOLINT(concurrency-mt-unsafe)
    }

    void testRunEnded (const Catch::TestRunStats& stats) override
    {
        static_cast<void> (stats);
        std::filesystem::remove_all (directory);
    }

private:
    std::filesystem::path directory;
};
} // namespace

// Catch2 has one way to register a listener, and it is an object with static
// storage duration whose constructor allocates.
CATCH_REGISTER_LISTENER (SettingsHome) // NOLINT(cert-err58-cpp)
