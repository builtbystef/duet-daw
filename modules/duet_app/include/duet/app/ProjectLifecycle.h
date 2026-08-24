#pragma once

#include <duet/persistence/Project.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace duet::gui
{
class Settings;
}

namespace duet::app
{
/** The producer's answer when leaving a dirty project. */
enum class UnsavedDecision : std::uint8_t
{
    save,
    discard,
    cancel
};

/** The paintless project policy hosted by duet_app.

    It owns the one project that is always open, while dialogs and components
    remain in the GUI shell. Tests drive this boundary with the same answers the
    Save / Discard / Cancel prompt supplies.
*/
class ProjectLifecycle
{
public:
    ProjectLifecycle (duet::gui::Settings& settings,
                      std::filesystem::path defaultProjectsDirectory);
    ~ProjectLifecycle();

    ProjectLifecycle (const ProjectLifecycle&) = delete;
    ProjectLifecycle& operator= (const ProjectLifecycle&) = delete;

    /** Opens the last project when it remains readable, otherwise creates the
        next Untitled project. */
    bool launch (duet::persistence::RecoveryChoice recoveryChoice =
                     duet::persistence::RecoveryChoice::decline);

    /** The last project launch will try, when it still exists. */
    [[nodiscard]] std::optional<std::filesystem::path> startupProjectFolder() const;

    /** Replaces the open project with the next numbered Untitled project after
        resolving any unsaved changes. */
    bool createNew (UnsavedDecision decision);

    /** Opens a project folder after resolving any unsaved changes. */
    bool open (const std::filesystem::path& folder,
               duet::persistence::RecoveryChoice recoveryChoice,
               UnsavedDecision decision);

    bool save();

    /** Continues in a whole-folder copy under the destination name. */
    bool saveAs (const std::filesystem::path& destinationFolder);

    /** True when closing may proceed after the supplied prompt answer. */
    bool mayClose (UnsavedDecision decision);

    [[nodiscard]] duet::persistence::Project& project() const { return *openProject; }
    [[nodiscard]] duet::persistence::Project* projectOrNull() const { return openProject.get(); }
    [[nodiscard]] std::string projectName() const;

    /** Existing recent projects, newest first. Missing folders are removed from
        the stored list as it is read. */
    [[nodiscard]] std::vector<std::filesystem::path> recentProjects();

    [[nodiscard]] const std::string& lastError() const { return error; }

private:
    [[nodiscard]] std::unique_ptr<duet::persistence::Project> makeUntitled();
    bool resolveUnsavedChanges (UnsavedDecision decision);
    void install (std::unique_ptr<duet::persistence::Project> project);
    void remember (const std::filesystem::path& folder);
    void storeRecent (const std::vector<std::filesystem::path>& folders);

    duet::gui::Settings& settings;
    std::filesystem::path defaultDirectory;
    std::unique_ptr<duet::persistence::Project> openProject;
    std::string error;
};
} // namespace duet::app
