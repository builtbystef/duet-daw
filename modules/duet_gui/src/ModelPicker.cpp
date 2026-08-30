#include <duet/gui/ModelPicker.h>

#include <duet/gui/Settings.h>

#include <algorithm>
#include <string>
#include <utility>

namespace duet::gui
{
namespace
{
    /** Whether that model is in the list and its provider's credentials resolve
        now, which together are the whole of "a run could be made with this".
    */
    [[nodiscard]] bool usable (const std::vector<ModelChoice>& models, const std::string& id)
    {
        return std::any_of (models.begin(),
                            models.end(),
                            [&id] (const ModelChoice& model)
                            { return model.id == id && model.authenticated; });
    }
} // namespace

ModelPicker::ModelPicker (Settings& store) : settings (store) {}

void ModelPicker::setSource (Source* access) { source = access; }

void ModelPicker::refresh()
{
    if (source == nullptr)
    {
        offered.clear();
        accounts.clear();
        resolveSelection();

        return;
    }

    auto listing = source->listing();

    offered = std::move (listing.models);
    accounts = std::move (listing.providers);

    if (! listing.trouble.empty())
        lastTrouble = std::move (listing.trouble);

    resolveSelection();
}

bool ModelPicker::useStoredChoice()
{
    chosen = settings.value (modelKey).value_or (std::string {});

    if (told != chosen)
    {
        told = chosen;

        if (source != nullptr)
            source->useModel (chosen);
    }

    return ! chosen.empty();
}

bool ModelPicker::isUsable (const std::string& modelId) const { return usable (offered, modelId); }

bool ModelPicker::anyModelUsable() const
{
    return std::any_of (offered.begin(),
                        offered.end(),
                        [] (const ModelChoice& model) { return model.authenticated; });
}

bool ModelPicker::select (const std::string& modelId)
{
    if (! isUsable (modelId))
        return false;

    // Stored before it is resolved, because what is stored is what the producer
    // asked for: the next launch works out what is in force from it again.
    settings.setValue (modelKey, modelId);
    resolveSelection();

    return true;
}

std::string ModelPicker::setApiKey (const std::string& provider, const std::string& key)
{
    if (source == nullptr)
        return "The Collaborator isn't running, so there is nowhere to keep that key.";

    lastTrouble = source->setApiKey (provider, key);
    refresh();

    return lastTrouble;
}

std::string ModelPicker::removeCredentials (const std::string& provider)
{
    if (source == nullptr)
        return "The Collaborator isn't running.";

    lastTrouble = source->removeCredentials (provider);
    refresh();

    return lastTrouble;
}

std::string ModelPicker::beginOAuth (const std::string& provider, OAuthStep& step)
{
    if (source == nullptr)
        return "The Collaborator isn't running, so there is no sign-in to begin.";

    lastTrouble = source->beginOAuth (provider, step);
    refresh();

    return lastTrouble;
}

std::string ModelPicker::completeOAuth (const std::string& provider, const std::string& code)
{
    if (source == nullptr)
        return "The Collaborator isn't running, so there is no sign-in to finish.";

    lastTrouble = source->completeOAuth (provider, code);
    refresh();

    return lastTrouble;
}

void ModelPicker::resolveSelection()
{
    // The producer's own choice first, while it still resolves: a key that has
    // come back makes the model they picked theirs again, and a launch in which
    // it has not is not the moment to forget it.
    const auto stored = settings.value (modelKey).value_or (std::string {});

    std::string inForce;

    if (usable (offered, stored))
        inForce = stored;
    else if (usable (offered, recommendedModel))
        inForce = recommendedModel;
    else
    {
        const auto first =
            std::find_if (offered.begin(),
                          offered.end(),
                          [] (const ModelChoice& model) { return model.authenticated; });

        if (first != offered.end())
            inForce = first->id;
    }

    chosen = inForce;

    // Told once, and only when the answer moves. Nothing is said about a
    // Collaborator that has never had a model: the service starts with none, and
    // saying so again would be a message about nothing.
    if (told == chosen)
        return;

    told = chosen;

    if (source != nullptr)
        source->useModel (chosen);
}
} // namespace duet::gui
