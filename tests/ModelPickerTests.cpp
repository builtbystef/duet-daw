// The model picker, with nothing behind it: what the producer sees of their
// providers, which model a run will use, and what happens to a choice when the
// credential under it goes away.
//
// The seam is `ModelPicker::Source` — one object standing for `models.list` and
// the `auth.*` methods — so every rule here is asserted with no sidecar, no
// socket and no provider. What the real source does with those calls is the
// protocol suite's (ModelAccessTests), and what the two agree about is the
// shape below.

#include <duet/gui/ModelPicker.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using duet::gui::ModelPicker;
using duet::gui::OAuthStep;
using duet::testing::StoredSettings;

namespace
{
/** Providers and models held in memory, answering the picker the way the
    sidecar does: a provider contributes its models once it is set up, and every
    entry says whether that provider's auth resolves now.
*/
class FakeAccess final : public ModelPicker::Source
{
public:
    struct Provider
    {
        std::string id;
        std::string name;
        std::vector<std::string> models;
        bool configured = false;
        bool authenticated = false;
    };

    /** In the order they are added, which is the order the picker must keep. */
    std::vector<Provider> providers;

    /** What the next call is to refuse with, and empty to let it through. */
    std::string refusal;

    /** Every model the picker has told this source a run should use. */
    std::vector<std::string> used;

    int listings = 0;

    Listing listing() override
    {
        ++listings;

        Listing out;

        for (const auto& provider : providers)
        {
            out.providers.push_back ({ provider.id,
                                       provider.name,
                                       true,
                                       true,
                                       provider.configured || provider.authenticated,
                                       provider.authenticated });

            if (! (provider.configured || provider.authenticated))
                continue;

            for (const auto& model : provider.models)
                out.models.push_back ({ provider.id + ":" + model,
                                        model,
                                        provider.id,
                                        provider.name,
                                        provider.authenticated });
        }

        return out;
    }

    std::string setApiKey (const std::string& provider, const std::string& key) override
    {
        if (! refusal.empty())
            return refusal;

        if (key.empty())
            return "Enter a key.";

        authenticate (provider, true);

        return {};
    }

    std::string removeCredentials (const std::string& provider) override
    {
        if (! refusal.empty())
            return refusal;

        authenticate (provider, false);

        return {};
    }

    std::string beginOAuth (const std::string& provider, OAuthStep& step) override
    {
        if (! refusal.empty())
            return refusal;

        step = { "https://example.invalid/" + provider, "Sign in there." };
        signingIn = provider;

        return {};
    }

    std::string completeOAuth (const std::string& provider, const std::string& code) override
    {
        if (signingIn != provider)
            return "No sign-in is waiting for a code.";

        if (code != "the-code")
            return "That code was not right.";

        authenticate (provider, true);

        return {};
    }

    void useModel (const std::string& modelId) override { used.push_back (modelId); }

    /** The provider whose sign-in is open, and empty when none is. */
    std::string signingIn;

private:
    /** A key entered, or a credential removed: the provider stays set up either
        way, which is what keeps its models in the picker.
    */
    void authenticate (const std::string& id, bool nowAuthenticated)
    {
        for (auto& provider : providers)
            if (provider.id == id)
            {
                provider.configured = true;
                provider.authenticated = nowAuthenticated;
            }
    }
};

/** Two providers, neither set up: what a first launch looks like. Anthropic is
    offered first so that the recommended default's provider is never the one
    the order would pick anyway.
*/
void offerTwoProviders (FakeAccess& access)
{
    access.providers.push_back (
        { "anthropic", "Anthropic", { "claude-opus-4-7", "claude-haiku" } });
    access.providers.push_back ({ "openai", "OpenAI", { "gpt-5.6-terra", "gpt-5.6-luna" } });
}

/** The ids of the models the picker offers, in the order it offers them. */
std::vector<std::string> offered (const ModelPicker& picker)
{
    std::vector<std::string> ids;

    for (const auto& model : picker.models())
        ids.push_back (model.id);

    return ids;
}
} // namespace

//==============================================================================
TEST_CASE ("with nothing configured the picker has nothing to offer")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    CHECK (picker.models().empty());
    CHECK (picker.selectedModel().empty());
    CHECK_FALSE (picker.anyModelUsable());

    // Both providers are still offered, so that the producer has somewhere to
    // put a key.
    CHECK (picker.providers().size() == 2);
    CHECK (access.used.empty());
}

TEST_CASE ("a picker with no source behind it is a Collaborator with nothing set up")
{
    StoredSettings store;
    ModelPicker picker { store };
    picker.refresh();

    CHECK (picker.models().empty());
    CHECK_FALSE (picker.anyModelUsable());
}

TEST_CASE ("entering a key makes that provider's models usable and picks the recommended one")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    CHECK (picker.setApiKey ("openai", "sk-1").empty());

    REQUIRE (picker.anyModelUsable());
    CHECK (picker.selectedModel() == ModelPicker::recommendedModel);
    CHECK (offered (picker)
           == std::vector<std::string> { "openai:gpt-5.6-terra", "openai:gpt-5.6-luna" });

    for (const auto& model : picker.models())
        CHECK (model.authenticated);

    // The choice is in force for the next run, and said once.
    REQUIRE (access.used.size() == 1);
    CHECK (access.used.front() == ModelPicker::recommendedModel);
}

TEST_CASE ("no provider is ordered ahead of another for being that provider")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    REQUIRE (picker.setApiKey ("openai", "sk-1").empty());
    REQUIRE (picker.setApiKey ("anthropic", "sk-2").empty());

    // The source's order, kept: the recommended default's provider does not
    // climb, and the picker sorts nothing of its own.
    CHECK (offered (picker)
           == std::vector<std::string> { "anthropic:claude-opus-4-7",
                                         "anthropic:claude-haiku",
                                         "openai:gpt-5.6-terra",
                                         "openai:gpt-5.6-luna" });

    // Being first is not being chosen: the recommended default is.
    CHECK (picker.selectedModel() == ModelPicker::recommendedModel);
}

TEST_CASE ("with the recommended default's provider unauthenticated the first configured one wins")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    REQUIRE (picker.setApiKey ("anthropic", "sk-2").empty());

    CHECK (picker.selectedModel() == "anthropic:claude-opus-4-7");
}

TEST_CASE ("an entry whose provider is not authenticated is offered and is not usable")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    REQUIRE (picker.setApiKey ("anthropic", "sk-2").empty());
    REQUIRE (picker.removeCredentials ("anthropic").empty());

    REQUIRE (picker.models().size() == 2);

    for (const auto& model : picker.models())
        CHECK_FALSE (model.authenticated);

    CHECK_FALSE (picker.isUsable ("anthropic:claude-haiku"));
    CHECK_FALSE (picker.select ("anthropic:claude-haiku"));
    CHECK (picker.selectedModel().empty());
}

TEST_CASE ("the producer's chosen model survives an app restart")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);

    {
        ModelPicker picker { store };
        picker.setSource (&access);
        picker.refresh();
        REQUIRE (picker.setApiKey ("openai", "sk-1").empty());
        REQUIRE (picker.select ("openai:gpt-5.6-luna"));
        CHECK (picker.selectedModel() == "openai:gpt-5.6-luna");
    }

    // A second launch, over the same store: the choice is theirs and not the
    // recommendation's again.
    ModelPicker second { store };
    second.setSource (&access);
    second.refresh();

    CHECK (second.selectedModel() == "openai:gpt-5.6-luna");
    REQUIRE_FALSE (access.used.empty());
    CHECK (access.used.back() == "openai:gpt-5.6-luna");
}

TEST_CASE ("removing the credential under the chosen model falls back rather than leaving it")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    REQUIRE (picker.setApiKey ("openai", "sk-1").empty());
    REQUIRE (picker.setApiKey ("anthropic", "sk-2").empty());
    REQUIRE (picker.selectedModel() == ModelPicker::recommendedModel);

    REQUIRE (picker.removeCredentials ("openai").empty());

    CHECK (picker.selectedModel() == "anthropic:claude-opus-4-7");
    CHECK (picker.isUsable (picker.selectedModel()));
    CHECK (access.used.back() == "anthropic:claude-opus-4-7");
}

TEST_CASE ("with the last credential gone the panel has a setup state again")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    REQUIRE (picker.setApiKey ("openai", "sk-1").empty());
    REQUIRE (picker.removeCredentials ("openai").empty());

    CHECK_FALSE (picker.anyModelUsable());
    CHECK (picker.selectedModel().empty());
    CHECK (access.used.back().empty());
}

TEST_CASE ("a key the provider layer refuses comes back as a plain message")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    access.refusal = "That key was refused.";

    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    CHECK (picker.setApiKey ("openai", "sk-wrong") == "That key was refused.");
    CHECK (picker.trouble() == "That key was refused.");
    CHECK_FALSE (picker.anyModelUsable());

    access.refusal.clear();

    CHECK (picker.setApiKey ("openai", "sk-1").empty());
    CHECK (picker.trouble().empty());
}

TEST_CASE ("a subscription sign-in shows the address, and the code finishes it")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    OAuthStep step;

    REQUIRE (picker.beginOAuth ("anthropic", step).empty());
    CHECK (step.url == "https://example.invalid/anthropic");
    CHECK (step.instructions == "Sign in there.");
    CHECK_FALSE (picker.anyModelUsable());

    CHECK (picker.completeOAuth ("anthropic", "wrong") == "That code was not right.");
    CHECK_FALSE (picker.anyModelUsable());

    CHECK (picker.completeOAuth ("anthropic", "the-code").empty());
    CHECK (picker.anyModelUsable());
    CHECK (picker.selectedModel() == "anthropic:claude-opus-4-7");
}

TEST_CASE ("choosing the model already chosen tells nobody anything")
{
    StoredSettings store;
    FakeAccess access;
    offerTwoProviders (access);
    ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    REQUIRE (picker.setApiKey ("openai", "sk-1").empty());
    REQUIRE (access.used.size() == 1);

    CHECK (picker.select (ModelPicker::recommendedModel));
    CHECK (access.used.size() == 1);

    picker.refresh();

    CHECK (access.used.size() == 1);
}
