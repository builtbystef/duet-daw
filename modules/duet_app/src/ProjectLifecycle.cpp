#include <duet/app/ProjectLifecycle.h>

#include <duet/gui/AutosaveSettings.h>
#include <duet/gui/ProjectsSettings.h>
#include <duet/gui/Settings.h>
#include <duet/persistence/ProjectLayout.h>

#include <algorithm>
#include <string_view>
#include <utility>

namespace duet::app
{
namespace
{
    constexpr std::string_view lastProjectKey = "lastProjectFolder";
    constexpr std::string_view recentCountKey = "recentProjectCount";
    constexpr std::string_view recentPrefix = "recentProject";

    std::string recentKey (std::size_t index)
    {
        return std::string { recentPrefix } + std::to_string (index);
    }

    bool isProject (const std::filesystem::path& folder)
    {
        std::error_code failure;
        return std::filesystem::is_regular_file (duet::persistence::editFile (folder), failure);
    }
} // namespace

ProjectLifecycle::ProjectLifecycle (duet::gui::Settings& settingsStore,
                                    std::filesystem::path defaultProjectsDirectory)
    : settings (settingsStore), defaultDirectory (std::move (defaultProjectsDirectory))
{
}

ProjectLifecycle::~ProjectLifecycle() = default;

bool ProjectLifecycle::launch (duet::persistence::RecoveryChoice recoveryChoice)
{
    if (const auto folder = startupProjectFolder(); folder.has_value())
    {
        auto opened = duet::persistence::Project::openWithResult (*folder, recoveryChoice);

        if (opened.project != nullptr)
        {
            install (std::move (opened.project));
            return true;
        }
    }

    auto untitled = makeUntitled();

    if (untitled == nullptr)
        return false;

    install (std::move (untitled));
    return true;
}

std::optional<std::filesystem::path> ProjectLifecycle::startupProjectFolder() const
{
    const auto stored = settings.value (lastProjectKey);

    if (! stored.has_value())
        return std::nullopt;

    const std::filesystem::path folder { *stored };
    return isProject (folder) ? std::optional { folder } : std::nullopt;
}

bool ProjectLifecycle::createNew (UnsavedDecision decision)
{
    if (! resolveUnsavedChanges (decision))
        return false;

    auto untitled = makeUntitled();

    if (untitled == nullptr)
        return false;

    install (std::move (untitled));
    return true;
}

bool ProjectLifecycle::open (const std::filesystem::path& folder,
                             duet::persistence::RecoveryChoice recoveryChoice,
                             UnsavedDecision decision)
{
    if (! resolveUnsavedChanges (decision))
        return false;

    auto opened = duet::persistence::Project::openWithResult (folder, recoveryChoice);

    if (opened.project == nullptr)
    {
        error = std::move (opened.message);
        return false;
    }

    install (std::move (opened.project));
    return true;
}

bool ProjectLifecycle::save()
{
    if (openProject == nullptr || ! openProject->save())
    {
        error = "Could not save the project.";
        return false;
    }

    error.clear();
    return true;
}

bool ProjectLifecycle::saveAs (const std::filesystem::path& destinationFolder)
{
    if (openProject == nullptr)
        return false;

    auto copy = openProject->saveAs (destinationFolder);

    if (copy == nullptr)
    {
        error = "Could not copy the project there.";
        return false;
    }

    install (std::move (copy));
    return true;
}

bool ProjectLifecycle::mayClose (UnsavedDecision decision)
{
    return resolveUnsavedChanges (decision);
}

std::string ProjectLifecycle::projectName() const
{
    return openProject != nullptr ? openProject->folder().filename().string() : std::string {};
}

std::vector<std::filesystem::path> ProjectLifecycle::recentProjects()
{
    std::vector<std::filesystem::path> existing;
    const auto storedCount = settings.value (recentCountKey);
    std::size_t count = 0;

    if (storedCount.has_value())
    {
        try
        {
            count = static_cast<std::size_t> (std::stoull (*storedCount));
        }
        catch (...)
        {
            count = 0;
        }
    }

    for (std::size_t index = 0; index < count; ++index)
        if (const auto stored = settings.value (recentKey (index)); stored.has_value())
        {
            const std::filesystem::path folder { *stored };

            if (isProject (folder))
                existing.push_back (folder);
        }

    if (existing.size() != count)
        storeRecent (existing);

    return existing;
}

std::unique_ptr<duet::persistence::Project> ProjectLifecycle::makeUntitled()
{
    const auto parent = duet::gui::projectsDirectory (settings, defaultDirectory);

    for (std::size_t number = 1;; ++number)
    {
        const auto folder = parent / ("Untitled " + std::to_string (number));
        std::error_code failure;

        if (std::filesystem::exists (folder, failure))
            continue;
        if (failure)
        {
            error = "Could not inspect the projects directory.";
            return nullptr;
        }

        auto project = duet::persistence::Project::create (folder);

        if (project == nullptr)
        {
            error = "Could not create an untitled project.";
            return nullptr;
        }

        auto& session = project->session();
        const auto originalTracks = session.tracks();
        duet::model::TrackRef audioTrack = duet::model::noTrack;
        session.performAction ("Create Project",
                               [&originalTracks, &audioTrack] (auto& ops)
                               {
                                   for (const auto& track : originalTracks)
                                       ops.removeTrack (track.track);

                                   ops.createTrack (duet::model::TrackKind::midi,
                                                    "Instrument 1",
                                                    duet::model::BuiltinPlugin::synth);
                                   audioTrack =
                                       ops.createTrack (duet::model::TrackKind::audio, "Audio 1");
                               });

        // A new project's audio track is ready for its first take. Input
        // selection remains available through the recording vocabulary, but
        // the ordinary launch path chooses the machine's first audio input so
        // that Arm then Record needs no routing dialog.
        for (const auto& input : session.availableInputs())
            if (input.kind == duet::model::InputKind::audio)
            {
                session.setTrackInput (audioTrack, input.input);
                break;
            }

        if (! project->save())
        {
            std::filesystem::remove_all (folder, failure);
            error = "Could not seed an untitled project.";
            return nullptr;
        }

        return project;
    }
}

bool ProjectLifecycle::resolveUnsavedChanges (UnsavedDecision decision)
{
    if (openProject == nullptr || ! openProject->hasUnsavedChanges())
        return true;

    switch (decision)
    {
        case UnsavedDecision::save:
            return save();
        case UnsavedDecision::discard:
            return true;
        case UnsavedDecision::cancel:
            return false;
    }

    return false;
}

void ProjectLifecycle::install (std::unique_ptr<duet::persistence::Project> project)
{
    project->setAutosaveInterval (duet::gui::autosaveInterval (settings));
    openProject = std::move (project);
    remember (openProject->folder());
    error.clear();
}

void ProjectLifecycle::remember (const std::filesystem::path& folder)
{
    settings.setValue (lastProjectKey, folder.string());
    auto recent = recentProjects();
    recent.erase (std::remove (recent.begin(), recent.end(), folder), recent.end());
    recent.insert (recent.begin(), folder);
    storeRecent (recent);
}

void ProjectLifecycle::storeRecent (const std::vector<std::filesystem::path>& folders)
{
    settings.setValue (recentCountKey, std::to_string (folders.size()));

    for (std::size_t index = 0; index < folders.size(); ++index)
        settings.setValue (recentKey (index), folders.at (index).string());
}
} // namespace duet::app
