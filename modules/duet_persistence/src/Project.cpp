#include <duet/persistence/Project.h>

#include <duet/model/EngineAccess.h>
#include <duet/persistence/ProjectLayout.h>

#include <utility>
#include <vector>

namespace te = tracktion;

namespace duet::persistence
{
namespace
{
    /** Duet's half of the project file (ADR 0005). */
    constexpr const char* duetTreeName = "DUET";
    constexpr const char* schemaVersionName = "duetSchemaVersion";
    constexpr const char* layoutVersionName = "layoutVersion";

    juce::File toJuceFile (const std::filesystem::path& path)
    {
        return juce::File { juce::String { path.string() } };
    }

    juce::Identifier toIdentifier (std::string_view name)
    {
        return juce::Identifier { juce::String { std::string { name } } };
    }

    /** One of Duet's own nodes, as the project file stores it.

        Walked with a stack of its own rather than by recursion, which is the
        rule the search below is written to as well. A `juce::ValueTree` is a
        handle onto a shared node, so a child can be appended empty and filled
        in afterwards through the handle that was kept.
    */
    juce::ValueTree toValueTree (const DataNode& node)
    {
        juce::ValueTree root { toIdentifier (node.type()) };
        std::vector<std::pair<const DataNode*, juce::ValueTree>> remaining { { &node, root } };

        while (! remaining.empty())
        {
            // Not const: a ValueTree is a handle, and writing through it is
            // writing to the node it names.
            auto [source, tree] = remaining.back();
            remaining.pop_back();

            for (const auto& [key, value] : source->values())
                tree.setProperty (toIdentifier (key), juce::String { value }, nullptr);

            for (const auto& child : source->children())
            {
                const juce::ValueTree childTree { toIdentifier (child.type()) };

                tree.appendChild (childTree, nullptr);
                remaining.emplace_back (&child, childTree);
            }
        }

        return root;
    }

    /** A node with the values of one tree on it, and nothing under it yet. */
    DataNode valuesOf (const juce::ValueTree& tree)
    {
        DataNode node { tree.getType().toString().toStdString() };

        for (int index = 0; index < tree.getNumProperties(); ++index)
        {
            const auto key = tree.getPropertyName (index);

            node.set (key.toString().toStdString(), tree[key].toString().toStdString());
        }

        return node;
    }

    /** The same node, back out of the project file.

        Depth first and finished from the bottom up: a `DataNode` holds its
        children by value, so a node is only added to its parent once nothing
        more will be added to it, and nothing ever points into a vector that is
        still growing.
    */
    DataNode toDataNode (const juce::ValueTree& tree)
    {
        struct Frame
        {
            juce::ValueTree source;
            int nextChild = 0;
            DataNode node;
        };

        std::vector<Frame> unfinished;
        unfinished.push_back ({ tree, 0, valuesOf (tree) });

        for (;;)
        {
            auto& frame = unfinished.back();

            if (frame.nextChild < frame.source.getNumChildren())
            {
                const auto child = frame.source.getChild (frame.nextChild++);

                unfinished.push_back ({ child, 0, valuesOf (child) });
                continue;
            }

            auto finished = std::move (frame.node);
            unfinished.pop_back();

            if (unfinished.empty())
                return finished;

            unfinished.back().node.add (std::move (finished));
        }
    }

    /** The plugin's own node in a copy of the state, found by its item ID.

        The engine writes a plugin's parameters onto the node it was loaded
        from; a copy of the state has the same nodes in the same shape, so the
        item ID is what says which of them is which.
    */
    juce::ValueTree pluginNodeFor (const juce::ValueTree& tree, te::EditItemID itemID)
    {
        std::vector<juce::ValueTree> remaining { tree };

        while (! remaining.empty())
        {
            const auto node = remaining.back();
            remaining.pop_back();

            if (node.hasType (te::IDs::PLUGIN) && node[te::IDs::id].toString() == itemID.toString())
                return node;

            for (const auto& child : node)
                remaining.push_back (child);
        }

        return {};
    }

    /** Writes every plugin's diverged parameters onto a copy of the state.

        The same blob the engine writes when it flushes, in the same property,
        on the same node — but onto the copy, and with no undo manager. That is
        the whole difference between this save and the engine's, and it is why
        the redo stack is still there when the save returns.
    */
    void applyParameterBlobs (te::Edit& edit, const juce::ValueTree& snapshot)
    {
        for (auto* plugin : te::getAllPlugins (edit, true))
        {
            juce::MemoryOutputStream blob;

            // Exactly the engine's own test for "automation has taken this
            // parameter away from the value the producer set", and exact is what
            // it means: a tolerance here would drop a small deliberate change.
            JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wfloat-equal")

            for (auto* parameter : plugin->getAutomatableParameters())
                if (parameter->getCurrentValue() != parameter->getCurrentExplicitValue())
                {
                    blob.writeString (parameter->paramID);
                    blob.writeFloat (parameter->getCurrentExplicitValue());
                }

            JUCE_END_IGNORE_WARNINGS_GCC_LIKE

            blob.flush();

            if (blob.getDataSize() == 0)
                continue;

            if (auto node = pluginNodeFor (snapshot, plugin->itemID); node.isValid())
                node.setProperty (te::IDs::parameters, blob.getMemoryBlock(), nullptr);
        }
    }

    juce::ValueTree duetTreeOf (te::Edit& edit)
    {
        return edit.state.getOrCreateChildWithName (juce::Identifier { duetTreeName }, nullptr);
    }

    /** Schema 1 introduced the durable per-project view node. */
    void migrateToSchema1 (juce::ValueTree duetTree)
    {
        duetTree.getOrCreateChildWithName (toIdentifier (viewTreeName), nullptr);
    }

    /** Schema 2 versioned the view layout and renamed the early notes key. */
    void migrateToSchema2 (juce::ValueTree duetTree)
    {
        auto view = duetTree.getOrCreateChildWithName (toIdentifier (viewTreeName), nullptr);
        view.setProperty (layoutVersionName, 1, nullptr);

        const juce::Identifier oldNotes { "projectNotes" };
        const juce::Identifier sessionNotes { "sessionNotes" };

        if (duetTree.hasProperty (oldNotes) && ! duetTree.hasProperty (sessionNotes))
            duetTree.setProperty (sessionNotes, duetTree[oldNotes], nullptr);

        duetTree.removeProperty (oldNotes, nullptr);
    }

    bool migrateToCurrentSchema (juce::ValueTree duetTree, int storedVersion)
    {
        auto version = std::max (0, storedVersion);

        while (version < currentSchemaVersion)
        {
            switch (version)
            {
                case 0:
                    migrateToSchema1 (duetTree);
                    break;

                case 1:
                    migrateToSchema2 (duetTree);
                    break;

                default:
                    jassertfalse;
                    return false;
            }

            ++version;
            duetTree.setProperty (schemaVersionName, version, nullptr);
        }

        return true;
    }

    std::chrono::steady_clock::duration durationOf (AutosaveInterval interval)
    {
        using enum AutosaveInterval;

        switch (interval)
        {
            case off:
                return std::chrono::steady_clock::duration::max();
            case twoMinutes:
                return std::chrono::minutes { 2 };
            case fiveMinutes:
                return std::chrono::minutes { 5 };
            case tenMinutes:
                return std::chrono::minutes { 10 };
        }

        return std::chrono::minutes { 10 };
    }
} // namespace

//==============================================================================
std::unique_ptr<Project> Project::create (const std::filesystem::path& folder)
{
    if (std::filesystem::exists (editFile (folder)))
        return nullptr;

    std::error_code failure;
    std::filesystem::create_directories (audioDirectory (folder), failure);

    if (failure)
        return nullptr;

    std::unique_ptr<Project> project { new Project {
        folder, std::make_unique<duet::model::Session> (editFile (folder)) } };

    return project->save() ? std::move (project) : nullptr;
}

std::unique_ptr<Project> Project::open (const std::filesystem::path& folder)
{
    return openWithResult (folder).project;
}

ProjectOpenResult Project::openWithResult (const std::filesystem::path& folder,
                                           RecoveryChoice recoveryChoice)
{
    const auto recoveryWasAvailable = recoveryAvailable (folder);
    const auto restoring = recoveryChoice == RecoveryChoice::restore && recoveryWasAvailable;
    const auto source = restoring ? recoveryFile (folder) : editFile (folder);
    auto session = duet::model::Session::openExisting (source);

    if (session == nullptr)
        return { nullptr, "No readable project was found there.", recoveryWasAvailable };

    auto& edit = duet::model::EngineAccess::editOf (*session);
    auto duetTree = duetTreeOf (edit);
    const auto storedVersion = static_cast<int> (duetTree.getProperty (schemaVersionName, 0));

    if (storedVersion > currentSchemaVersion)
        return { nullptr,
                 "This project needs Duet schema version " + std::to_string (storedVersion)
                     + "; open it with a newer Duet.",
                 recoveryWasAvailable };

    const auto migrated = storedVersion < currentSchemaVersion;

    if (! migrateToCurrentSchema (duetTree, storedVersion))
        return { nullptr,
                 "This project's Duet schema could not be migrated.",
                 recoveryWasAvailable };

    if (! restoring)
    {
        std::error_code ignored;
        std::filesystem::remove (recoveryFile (folder), ignored);
        std::filesystem::remove (partialRecoveryFile (folder), ignored);
    }

    return { std::unique_ptr<Project> {
                 new Project { folder, std::move (session), migrated || restoring } },
             {},
             recoveryWasAvailable };
}

bool Project::recoveryAvailable (const std::filesystem::path& folder)
{
    std::error_code failure;
    const auto projectTime = std::filesystem::last_write_time (editFile (folder), failure);

    if (failure)
        return false;

    const auto recoveryTime = std::filesystem::last_write_time (recoveryFile (folder), failure);
    return ! failure && recoveryTime > projectTime;
}

Project::Project (std::filesystem::path folder,
                  std::unique_ptr<duet::model::Session> session,
                  bool migrated)
    : projectFolder (std::move (folder)), editSession (std::move (session)),
      unsavedChanges (migrated)
{
    // The shape of a project folder is this facade's, so this is where the model
    // is told which part of it a take goes into (ADR 0005).
    editSession->setRecordingDirectory (audioDirectory (projectFolder));
    editSession->onProjectChanged ([this] { unsavedChanges = true; });
}

Project::~Project() = default;

//==============================================================================
bool Project::save()
{
    if (! writeSnapshot (editFile (projectFolder), partialSaveFile (projectFolder)))
        return false;

    std::error_code ignored;
    std::filesystem::remove (recoveryFile (projectFolder), ignored);
    std::filesystem::remove (partialRecoveryFile (projectFolder), ignored);
    unsavedChanges = false;
    return true;
}

void Project::setAutosaveInterval (AutosaveInterval interval,
                                   std::chrono::steady_clock::time_point now)
{
    autosaveEvery = interval;
    lastAutosaveTick = now;
}

bool Project::autosaveTick (std::chrono::steady_clock::time_point now)
{
    if (autosaveEvery == AutosaveInterval::off)
        return false;

    if (now < lastAutosaveTick || now - lastAutosaveTick < durationOf (autosaveEvery))
        return false;

    lastAutosaveTick = now;

    if (! unsavedChanges
        || ! writeSnapshot (recoveryFile (projectFolder), partialRecoveryFile (projectFolder)))
        return false;

    // Some filesystems expose coarse mtimes. Recovery is offered only when it
    // is newer than the explicit save, so make that ordering true even when two
    // successful writes would otherwise receive the same timestamp.
    std::error_code failure;
    const auto savedTime = std::filesystem::last_write_time (editFile (projectFolder), failure);

    if (! failure)
    {
        const auto recoveredTime =
            std::filesystem::last_write_time (recoveryFile (projectFolder), failure);

        if (! failure && recoveredTime <= savedTime)
            std::filesystem::last_write_time (recoveryFile (projectFolder),
                                              savedTime + decltype (savedTime)::duration { 1 },
                                              failure);
    }

    return true;
}

bool Project::writeSnapshot (const std::filesystem::path& destination,
                             const std::filesystem::path& partial)
{
    auto& edit = duet::model::EngineAccess::editOf (*editSession);

    duetTreeOf (edit).setProperty (schemaVersionName, currentSchemaVersion, nullptr);
    writeViewState();

    const auto snapshot = edit.state.createCopy();
    applyParameterBlobs (edit, snapshot);

    const auto xml = snapshot.createXml();

    if (xml == nullptr || ! xml->writeTo (toJuceFile (partial)))
        return false;

    std::error_code failure;
    std::filesystem::rename (partial, destination, failure);

    if (failure)
    {
        std::error_code ignored;
        std::filesystem::remove (partial, ignored);
        return false;
    }

    return true;
}

std::filesystem::path Project::importAudioFile (const std::filesystem::path& sourceFile)
{
    auto destination = audioDirectory (projectFolder) / sourceFile.filename();

    // Importing what the project already holds is not a copy onto itself.
    std::error_code alreadyHere;

    if (std::filesystem::equivalent (sourceFile, destination, alreadyHere))
        return destination;

    std::error_code failure;
    std::filesystem::create_directories (audioDirectory (projectFolder), failure);

    if (failure)
        return {};

    std::filesystem::copy_file (
        sourceFile, destination, std::filesystem::copy_options::overwrite_existing, failure);

    return failure ? std::filesystem::path {} : std::move (destination);
}

//==============================================================================
void Project::setDuetValue (std::string_view key, std::string_view value)
{
    auto& edit = duet::model::EngineAccess::editOf (*editSession);
    auto duetTree =
        edit.state.getOrCreateChildWithName (juce::Identifier { duetTreeName }, nullptr);

    duetTree.setProperty (toIdentifier (key), juce::String { std::string { value } }, nullptr);

    unsavedChanges = true;
}

std::string Project::duetValue (std::string_view key) const
{
    auto& edit = duet::model::EngineAccess::editOf (*editSession);
    const auto duetTree = edit.state.getChildWithName (juce::Identifier { duetTreeName });

    return duetTree[toIdentifier (key)].toString().toStdString();
}

//==============================================================================
void Project::onCaptureViewState (std::function<DataNode()> capture)
{
    captureViewState = std::move (capture);
}

DataNode Project::viewState() const
{
    auto& edit = duet::model::EngineAccess::editOf (*editSession);
    const auto duetTree = edit.state.getChildWithName (juce::Identifier { duetTreeName });
    const auto view = duetTree.getChildWithName (toIdentifier (viewTreeName));

    return view.isValid() ? toDataNode (view) : DataNode { std::string { viewTreeName } };
}

void Project::writeViewState()
{
    if (! captureViewState)
        return;

    const auto view = captureViewState();

    auto& edit = duet::model::EngineAccess::editOf (*editSession);
    auto duetTree =
        edit.state.getOrCreateChildWithName (juce::Identifier { duetTreeName }, nullptr);

    // No undo manager, on this write and on the one that clears the last view
    // out of the way: a view is not a producer edit, and an undo that could put
    // a dock back is exactly what writing it this way prevents. The unsaved
    // flag is left alone for the same reason — this write is part of the save
    // that is about to clear it.
    duetTree.removeChild (duetTree.getChildWithName (toIdentifier (view.type())), nullptr);
    duetTree.appendChild (toValueTree (view), nullptr);
}
} // namespace duet::persistence
