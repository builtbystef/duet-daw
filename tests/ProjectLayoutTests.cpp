#include <duet/persistence/ProjectLayout.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("a project's recordings and imports live in its audio subdirectory")
{
    const std::filesystem::path project { "/home/producer/Tracks/Nocturne" };

    REQUIRE (duet::persistence::audioDirectory (project)
             == std::filesystem::path { "/home/producer/Tracks/Nocturne/audio" });
}
