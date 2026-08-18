#pragma once

#include <filesystem>

/** Project folder shape. A project is a folder: the edit file plus an audio/
    subdirectory for recordings and imports, with paths stored project-relative
    (ADR 0005).
*/
namespace duet::persistence
{
/** The recordings-and-imports subdirectory of a project folder. */
std::filesystem::path audioDirectory (const std::filesystem::path& projectFolder);
} // namespace duet::persistence
