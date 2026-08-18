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

    juce::File toJuceFile (const std::filesystem::path& path)
    {
        return juce::File { juce::String { path.string() } };
    }

    juce::Identifier toIdentifier (std::string_view name)
    {
        return juce::Identifier { juce::String { std::string { name } } };
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
    auto session = duet::model::Session::openExisting (editFile (folder));

    if (session == nullptr)
        return nullptr;

    return std::unique_ptr<Project> { new Project { folder, std::move (session) } };
}

Project::Project (std::filesystem::path folder, std::unique_ptr<duet::model::Session> session)
    : projectFolder (std::move (folder)), editSession (std::move (session))
{
    editSession->onProjectChanged ([this] { unsavedChanges = true; });
}

Project::~Project() = default;

//==============================================================================
bool Project::save()
{
    auto& edit = duet::model::EngineAccess::editOf (*editSession);

    const auto snapshot = edit.state.createCopy();
    applyParameterBlobs (edit, snapshot);

    const auto partial = partialSaveFile (projectFolder);
    const auto xml = snapshot.createXml();

    if (xml == nullptr || ! xml->writeTo (toJuceFile (partial)))
        return false;

    std::error_code failure;
    std::filesystem::rename (partial, editFile (projectFolder), failure);

    if (failure)
    {
        std::error_code ignored;
        std::filesystem::remove (partial, ignored);
        return false;
    }

    unsavedChanges = false;
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
} // namespace duet::persistence
