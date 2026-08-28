#include <duet/app/Collaborator.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace duet::app
{
namespace
{
    using duet::collab::Json;

    /** What the panel reads an estimated value as. A guess about a key is a
        name and a guess about a chord list is not, so a value that is already
        text crosses as itself and everything else as the JSON it is.
    */
    [[nodiscard]] std::string readable (const Json& value)
    {
        return value.is_string() ? value.get<std::string>() : value.dump();
    }

    /** How the History section says where a Suggestion went. */
    [[nodiscard]] std::string outcomeOf (duet::collab::SuggestionState state)
    {
        switch (state)
        {
            case duet::collab::SuggestionState::accepted:
                return "accepted";

            case duet::collab::SuggestionState::rejected:
                return "rejected";

            case duet::collab::SuggestionState::superseded:
                return "asked again";

            case duet::collab::SuggestionState::pending:
                break;
        }

        return {};
    }
} // namespace

//==============================================================================
Collaborator::Collaborator (duet::collab::CollaboratorService& collaboratorService,
                            duet::gui::CollaboratorPanel& conversationPanel,
                            OpeningContextSource openingContext,
                            MessageThread threadAccess)
    : service (collaboratorService), panel (conversationPanel),
      openingContextOf (std::move (openingContext)), messageThread (std::move (threadAccess))
{
    service.setMethodHandler ("tool.call",
                              [this] (const Json& params) { return answerToolCall (params); });
    service.setEstimateLedger (&ledger);
    service.setTaskRunListener (this);
    panel.setSource (this);
}

Collaborator::~Collaborator()
{
    // The order matters: the panel stops asking first, then the service stops
    // telling, and only then does the token that a posted event checks go. What
    // is left in flight after that finds nothing to touch.
    panel.setSource (nullptr);
    service.setTaskRunListener (nullptr);
    service.setEstimateLedger (nullptr);
    life.reset();
}

//==============================================================================
void Collaborator::setSession (duet::model::Session* openProject,
                               std::filesystem::path renderFolder)
{
    // A run about a project that is going is a run about nothing, so it ends
    // here rather than reading what is left of it.
    if (! shownRun.empty())
        service.cancelRun (shownRun);

    estimated.reset();
    measured.reset();
    reads.reset();
    renders.reset();
    session = openProject;

    if (session == nullptr)
        return;

    auto marshal = readMarshalFor (session);

    renders = std::make_unique<duet::collab::TrackRenders> (
        duet::collab::offlineTrackRenderer (*session),
        std::move (renderFolder),
        [this] (const std::string& runId) { return service.isRunInProgress (runId); });

    reads = std::make_unique<duet::collab::ProjectTools> (*session, marshal);
    measured = std::make_unique<duet::collab::TrackAnalysis> (*session, marshal, *renders);
    estimated =
        std::make_unique<duet::collab::ContentEstimates> (*session, marshal, *renders, ledger);

    reads->addTo (registry);
    measured->addTo (registry);
    estimated->addTo (registry);
}

void Collaborator::setSuggestions (duet::collab::SuggestionManager* manager)
{
    suggestions = manager;
    refreshHistory();
}

void Collaborator::refreshHistory()
{
    std::vector<duet::gui::ResolvedSuggestion> resolved;

    if (suggestions != nullptr)
        for (const auto& held : suggestions->suggestions())
            if (held.state != duet::collab::SuggestionState::pending)
                resolved.push_back ({ held.made.summary, outcomeOf (held.state) });

    panel.setHistory (std::move (resolved));
}

//==============================================================================
void Collaborator::producerSent (duet::gui::CollaboratorPanel& sender, const std::string& message)
{
    sender.beginTaskRun();

    const auto context = openingContextOf ? openingContextOf() : duet::collab::OpeningContext {};
    const auto start = service.startRun (message, context);

    if (! start.started)
    {
        // Nothing queues and nothing retries: the run that was refused is one
        // line in the conversation, and the next one is the producer's to ask
        // for (spec js437t).
        sender.failTaskRun (start.error.message);
        return;
    }

    shownRun = start.runId;
}

void Collaborator::taskRunCanceled (duet::gui::CollaboratorPanel& /*sender*/)
{
    if (shownRun.empty())
        return;

    // The panel has already said so, so the run stops being the one shown here
    // too — which is what makes the ending the service sends back a word about
    // a run nobody is showing any more.
    const auto canceled = std::exchange (shownRun, {});
    service.cancelRun (canceled);
}

//==============================================================================
void Collaborator::commentaryDelta (const std::string& runId,
                                    const std::string& delta,
                                    bool basedOnEstimates)
{
    onMessageThread (
        [this, runId, delta, basedOnEstimates]
        {
            if (runId != shownRun)
                return;

            panel.streamCommentary (delta,
                                    basedOnEstimates ? markFor (runId)
                                                     : std::vector<duet::gui::EstimateMarkLine> {});
        });
}

void Collaborator::toolActivity (const std::string& /*runId*/,
                                 const std::string& /*tool*/,
                                 duet::collab::ToolPhase /*phase*/)
{
    // Nothing: this says a tool call began or ended, and the development trace
    // wants what it asked and what it was answered, which exist only where the
    // call is answered. The Target Producer reads the status phrases instead.
}

void Collaborator::runFinished (const std::string& runId,
                                duet::collab::RunStatus status,
                                const std::string& error)
{
    onMessageThread (
        [this, runId, status, error]
        {
            if (runId != shownRun)
                return;

            shownRun.clear();

            switch (status)
            {
                case duet::collab::RunStatus::completed:
                    panel.finishTaskRun();
                    break;

                case duet::collab::RunStatus::canceled:
                    panel.cancelTaskRun();
                    break;

                case duet::collab::RunStatus::failed:
                    panel.failTaskRun (error);
                    break;
            }

            refreshHistory();
        });
}

//==============================================================================
duet::collab::RpcOutcome Collaborator::answerToolCall (const Json& params)
{
    const auto outcome = registry.call (params);

    auto tool = params.value ("tool", std::string {});
    auto arguments = params.value ("args", Json::object()).dump();
    auto result =
        outcome.succeeded
            ? outcome.result.dump()
            : Json { { "code", outcome.error.code }, { "message", outcome.error.message } }.dump();

    onMessageThread (
        [this,
         tool = std::move (tool),
         arguments = std::move (arguments),
         result = std::move (result)]() mutable
        { panel.recordToolCall (std::move (tool), std::move (arguments), std::move (result)); });

    return outcome;
}

void Collaborator::onMessageThread (std::function<void()> work)
{
    if (! messageThread.post)
        return;

    messageThread.post (
        [held = std::weak_ptr<int> { life }, work = std::move (work)]
        {
            if (! held.expired())
                work();
        });
}

std::vector<duet::gui::EstimateMarkLine> Collaborator::markFor (const std::string& runId) const
{
    std::vector<duet::gui::EstimateMarkLine> lines;

    for (const auto& line : ledger.entries (runId))
        lines.push_back ({ line.field,
                           readable (line.estimate.value),
                           line.estimate.method,
                           line.estimate.confidence });

    return lines;
}

duet::collab::ProjectReadMarshal Collaborator::readMarshalFor (duet::model::Session* project)
{
    return [this, project] (const std::function<void()>& work)
    {
        // The check runs where the swap runs, so a read of a project that has
        // gone is a read that never starts rather than one that races. A tool
        // whose read did not run answers that it could not read the project,
        // which is the truth.
        messageThread.readAndWait (
            [this, project, &work]
            {
                if (session == project)
                    work();
            });
    };
}
} // namespace duet::app
