#include <duet/app/Collaborator.h>

#include <algorithm>
#include <memory>
#include <mutex>
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
                            MessageThread threadAccess,
                            TrackRendererFor trackRenderer)
    : service (collaboratorService), panel (conversationPanel),
      openingContextOf (std::move (openingContext)), messageThread (std::move (threadAccess)),
      trackRendererFor (std::move (trackRenderer))
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
    // telling and stops asking for a tool, and only then does the token that a
    // posted event checks go. What is left in flight after that finds nothing
    // to touch.
    panel.setSource (nullptr);
    service.setTaskRunListener (nullptr);
    service.setEstimateLedger (nullptr);
    service.setMethodHandler ("tool.call", {});
    life.reset();

    // The service is stopped before this object goes — that is what anything
    // holding the two owes them — so no call is inside a project any more, and
    // every one of them is put down here, on the thread that may put one down.
    registry.clear();
    open.reset();
    retired.clear();
}

//==============================================================================
void Collaborator::setSession (std::shared_ptr<duet::model::Session> openProject,
                               std::filesystem::path renderFolder)
{
    // A run about a project that is going is a run about nothing, so it ends
    // here rather than reading what is left of it.
    if (! shownRun.empty())
        service.cancelRun (shownRun);

    surfaces.setManager (nullptr, nullptr);
    suggestions.reset();

    // The registry answers no more calls out of the project that is going, and
    // lets go of it: a project is detached on every New, Open and Save As, and
    // stays detached when opening one fails.
    registry.clear();

    // What a call is still inside is not destroyed here. This returns to the
    // producer at once, and a measurement rendering on the service thread needs
    // the message loop to take its copy down, so waiting for one here would be
    // the message thread waiting for itself.
    {
        const std::lock_guard lock (openMutex);

        if (open != nullptr)
            retired.push_back (std::exchange (open, nullptr));
    }

    putDownWhatIsFinishedWith();
    session = openProject.get();

    if (session == nullptr)
    {
        refreshHistory();

        return;
    }

    auto marshal = readMarshalFor (session);
    auto taking = std::make_shared<OpenProject>();
    auto& project = *openProject;
    taking->project = std::move (openProject);

    taking->renders = std::make_unique<duet::collab::TrackRenders> (
        trackRendererFor ? trackRendererFor (project)
                         : duet::collab::offlineTrackRenderer (project),
        std::move (renderFolder),
        [this] (const std::string& runId) { return service.isRunInProgress (runId); });

    taking->reads = std::make_unique<duet::collab::ProjectTools> (project, marshal, &ledger);
    taking->measured =
        std::make_unique<duet::collab::TrackAnalysis> (project, marshal, *taking->renders);
    taking->estimated = std::make_unique<duet::collab::ContentEstimates> (
        project, marshal, *taking->renders, ledger);
    taking->writes = std::make_unique<duet::collab::SuggestTool> (project, marshal, &ledger);

    // The Duet Loop is the open project's: what a Suggestion names lives there,
    // and a Suggestion made against the project before it is not a Suggestion
    // about this one.
    suggestions = std::make_unique<duet::collab::SuggestionManager> (
        project, [this] (const std::string& prompt) { return startRun (prompt); });

    taking->reads->addTo (registry);
    taking->measured->addTo (registry);
    taking->estimated->addTo (registry);
    taking->writes->addTo (registry);
    registry.hold (taking);

    {
        const std::lock_guard lock (openMutex);
        open = std::move (taking);
    }

    surfaces.setManager (suggestions.get(), session);
    refreshHistory();
}

std::shared_ptr<Collaborator::OpenProject> Collaborator::heldProject() const
{
    const std::lock_guard lock (openMutex);

    return open;
}

void Collaborator::putDownWhatIsFinishedWith()
{
    // The list is the only hold left on a project no call is inside, so a
    // count of one is what says the last call is out of it.
    const auto finished = std::remove_if (retired.begin(),
                                          retired.end(),
                                          [] (const std::shared_ptr<OpenProject>& closed)
                                          { return closed.use_count() == 1; });

    retired.erase (finished, retired.end());
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
void Collaborator::producerSent (duet::gui::CollaboratorPanel& /*sender*/,
                                 const std::string& message)
{
    // Through the manager when there is one, because what was asked is what a
    // revision and a redo ask again, and the manager is what remembers it. With
    // no project open there is no manager, and the run is still the producer's
    // to have.
    if (suggestions != nullptr)
    {
        suggestions->ask (message);

        return;
    }

    startRun (message);
}

duet::collab::RunStart Collaborator::startRun (const std::string& prompt)
{
    if (! panel.taskRunning())
        panel.beginTaskRun();

    const auto context = openingContextOf ? openingContextOf() : duet::collab::OpeningContext {};
    const auto start = service.startRun (prompt, context);

    if (! start.started)
    {
        // Nothing queues and nothing retries: the run that was refused is one
        // line in the conversation, and the next one is the producer's to ask
        // for (spec js437t).
        panel.failTaskRun (start.error.message);

        return start;
    }

    shownRun = start.runId;

    return start;
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

    if (outcome.succeeded && tool == "suggest")
    {
        // The project of the moment, and not the one the call was answered out
        // of: a Suggestion about a project the producer has since closed is a
        // Suggestion about nothing, and there is nothing left to hold it in.
        if (const auto project = heldProject(); project != nullptr)
            holdSuggestion (*project->writes,
                            params.value ("runId", std::string {}),
                            outcome.result.value ("suggestionId", std::string {}));
    }

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

void Collaborator::holdSuggestion (duet::collab::SuggestTool& madeBy,
                                   const std::string& runId,
                                   const std::string& suggestionId)
{
    if (suggestionId.empty())
        return;

    auto made = madeBy.suggestion (suggestionId);

    if (! made.has_value())
        return;

    onMessageThread (
        [this, runId, made = std::move (made.value())]() mutable
        {
            const auto id = made.id;
            auto summary = made.summary;

            // A run this Collaborator did not start is a run with no request
            // behind it, and a Suggestion nothing could revise or redo.
            if (suggestions == nullptr || ! suggestions->suggested (runId, std::move (made)))
                return;

            std::string revises;

            if (const auto held = suggestions->suggestion (id); held.has_value())
                revises = held->revises;

            panel.showSuggestion (id, std::move (summary), revises);
        });
}

void Collaborator::onMessageThread (std::function<void()> work)
{
    if (! messageThread.post)
        return;

    messageThread.post (
        [this, held = std::weak_ptr<int> { life }, work = std::move (work)]
        {
            if (held.expired())
                return;

            work();

            // Every event of a run crosses here, so this is where a project the
            // producer closed while a call was inside it is finally put down —
            // on the message thread, which is the only thread that may.
            putDownWhatIsFinishedWith();
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
