#pragma once

#include <string>
#include <vector>

namespace duet::gui
{
class Settings;

/** One model the producer can pick, as the picker shows it. */
struct ModelChoice
{
    /** `provider:model` — what a Task Run is configured with, and what the
        app-global store keeps.
    */
    std::string id;

    /** The model's own name, and the provider's beside it: a model id says
        nothing about who is charging for it.
    */
    std::string name;
    std::string provider;
    std::string providerName;

    /** Whether its provider's credentials resolve now. An entry that says no is
        offered and cannot be chosen — the producer sees what they had rather
        than a provider that has vanished.
    */
    bool authenticated = false;

    friend bool operator== (const ModelChoice& first, const ModelChoice& second) = default;
};

/** One provider the producer can set up, as the settings surface shows it.

    The surface needs these before there is a credential for any model to come
    from: with nothing configured there is nothing in the picker, and a provider
    is still what a key or a sign-in is entered against.
*/
struct ProviderAccount
{
    std::string id;
    std::string name;

    /** Whether it takes an API key, and whether it has a subscription sign-in. */
    bool apiKey = false;
    bool oauth = false;

    /** Whether the producer has set it up, and whether its auth resolves now. */
    bool configured = false;
    bool authenticated = false;

    friend bool operator== (const ProviderAccount& first, const ProviderAccount& second) = default;
};

/** What the DAW shows while a subscription sign-in is open: where to go, and
    what to do when the browser is on another machine.
*/
struct OAuthStep
{
    std::string url;
    std::string instructions;
};

/** The model picker and the provider setup, without the painting.

    What the producer has set up, what each model costs them in usability, which
    model the next Task Run will use, and the four gestures that change any of
    it. It knows nothing of sockets or providers: `Source` is the whole of the
    provider layer as this module sees it, and `duet_app` is what implements it
    over the Collaborator service.

    The chosen model is app-global and not a project's (spec js437t), so it lives
    in the settings store — one key, written when the producer chooses, and read
    again at the next launch. What is *in force* is worked out at every refresh
    from what is authenticated now: the producer's own choice while it still
    resolves, the recommended default when its provider is authenticated, and
    otherwise the first model of the first provider that is. No provider is
    privileged in the order — the listing arrives in the provider layer's own
    order and is kept — and the recommendation is a selection and never a sort.
*/
class ModelPicker
{
public:
    /** The provider layer: `models.list` and the `auth.*` methods of ADR 0003,
        in the shape this module can name.

        Every call is made from the message thread and answered before it
        returns, because the producer is looking at the answer. Each one that can
        fail answers with the plain message to show them, and empty when it
        worked — never a code, and never silence.
    */
    class Source
    {
    public:
        virtual ~Source() = default;

        Source (const Source& other) = delete;
        Source& operator= (const Source& other) = delete;

        struct Listing
        {
            std::vector<ModelChoice> models;
            std::vector<ProviderAccount> providers;

            /** Why the listing is empty, when something went wrong rather than
                nothing being set up.
            */
            std::string trouble;
        };

        [[nodiscard]] virtual Listing listing() = 0;

        [[nodiscard]] virtual std::string setApiKey (const std::string& provider,
                                                     const std::string& key) = 0;
        [[nodiscard]] virtual std::string removeCredentials (const std::string& provider) = 0;

        /** Begins a subscription sign-in and fills in where to send the
            producer. The flow goes on running while they are in a browser.
        */
        [[nodiscard]] virtual std::string beginOAuth (const std::string& provider,
                                                      OAuthStep& step) = 0;

        /** What the producer pasted back, and the flow's answer. */
        [[nodiscard]] virtual std::string completeOAuth (const std::string& provider,
                                                         const std::string& code) = 0;

        /** The model every later Task Run is to use, or empty when none can be.
            Called only when the answer changes.
        */
        virtual void useModel (const std::string& modelId) = 0;

    protected:
        Source() = default;
    };

    explicit ModelPicker (Settings& store);
    ~ModelPicker() = default;

    ModelPicker (const ModelPicker& other) = delete;
    ModelPicker& operator= (const ModelPicker& other) = delete;

    /** The provider layer, read and never owned. None is a Collaborator with
        nothing set up, which is what a service that could not start looks like:
        the panel shows its setup state and the DAW is otherwise untouched.

        Nothing is asked of it here. The sidecar behind it is spawned by the
        first question anyone asks it (ADR 0003), and the launch is not that
        question — `refresh` is, and the setup surface is what makes it.
    */
    void setSource (Source* access);

    /** Asks the provider layer again, and works out what is in force. Called
        whenever a credential can have changed, and when the surface is opened.
    */
    void refresh();

    /** Puts the producer's stored choice in force without asking the provider
        layer anything, and says whether there was one.

        What the shell does at launch. The sidecar is spawned on first use (ADR
        0003), and asking what is authenticated would spawn it before the
        producer has asked the Collaborator for anything — so a stored choice is
        taken as it stands, which is enough to configure the next run with.
        Whether it still resolves is answered by that run, or by the setup
        surface, whichever the producer reaches first.
    */
    bool useStoredChoice();

    [[nodiscard]] const std::vector<ModelChoice>& models() const { return offered; }
    [[nodiscard]] const std::vector<ProviderAccount>& providers() const { return accounts; }

    /** The model the next Task Run will use, and empty when none can be. */
    [[nodiscard]] const std::string& selectedModel() const { return chosen; }

    /** Whether that model is one a run could be made with now. */
    [[nodiscard]] bool isUsable (const std::string& modelId) const;

    /** Whether anything at all is usable — false is the panel's setup state. */
    [[nodiscard]] bool anyModelUsable() const;

    /** The producer's choice. Refuses one that is not usable and changes
        nothing, so a picker never holds a selection a run would fail on.
    */
    bool select (const std::string& modelId);

    /** The plain message from the last gesture that failed, and empty when the
        last one worked.
    */
    [[nodiscard]] const std::string& trouble() const { return lastTrouble; }

    //==============================================================================
    /** The four gestures of the setup surface. Each answers with the message to
        show the producer, empty when it worked, and each refreshes what the
        picker offers — a key entered changes what is usable, and so does a
        credential removed.
    */
    std::string setApiKey (const std::string& provider, const std::string& key);
    std::string removeCredentials (const std::string& provider);
    std::string beginOAuth (const std::string& provider, OAuthStep& step);
    std::string completeOAuth (const std::string& provider, const std::string& code);

    //==============================================================================
    /** The model this milestone recommends, selected when its provider is
        authenticated (spec js437t, from fod077's runs). It is a default and not
        a privilege: it is never ordered ahead of anything.
    */
    static constexpr const char* recommendedModel = "openai:gpt-5.6-terra";

    /** Where the producer's choice is kept in the app-global store. */
    static constexpr const char* modelKey = "collaborator.model";

private:
    /** Works out what is in force from what is authenticated now, and tells the
        provider layer when the answer has changed.
    */
    void resolveSelection();

    Settings& settings;
    Source* source = nullptr;

    std::vector<ModelChoice> offered;
    std::vector<ProviderAccount> accounts;
    std::string chosen;
    std::string lastTrouble;

    /** What the provider layer was last told, so that it is told once. */
    std::string told;
};
} // namespace duet::gui
