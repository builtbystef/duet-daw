#pragma once

#include <duet/model/Session.h>
#include <duet/persistence/DataNode.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

/** The persistence facade: a project's folder, the model that edits it, and
    what it takes to get that model onto disk and back (ADR 0005).

    Same engine-free rule as duet_model: nothing here names an engine or JUCE
    type.
*/
namespace duet::persistence
{
/** The name the interface's per-project view state has in the DUET tree, as
    spec 535bbo settled it. Part of the project format, so it lives here rather
    than with the view-model that fills it in.
*/
inline constexpr std::string_view viewTreeName = "VIEW";

/** The project-format version written by this build. */
inline constexpr int currentSchemaVersion = 2;

/** How often a dirty project writes its one recovery snapshot. The values are
    deliberately closed: they are the four choices in Settings > Interface.
*/
enum class AutosaveInterval : std::uint8_t
{
    off,
    twoMinutes,
    fiveMinutes,
    tenMinutes
};

inline constexpr auto defaultAutosaveInterval = AutosaveInterval::tenMinutes;

/** Which durable state to open after the producer answers a recovery offer. */
enum class RecoveryChoice : std::uint8_t
{
    decline,
    restore
};

struct ProjectOpenResult;

/** One open project. */
class Project
{
public:
    /** Creates a project and opens it: the folder, its audio subdirectory, and
        an edit file written straight away — so that a project that has been
        created is a project on disk before it is edited.

        Null when the folder cannot be made, or already holds a project.
    */
    [[nodiscard]] static std::unique_ptr<Project> create (const std::filesystem::path& folder);

    /** Opens the project a folder holds. Null when there is none to read or its
        schema needs a newer Duet. */
    [[nodiscard]] static std::unique_ptr<Project> open (const std::filesystem::path& folder);

    /** Opens with the reason for a refusal. Migrations finish before the
        returned project can expose any of the DUET tree to a caller. */
    [[nodiscard]] static ProjectOpenResult
        openWithResult (const std::filesystem::path& folder,
                        RecoveryChoice recoveryChoice = RecoveryChoice::decline);

    /** True when one newer recovery snapshot can be offered for this project. */
    [[nodiscard]] static bool recoveryAvailable (const std::filesystem::path& folder);

    ~Project();

    Project (const Project&) = delete;
    Project& operator= (const Project&) = delete;

    /** The folder that is this project. */
    [[nodiscard]] const std::filesystem::path& folder() const { return projectFolder; }

    /** The model that edits this project. */
    [[nodiscard]] duet::model::Session& session() const { return *editSession; }

    /** Writes the project to its folder.

        A snapshot, never the engine's own flush-and-save path: the state is
        copied, the plugins' parameter blobs are applied to the copy with no
        undo history in the way, and the copy is what gets written. The engine's
        path puts those blobs through the undo history, which drops the redo
        stack exactly when automation has driven a parameter away from the value
        the producer set (ADR 0005, hazards 3 and 4).

        The write lands beside the project file and is renamed onto it, so a
        save that dies halfway leaves the last saved project intact.

        False when nothing could be written; the project on disk is untouched.
    */
    bool save();

    /** True when the project has changed since it was last saved.

        Every Action raises it and a save clears it. Undoing back to the state
        that was saved does not: what it reports is that changes have happened,
        not that the state differs from the file.
    */
    [[nodiscard]] bool hasUnsavedChanges() const { return unsavedChanges; }

    //==============================================================================
    /** Changes how often this project writes recovery snapshots. The optional
        time is the scheduling seam: the app uses the steady clock, while tests
        can advance it without waiting for minutes.
    */
    void setAutosaveInterval (
        AutosaveInterval interval,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    [[nodiscard]] AutosaveInterval autosaveInterval() const { return autosaveEvery; }

    /** Gives the autosave scheduler its current time. Returns true only when an
        elapsed, enabled interval wrote the dirty project to recovery.
    */
    bool
        autosaveTick (std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    /** Copies an audio file into the project's audio subdirectory and returns
        the copy, so that a clip inserted from it travels with the folder. A
        file already in the project is returned as it is; a file whose name is
        already taken replaces what is there. Empty when the copy failed.
    */
    std::filesystem::path importAudioFile (const std::filesystem::path& sourceFile);

    /** Writes one of Duet's own facts about this project.

        These live in the project file beside the engine's own data, in the DUET
        tree that is Duet's half of the format, and they are saved and reloaded
        with it (ADR 0005). They are not producer edits, so they stay out of the
        undo history: an undo never takes one back.
    */
    void setDuetValue (std::string_view key, std::string_view value);

    /** Reads one of Duet's own facts. Empty when the project holds none. */
    [[nodiscard]] std::string duetValue (std::string_view key) const;

    //==============================================================================
    /** Says where the interface's per-project view state is to be read from.

        The view — zoom, scroll, which docks are open and how wide — belongs to
        the project and comes back with it, but it is not a producer edit. So it
        is not written when it changes: it is asked for here, once, as a save
        begins, and written into the DUET tree with no undo history (ADR 0005,
        spec 535bbo). That is what makes dragging a divider something an undo
        never puts back, and something that never makes a project dirty.

        One reader at a time. Without one, a save leaves whatever view the
        project already held alone.
    */
    void onCaptureViewState (std::function<DataNode()> capture);

    /** The view the project holds. A project saved before the interface existed,
        or by nothing that had a view to give, gives back an empty VIEW node —
        which is what the interface reads its own defaults from.
    */
    [[nodiscard]] DataNode viewState() const;

private:
    Project (std::filesystem::path folder,
             std::unique_ptr<duet::model::Session> session,
             bool migrated = false);

    bool writeSnapshot (const std::filesystem::path& destination,
                        const std::filesystem::path& partial);
    void writeViewState();

    std::filesystem::path projectFolder;
    std::unique_ptr<duet::model::Session> editSession;
    std::function<DataNode()> captureViewState;
    std::chrono::steady_clock::time_point lastAutosaveTick = std::chrono::steady_clock::now();
    AutosaveInterval autosaveEvery = defaultAutosaveInterval;
    bool unsavedChanges = false;
};

/** The result of trying to open a project. A refusal has no project and carries
    the producer-facing reason; success has a project and no message. */
struct ProjectOpenResult
{
    std::unique_ptr<Project> project;
    std::string message;
    bool recoveryAvailable = false;
};
} // namespace duet::persistence
