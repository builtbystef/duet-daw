#pragma once

#include <duet/collab/CollaboratorService.h>

#include <duet/gui/ModelPicker.h>

#include <filesystem>
#include <string>

namespace duet::app
{
/** Where the producer's provider credentials are kept: beside the app-global
    settings under the user's configuration folder, which is the machine's place
    and not a project's.

    The DAW hands this to the sidecar, which is what writes it. Nothing about a
    credential ever reaches a project folder or the project's persistence (spec
    js437t) — this is the one path there is, and it is outside every one of
    them.
*/
[[nodiscard]] std::filesystem::path collaboratorCredentialsFile();

/** The provider layer as the model picker sees it, over the Collaborator
    service.

    The picker holds what the producer sees and the service owns the socket;
    neither can name the other, one linking no JUCE and the other nothing of the
    interface, so this is where the two meet — the same seam the conversation
    panel is joined to its runs at.

    Every call here is a question with an answer the producer is waiting for, so
    every one of them waits: `models.list` and the four `auth.*` methods of ADR
    0003, asked on the message thread and answered before the call returns. A
    sidecar that is not running is spawned to answer them. What a Task Run uses
    is the one thing that does not wait — `useModel` leaves the choice with the
    service, which sends it in front of the next run.

    A failure comes back as the plain sentence to show, never as a code: the
    sidecar's own words when it had any, and the one line the spec gives a
    backend that is not there when it did not.
*/
class ModelAccess final : public duet::gui::ModelPicker::Source
{
public:
    explicit ModelAccess (duet::collab::CollaboratorService& collaboratorService);

    [[nodiscard]] Listing listing() override;

    [[nodiscard]] std::string setApiKey (const std::string& provider,
                                         const std::string& key) override;
    [[nodiscard]] std::string removeCredentials (const std::string& provider) override;
    [[nodiscard]] std::string beginOAuth (const std::string& provider,
                                          duet::gui::OAuthStep& step) override;
    [[nodiscard]] std::string completeOAuth (const std::string& provider,
                                             const std::string& code) override;

    void useModel (const std::string& modelId) override;

private:
    /** One `auth.*` call, as the message to show: empty when it worked. */
    [[nodiscard]] std::string authCall (const std::string& method,
                                        const duet::collab::Json& params);

    duet::collab::CollaboratorService& service;
};
} // namespace duet::app
