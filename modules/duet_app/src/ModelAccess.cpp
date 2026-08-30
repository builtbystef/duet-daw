#include <duet/app/ModelAccess.h>

#include <duet/collab/TaskRun.h>
#include <duet/model/AppSettings.h>

#include <utility>

namespace duet::app
{
namespace
{
    using duet::collab::Json;
    using duet::collab::RpcOutcome;

    /** What went wrong, in words the producer can act on.

        A sidecar that is not there gets the one sentence the spec gives that
        case; everything else carries the sidecar's own text, which is the
        provider layer's own complaint about a key or a code.
    */
    [[nodiscard]] std::string plainly (const RpcOutcome& outcome)
    {
        if (outcome.succeeded)
            return {};

        if (outcome.error.code == duet::collab::rpcError::sidecarUnavailable
            || outcome.error.message.empty())
            return duet::collab::backendUnavailableMessage;

        return outcome.error.message;
    }

    [[nodiscard]] std::string member (const Json& object, const char* name)
    {
        return object.value (name, std::string {});
    }
} // namespace

std::filesystem::path collaboratorCredentialsFile()
{
    // The store is a shared resource: asking one for where it lives opens the
    // same one every other holder has, and puts it down again.
    const duet::model::AppSettings appSettings;

    return appSettings.folder() / "collaborator-credentials.json";
}

ModelAccess::ModelAccess (duet::collab::CollaboratorService& collaboratorService)
    : service (collaboratorService)
{
}

ModelAccess::Listing ModelAccess::listing()
{
    Listing listing;

    const auto outcome = service.ask ("models.list", Json::object());

    if (! outcome.succeeded)
    {
        listing.trouble = plainly (outcome);

        return listing;
    }

    // The order is the provider layer's, kept: the picker sorts nothing and
    // neither does this (spec js437t — no provider is privileged).
    if (const auto models = outcome.result.find ("models"); models != outcome.result.end())
        for (const auto& model : *models)
            listing.models.push_back ({ member (model, "id"),
                                        member (model, "name"),
                                        member (model, "provider"),
                                        member (model, "providerName"),
                                        model.value ("authenticated", false) });

    if (const auto providers = outcome.result.find ("providers"); providers != outcome.result.end())
        for (const auto& provider : *providers)
            listing.providers.push_back ({ member (provider, "id"),
                                           member (provider, "name"),
                                           provider.value ("apiKey", false),
                                           provider.value ("oauth", false),
                                           provider.value ("configured", false),
                                           provider.value ("authenticated", false) });

    return listing;
}

std::string ModelAccess::setApiKey (const std::string& provider, const std::string& key)
{
    return authCall ("auth.setApiKey", Json { { "provider", provider }, { "key", key } });
}

std::string ModelAccess::removeCredentials (const std::string& provider)
{
    return authCall ("auth.remove", Json { { "provider", provider } });
}

std::string ModelAccess::beginOAuth (const std::string& provider, duet::gui::OAuthStep& step)
{
    const auto outcome = service.ask ("auth.beginOAuth", Json { { "provider", provider } });

    if (! outcome.succeeded)
        return plainly (outcome);

    step.url = member (outcome.result, "url");
    step.instructions = member (outcome.result, "instructions");

    return {};
}

std::string ModelAccess::completeOAuth (const std::string& provider, const std::string& code)
{
    return authCall ("auth.completeOAuth", Json { { "provider", provider }, { "code", code } });
}

void ModelAccess::useModel (const std::string& modelId) { service.setModel (modelId); }

std::string ModelAccess::authCall (const std::string& method, const Json& params)
{
    return plainly (service.ask (method, params));
}
} // namespace duet::app
