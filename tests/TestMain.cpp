#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_session.hpp>

#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>

#include <unistd.h>

namespace
{
class TestSettingsHome
{
public:
    TestSettingsHome()
    {
        // Process startup is single-threaded here.
        if (const auto* inherited =
                std::getenv (settingsHomeVariable)) // NOLINT(concurrency-mt-unsafe)
        {
            directory = inherited;
        }
        else
        {
            ownsDirectory = true;
            directory = std::filesystem::temp_directory_path()
                        / ("duet-test-settings-" + std::to_string (getpid()));
            std::filesystem::remove_all (directory);
            std::filesystem::create_directories (directory);
            setenv (settingsHomeVariable, directory.c_str(), 1); // NOLINT(concurrency-mt-unsafe)
        }

        // A scanner child inherits the marker and therefore reads the same
        // app-global store as its parent without touching the producer's real
        // settings. JUCE resolves userApplicationDataDirectory through XDG.
        setenv ("XDG_CONFIG_HOME", directory.c_str(), 1); // NOLINT(concurrency-mt-unsafe)
    }

    ~TestSettingsHome()
    {
        if (ownsDirectory)
            std::filesystem::remove_all (directory);
    }

    TestSettingsHome (const TestSettingsHome&) = delete;
    TestSettingsHome& operator= (const TestSettingsHome&) = delete;

private:
    static constexpr const char* settingsHomeVariable = "DUET_TEST_SETTINGS_HOME";

    std::filesystem::path directory;
    bool ownsDirectory = false;
};
} // namespace

int main (int argc, char* argv[])
{
    const TestSettingsHome settingsHome;
    std::string commandLine;

    const std::span arguments { argv, static_cast<std::size_t> (argc) };
    bool first = true;

    for (auto* argument : arguments.subspan (1))
    {
        if (! first)
            commandLine += ' ';

        commandLine += argument;
        first = false;
    }

    if (duet::model::Session::startPluginScanChild (commandLine))
        for (;;)
            duet::testing::pumpMessages (1000);

    return Catch::Session {}.run (argc, argv);
}
