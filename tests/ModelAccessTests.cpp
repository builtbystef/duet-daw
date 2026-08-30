// Model access across the socket: the model list, and the four ways a provider
// comes to be authenticated or stops being.
//
// The real sidecar, driven by the real service, with no provider reachable and
// no account anywhere. `--offline-script` puts two scripted providers where the
// forty real ones would be: one that authenticates itself, which every run test
// needs, and one locked twin that answers to nothing but a stored credential —
// so entering a key, signing in, pasting a code and removing a credential are
// all assertable here, deterministically, at no cost and with nothing on the
// network.
//
// The suite skips itself when the binary was not built, which is what a machine
// without bun sees.

#include "CollaboratorHarness.h"

#include <duet/app/ModelAccess.h>
#include <duet/collab/TaskRun.h>
#include <duet/gui/ModelPicker.h>
#include <duet/model/AppSettings.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using duet::collab::Json;
using duet::testing::Harness;
using duet::testing::TempFiles;

namespace
{
#ifdef DUET_SIDECAR_BINARY
constexpr std::string_view sidecarBinary = DUET_SIDECAR_BINARY;
#else
constexpr std::string_view sidecarBinary;
#endif

bool sidecarWasBuilt()
{
    return ! sidecarBinary.empty() && std::filesystem::exists (sidecarBinary);
}

/** The scripted providers, and a place of the DAW's choosing to keep credentials
    in. The script itself never runs: nothing here starts a Task Run.
*/
Harness accessing (const TempFiles& files, const std::filesystem::path& credentials)
{
    const auto script =
        files.write ("script.json", Json::array ({ Json { { "text", "ready" } } }).dump());

    return Harness { { "--offline-script", script.string(), "--credentials", credentials.string() },
                     sidecarBinary };
}

/** The listing, or a failure the test can say out loud. */
Json listing (const Harness& harness)
{
    const auto outcome = harness->ask ("models.list", Json::object());

    INFO (outcome.error.message);
    REQUIRE (outcome.succeeded);

    return outcome.result;
}

/** One provider of a listing, by id, and an empty object when the listing holds
    no such provider — which every assertion here reads as "not offered".
*/
Json providerIn (const Json& list, const std::string& id)
{
    for (const auto& provider : list.at ("providers"))
        if (provider.value ("id", std::string {}) == id)
            return provider;

    return Json::object();
}

/** Every model of one provider in a listing. */
std::vector<Json> modelsOf (const Json& list, const std::string& providerId)
{
    std::vector<Json> found;

    for (const auto& model : list.at ("models"))
        if (model.value ("provider", std::string {}) == providerId)
            found.push_back (model);

    return found;
}

std::string readWhole (const std::filesystem::path& path)
{
    const std::ifstream file { path };
    std::ostringstream contents;
    contents << file.rdbuf();

    return contents.str();
}

/** The locked twin's own names, as `offline.ts` declares them. */
constexpr const char* locked = "duet-locked";
constexpr const char* lockedModel = "duet-locked:padlock";
constexpr const char* lockedCode = "duet-offline-code";
} // namespace

//==============================================================================
TEST_CASE ("a provider nobody has set up offers no models and says what it takes", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    const auto list = listing (harness);
    const auto provider = providerIn (list, locked);

    REQUIRE (provider.contains ("id"));
    CHECK (provider.value ("configured", true) == false);
    CHECK (provider.value ("authenticated", true) == false);
    CHECK (provider.value ("apiKey", false));
    CHECK (provider.value ("oauth", false));
    CHECK (modelsOf (list, locked).empty());
}

TEST_CASE ("an API key makes that provider's models appear authenticated", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    REQUIRE (
        harness->ask ("auth.setApiKey", Json { { "provider", locked }, { "key", "sk-duet-1" } })
            .succeeded);

    const auto list = listing (harness);
    const auto models = modelsOf (list, locked);

    REQUIRE (models.size() == 1);
    CHECK (models.front().value ("id", std::string {}) == lockedModel);
    CHECK (models.front().value ("authenticated", false));

    // The provider's own name travels with the model, because a model id says
    // nothing about who is charging for it.
    CHECK (models.front().value ("providerName", std::string {}) == "Duet locked script");
    CHECK (providerIn (list, locked).value ("authenticated", false));
}

TEST_CASE ("removing a provider's credentials returns its models to unauthenticated", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    REQUIRE (
        harness->ask ("auth.setApiKey", Json { { "provider", locked }, { "key", "sk-duet-1" } })
            .succeeded);
    REQUIRE (harness->ask ("auth.remove", Json { { "provider", locked } }).succeeded);

    const auto list = listing (harness);
    const auto models = modelsOf (list, locked);

    // Still in the picker, and no longer usable: the producer sees what they had
    // rather than a provider that has vanished.
    REQUIRE (models.size() == 1);
    CHECK (models.front().value ("authenticated", true) == false);
    CHECK (providerIn (list, locked).value ("configured", false));
    CHECK (providerIn (list, locked).value ("authenticated", true) == false);
}

TEST_CASE ("a key is kept where the DAW put it and nowhere near a project", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    const auto credentials = files.at ("credentials.json");
    const auto project = files.at ("Night Drive");
    std::filesystem::create_directories (project);

    auto harness = accessing (files, credentials);
    harness->start();

    REQUIRE (
        harness->ask ("auth.setApiKey", Json { { "provider", locked }, { "key", "sk-secret-key" } })
            .succeeded);
    REQUIRE (duet::testing::waitUntil ([&credentials]
                                       { return std::filesystem::exists (credentials); }));

    CHECK_THAT (readWhole (credentials), Catch::Matchers::ContainsSubstring ("sk-secret-key"));

    // Nothing of the producer's project has heard of it (spec js437t: a
    // credential belongs to the provider layer, never to project data).
    for (const auto& entry : std::filesystem::recursive_directory_iterator (project))
        CHECK_THAT (readWhole (entry.path()),
                    ! Catch::Matchers::ContainsSubstring ("sk-secret-key"));

    CHECK (std::filesystem::is_empty (project));
}

TEST_CASE ("a key entered once is there for the sidecar that replaces this one", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    const auto credentials = files.at ("credentials.json");

    {
        auto harness = accessing (files, credentials);
        harness->start();
        REQUIRE (
            harness->ask ("auth.setApiKey", Json { { "provider", locked }, { "key", "sk-duet-1" } })
                .succeeded);
    }

    auto second = accessing (files, credentials);
    second->start();

    const auto models = modelsOf (listing (second), locked);

    REQUIRE (models.size() == 1);
    CHECK (models.front().value ("authenticated", false));
}

TEST_CASE ("a blank key is refused with a plain message rather than stored", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    const auto refused =
        harness->ask ("auth.setApiKey", Json { { "provider", locked }, { "key", "   " } });

    CHECK (! refused.succeeded);
    CHECK_THAT (refused.error.message, Catch::Matchers::ContainsSubstring ("API key"));
    CHECK (modelsOf (listing (harness), locked).empty());
}

TEST_CASE ("a provider Duet does not know is refused by name", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    const auto refused =
        harness->ask ("auth.setApiKey", Json { { "provider", "moon-labs" }, { "key", "k" } });

    CHECK (! refused.succeeded);
    CHECK_THAT (refused.error.message, Catch::Matchers::ContainsSubstring ("moon-labs"));
}

//==============================================================================
TEST_CASE ("the sign-in hands back the address and the instructions to show", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    const auto begun = harness->ask ("auth.beginOAuth", Json { { "provider", locked } });

    INFO (begun.error.message);
    REQUIRE (begun.succeeded);
    CHECK_THAT (begun.result.value ("url", std::string {}),
                Catch::Matchers::StartsWith ("https://example.invalid/duet/authorize"));
    CHECK_THAT (begun.result.value ("instructions", std::string {}),
                Catch::Matchers::ContainsSubstring ("paste the code"));

    // Nothing is authenticated by having been offered an address.
    CHECK (! providerIn (listing (harness), locked).value ("authenticated", true));
}

TEST_CASE ("completing the sign-in authenticates the provider with no API key", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    const auto credentials = files.at ("credentials.json");
    auto harness = accessing (files, credentials);
    harness->start();

    REQUIRE (harness->ask ("auth.beginOAuth", Json { { "provider", locked } }).succeeded);

    const auto completed = harness->ask ("auth.completeOAuth",
                                         Json { { "provider", locked }, { "code", lockedCode } });

    INFO (completed.error.message);
    REQUIRE (completed.succeeded);

    const auto models = modelsOf (listing (harness), locked);

    REQUIRE (models.size() == 1);
    CHECK (models.front().value ("authenticated", false));

    // What was stored is a subscription and not a key: the producer typed no key
    // and none was invented for them.
    REQUIRE (duet::testing::waitUntil ([&credentials]
                                       { return std::filesystem::exists (credentials); }));

    const auto stored = readWhole (credentials);
    CHECK_THAT (stored, Catch::Matchers::ContainsSubstring ("oauth"));
    CHECK_THAT (stored, ! Catch::Matchers::ContainsSubstring ("api_key"));
}

TEST_CASE ("a code the provider refuses fails plainly and leaves nothing behind", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    REQUIRE (harness->ask ("auth.beginOAuth", Json { { "provider", locked } }).succeeded);

    const auto refused =
        harness->ask ("auth.completeOAuth", Json { { "provider", locked }, { "code", "not-it" } });

    CHECK (! refused.succeeded);
    CHECK_THAT (refused.error.message, Catch::Matchers::ContainsSubstring ("code"));
    CHECK (! providerIn (listing (harness), locked).value ("authenticated", true));
}

TEST_CASE ("a code nobody asked for is refused rather than held", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    const auto refused = harness->ask ("auth.completeOAuth",
                                       Json { { "provider", locked }, { "code", lockedCode } });

    CHECK (! refused.succeeded);
    CHECK_THAT (refused.error.message, Catch::Matchers::ContainsSubstring ("sign-in"));
}

//==============================================================================
TEST_CASE ("no provider is ordered ahead of another for being that provider", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    REQUIRE (
        harness->ask ("auth.setApiKey", Json { { "provider", locked }, { "key", "sk-duet-1" } })
            .succeeded);

    const auto list = listing (harness);
    std::vector<std::string> order;

    for (const auto& provider : list.at ("providers"))
        order.push_back (provider.value ("id", std::string {}));

    // Registration order, and neither alphabetical nor rank: the scripted
    // provider was registered before its locked twin and comes first, though
    // "duet-locked" sorts before "duet-offline" and is the one with a key.
    REQUIRE (order.size() == 2);
    CHECK (order.front() == "duet-offline");
    CHECK (order.back() == locked);
}

//==============================================================================
// The picker over the real thing: the same view-model the settings surface
// drives, on `duet::app::ModelAccess`, on the real service, on the real sidecar.
// The suite above says what the protocol answers; this says that what the
// producer sees is made of those answers.

TEST_CASE ("a key entered in the picker makes that provider's models usable, and a run uses the "
           "chosen one",
           "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = accessing (files, files.at ("credentials.json"));
    harness->start();

    duet::testing::StoredSettings store;
    duet::app::ModelAccess access { *harness };
    duet::gui::ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    // The locked provider is offered as somewhere to put a key, and none of its
    // models can be chosen yet.
    CHECK (std::any_of (picker.providers().begin(),
                        picker.providers().end(),
                        [] (const duet::gui::ProviderAccount& provider)
                        { return provider.id == locked && ! provider.configured; }));
    CHECK_FALSE (picker.isUsable (lockedModel));

    REQUIRE (picker.setApiKey (locked, "sk-duet-1").empty());
    REQUIRE (picker.isUsable (lockedModel));
    REQUIRE (picker.select (lockedModel));

    // What the next Task Run will be configured with, down at the service.
    CHECK (harness->chosenModel() == lockedModel);

    // And the credential removed takes it back: unusable, and the picker falls
    // to the one model that is left.
    REQUIRE (picker.removeCredentials (locked).empty());

    CHECK_FALSE (picker.isUsable (lockedModel));
    CHECK (picker.selectedModel() == "duet-offline:scripted");
    CHECK (harness->chosenModel() == "duet-offline:scripted");
}

TEST_CASE ("a Collaborator with no sidecar to reach says so in one plain sentence", "[sidecar]")
{
    const TempFiles files;

    // A sidecar that is not there is the one way to drive a spawn that cannot
    // succeed, and what the producer must not get is silence.
    const std::vector<std::string> arguments { "--offline-script" };
    const Harness harness { arguments, files.at ("no-such-sidecar") };
    harness->start();

    duet::testing::StoredSettings store;
    duet::app::ModelAccess access { *harness };
    duet::gui::ModelPicker picker { store };
    picker.setSource (&access);
    picker.refresh();

    CHECK_FALSE (picker.anyModelUsable());
    CHECK (picker.trouble() == duet::collab::backendUnavailableMessage);
    CHECK (picker.setApiKey (locked, "sk-duet-1") == duet::collab::backendUnavailableMessage);
}

TEST_CASE ("the credential file is beside the app's settings and inside no project")
{
    const auto credentials = duet::app::collaboratorCredentialsFile();
    const duet::model::AppSettings appSettings;

    // The machine's place, not a project's: the same folder `Settings.xml` is
    // in, which is under the user's configuration folder (spec js437t).
    CHECK (credentials.parent_path() == appSettings.folder());
    CHECK (credentials.has_filename());

    // And nowhere a project could be: a project folder is made under the
    // producer's projects directory or wherever they chose, and this is under
    // neither.
    const duet::testing::TempProject project;

    CHECK (credentials.string().find (project.folder().string()) == std::string::npos);
}
