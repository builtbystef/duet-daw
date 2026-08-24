#include <duet/gui/ProjectsSettings.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("the projects directory is app-global and starts at the supplied default")
{
    duet::testing::StoredSettings settings;
    const std::filesystem::path defaultDirectory { "/home/producer/Music/Duet Projects" };
    const std::filesystem::path chosenDirectory { "/studio/Songs" };

    REQUIRE (duet::gui::projectsDirectory (settings, defaultDirectory) == defaultDirectory);

    duet::gui::setProjectsDirectory (settings, chosenDirectory);

    REQUIRE (duet::gui::projectsDirectory (settings, defaultDirectory) == chosenDirectory);
}
