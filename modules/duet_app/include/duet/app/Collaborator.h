#pragma once

#include <duet/app/SuggestionSurfaces.h>

#include <duet/collab/CollaboratorService.h>
#include <duet/collab/ContentEstimates.h>
#include <duet/collab/Estimate.h>
#include <duet/collab/ProjectTools.h>
#include <duet/collab/SuggestTool.h>
#include <duet/collab/SuggestionManager.h>
#include <duet/collab/TaskRun.h>
#include <duet/collab/ToolDispatch.h>
#include <duet/collab/TrackAnalysis.h>
#include <duet/collab/TrackRenders.h>

#include <duet/gui/CollaboratorPanel.h>
#include <duet/model/Session.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace duet::app
{
/** What puts the Collaborator service and the Collaborator panel together.

    The panel holds what was said and the service runs the runs; neither can
    name the other, one linking no JUCE and the other no engine, so this is
    where the two meet. It is the panel's `Source` — a sent message becomes one
    Task Run — and the service's `TaskRunListener` — what the run says becomes
    entries in the conversation. It is also where the Tool Vocabulary is
    answered from: the read-only tools of the live project, and the estimate
    ledger they write to, which is what marks a run's commentary.

    **Threads.** Every listener call arrives on the service's own thread and is
    put on the message thread before it touches the panel, because the panel is
    an interface object and the message thread is the only thread that owns one.
    Every tool read goes the other way, onto the message thread and back, that
    being the sole writer of the project model. This object can reach neither by
    itself — it links no JUCE — so `MessageThread` is what the host supplies.

    **One run at a time.** The service refuses a second run and the panel's
    composer is held while one is on, so the two rules agree by construction
    rather than by this object arbitrating between them.

    The service, the panel and whatever the host wires in must all outlive this
    object, and the tool handler it registers means it must be built before the
    service is started — and the service must be stopped before this object goes,
    since the tool handler is what its thread calls into and nothing waits for a
    call that is already inside one.
*/
class Collaborator final : public duet::gui::CollaboratorPanel::Source,
                           private duet::collab::TaskRunListener
{
public:
    /** What a run carries about the producer at the moment it starts: what they
        had selected, where the playhead was, whether the transport was rolling.

        Asked once per run, on the message thread, and frozen into the run — a
        selection the producer changes afterwards is a fact about a moment that
        has already passed (spec js437t).
    */
    using OpeningContextSource = std::function<duet::collab::OpeningContext()>;

    /** How a track of one project is rendered for the tools that measure and
        estimate over its audio.

        The project's own detached offline render, unless the host names
        another. It is a seam because it is the one part of answering a tool
        call that is not the message thread's, and therefore the one part that
        can still be running when a project is closed: what asserts that a
        render outliving its project reads nothing that has gone is a render
        held deliberately still.
    */
    using TrackRendererFor = std::function<duet::collab::TrackRenderer (duet::model::Session&)>;

    /** The two ways this object reaches the message thread, which is neither of
        the threads it is called on.
    */
    struct MessageThread
    {
        /** Runs a project read there and returns once it has run: what the
            service thread waits on for a tool result.
        */
        duet::collab::ProjectReadMarshal readAndWait;

        /** Leaves something there to be done and returns at once: what a run's
            events cross on, so that nothing about a run ever holds the service
            thread up.
        */
        std::function<void (std::function<void()>)> post;
    };

    Collaborator (duet::collab::CollaboratorService& collaboratorService,
                  duet::gui::CollaboratorPanel& conversationPanel,
                  OpeningContextSource openingContext,
                  MessageThread threadAccess,
                  TrackRendererFor trackRenderer = {});

    ~Collaborator() override;

    Collaborator (const Collaborator&) = delete;
    Collaborator (Collaborator&&) = delete;
    Collaborator& operator= (const Collaborator&) = delete;
    Collaborator& operator= (Collaborator&&) = delete;

    //==============================================================================
    /** The project the tools read, and nothing when none is open.

        A project the producer has closed is a project no tool may read, so
        swapping one ends the run that was asking about it: what it was asking
        about is gone. `renderFolder` is where the measured and estimated tools
        keep their renders, and they are removed from it when the project goes.

        Called on the message thread, which is also where a read runs, so a read
        of the project that has gone is a read that never starts rather than one
        that races.

        A hold and not a pointer, because a call can be inside a render when
        this is called, and this returns at once: the producer waits for nothing
        here, and the render goes on reading the project it entered until it
        stops. The project it took is put down on the message thread, once the
        last call is out of it.
    */
    void setSession (std::shared_ptr<duet::model::Session> openProject,
                     std::filesystem::path renderFolder);

    /** The Duet Loop of the open project, and nothing when none is open.

        The manager is this object's because a Suggestion is what a Task Run
        makes: the run that made it is the one whose request has to be
        remembered, and asking again is starting another run. Public so that the
        surfaces the ghosts are drawn on can be handed the same one.
    */
    [[nodiscard]] duet::collab::SuggestionManager* suggestionManager() { return suggestions.get(); }

    /** What the interface's Suggestion surfaces read: the manager, in the shape
        the card and the ghosts speak. The shell hands this to its own
        `duet::gui::Suggestions`, which is the one thing all three surfaces
        read.
    */
    [[nodiscard]] duet::gui::Suggestions::Source& suggestionSurfaces() { return surfaces; }

    /** Takes the manager's resolved Suggestions onto the panel. Called whenever
        one can have been resolved.
    */
    void refreshHistory();

    //==============================================================================
    /** The registry the Tool Vocabulary is answered from, which is what a run's
        `tool.call` reaches. Public so that the write-tool and whatever else the
        Collaborator grows can be added to the same closed set.
    */
    [[nodiscard]] duet::collab::ToolRegistry& tools() { return registry; }

    /** The ledger every run of this Collaborator is marked from. */
    [[nodiscard]] duet::collab::EstimateLedger& estimateLedger() { return ledger; }

    //==============================================================================
    void producerSent (duet::gui::CollaboratorPanel& sender, const std::string& message) override;
    void taskRunCanceled (duet::gui::CollaboratorPanel& sender) override;

private:
    //==============================================================================
    /** Starts one Task Run for a prompt, and puts the card in front of the
        producer while it lasts.

        Every run of this Collaborator goes through here — the producer's own
        message, a revision the manager asks for, a redo against current state —
        because every one of them is a run the panel has to be showing.
    */
    duet::collab::RunStart startRun (const std::string& prompt);

    /** Holds what a run's `suggest` call made, and puts its card in the
        conversation where the producer asked for it.
    */
    void holdSuggestion (duet::collab::SuggestTool& madeBy,
                         const std::string& runId,
                         const std::string& suggestionId);

    //==============================================================================
    void commentaryDelta (const std::string& runId,
                          const std::string& delta,
                          bool basedOnEstimates) override;
    void toolActivity (const std::string& runId,
                       const std::string& tool,
                       duet::collab::ToolPhase phase) override;
    void runFinished (const std::string& runId,
                      duet::collab::RunStatus status,
                      const std::string& error) override;

    /** Answers one `tool.call`, and takes the development trace off it on the
        way past: the names, the arguments and the results are what a developer
        debugging a run needs, and they exist nowhere else.
    */
    [[nodiscard]] duet::collab::RpcOutcome answerToolCall (const duet::collab::Json& params);

    /** Puts work on the message thread, and drops it if this object has gone by
        the time it gets there.
    */
    void onMessageThread (std::function<void()> work);

    /** What the panel shows a run's estimate mark as: that run's ledger, in the
        panel's own engine-free shape.
    */
    [[nodiscard]] std::vector<duet::gui::EstimateMarkLine> markFor (const std::string& runId) const;

    /** The read a tool makes, refused once the project it was to read has gone. */
    [[nodiscard]] duet::collab::ProjectReadMarshal readMarshalFor (duet::model::Session* project);

    /** One open project and everything built over it: the renders its
        measurements are kept in, and the four tool objects that read it.

        Held together and by a hold, because a tool call in flight is inside all
        of it at once. The swap that closes a project lets go of this and
        returns; whatever call is still inside goes on reading a project nobody
        has destroyed, and the last one out is what lets it go.
    */
    struct OpenProject
    {
        std::shared_ptr<duet::model::Session> project;
        std::unique_ptr<duet::collab::TrackRenders> renders;
        std::unique_ptr<duet::collab::ProjectTools> reads;
        std::unique_ptr<duet::collab::TrackAnalysis> measured;
        std::unique_ptr<duet::collab::ContentEstimates> estimated;
        std::unique_ptr<duet::collab::SuggestTool> writes;
    };

    /** A hold on the open project, or nothing when none is open. Taken by the
        service thread, which is not the thread that swaps it.
    */
    [[nodiscard]] std::shared_ptr<OpenProject> heldProject() const;

    /** Puts down every retired project no call is inside any more.

        Called on the message thread alone, which is where a project must be put
        down: the engine's own teardown is the message thread's work, and the
        service thread that let the last call out of one cannot do it.
    */
    void putDownWhatIsFinishedWith();

    duet::collab::CollaboratorService& service;
    duet::gui::CollaboratorPanel& panel;
    OpeningContextSource openingContextOf;
    MessageThread messageThread;
    TrackRendererFor trackRendererFor;

    duet::collab::ToolRegistry registry;
    duet::collab::EstimateLedger ledger;

    /** The project the marshal checks a read against: the one the producer has
        open, and null between projects. The message thread's alone.
    */
    duet::model::Session* session = nullptr;

    mutable std::mutex openMutex;
    std::shared_ptr<OpenProject> open;

    /** The projects a swap has closed that a call was still inside. Emptied as
        each one is finished with, on the message thread.
    */
    std::vector<std::shared_ptr<OpenProject>> retired;

    std::unique_ptr<duet::collab::SuggestionManager> suggestions;

    /** Declared after the manager so that it goes before it: it reads the
        manager and the manager reads nothing of it.
    */
    SuggestionSurfaces surfaces { [this] { refreshHistory(); } };

    /** The run the panel is showing, and empty between runs. Written and read on
        the message thread alone, which is what lets a late event about a run the
        producer has already canceled be dropped without a lock.
    */
    std::string shownRun;

    /** What a posted event checks before it touches anything: this object is
        gone once the token is, and a run's events are in flight across two
        threads.
    */
    std::shared_ptr<int> life = std::make_shared<int> (0);
};
} // namespace duet::app
