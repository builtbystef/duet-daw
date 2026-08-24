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

/** The file a project keeps its state in: the engine's edit and, inside it,
    Duet's own DUET tree. Everything the project stores relative paths against.
*/
std::filesystem::path editFile (const std::filesystem::path& projectFolder);

/** Where a save writes before it renames the result onto the edit file.

    A save that dies halfway leaves this file behind and the project file as it
    was, so a file at this path is the remains of an interrupted save and never
    a project.
*/
std::filesystem::path partialSaveFile (const std::filesystem::path& projectFolder);

/** The one crash-recovery snapshot beside the project file. */
std::filesystem::path recoveryFile (const std::filesystem::path& projectFolder);

/** Where an autosave is completed before it atomically replaces recovery. */
std::filesystem::path partialRecoveryFile (const std::filesystem::path& projectFolder);
} // namespace duet::persistence
