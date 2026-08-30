#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace duet::gui
{
/** What the producer had selected, as the Collaborator panel names it.

    The panel never reads the project: the shell hands it the selection in this
    engine-free shape, and the panel freezes a copy onto each message the
    producer sends. That is what makes the chip on a sent message a record of
    the moment rather than a live readout.
*/
enum class SelectionScope : std::uint8_t
{
    nothing,
    clips,
    track
};

struct SelectionContext
{
    SelectionScope scope = SelectionScope::nothing;

    /** How many clips, when clips are what is selected. */
    int clipCount = 0;

    /** What the one thing is called: the clip's name when it is one clip, the
        track's name when it is a track. Empty for several clips, which are not
        one thing to name — what shows them is the count.
    */
    std::string name;

    friend bool operator== (const SelectionContext& first,
                            const SelectionContext& second) = default;
};

[[nodiscard]] SelectionContext noSelection();
[[nodiscard]] SelectionContext clipSelected (std::string name);
[[nodiscard]] SelectionContext clipsSelected (int count);
[[nodiscard]] SelectionContext trackSelected (std::string name);

/** What the chip on a sent message reads, and empty when nothing was selected. */
[[nodiscard]] std::string contextChipText (const SelectionContext& context);

/** One line of a run's estimate ledger, as the panel reads it: what was
    guessed, what the guess was, the routine that made it, and how far that
    routine trusts itself, from 0 to 1.

    The panel takes it in this shape rather than the service's own, because the
    ledger lives in a module this one cannot name.
*/
struct EstimateMarkLine
{
    std::string field;
    std::string value;
    std::string method;
    double confidence = 0.0;
};

/** One call of the Tool Vocabulary, as the development trace keeps it: the
    tool's name, the arguments it was called with, and what it answered, each as
    the text that crossed the seam.
*/
struct ToolTraceEntry
{
    std::string tool;
    std::string arguments;
    std::string result;
};

/** One Suggestion the producer has finished with, as the History section reads
    it: what it was called, and where it went.
*/
struct ResolvedSuggestion
{
    std::string summary;
    std::string outcome;
};

/** Whether this is a development build.

    The raw tool-call trace exists to debug the Collaborator, and the Target
    Producer reads the rotating status phrases instead (spec js437t), so what
    the trace defaults to is what kind of build this is.
*/
inline constexpr bool developmentBuild =
#ifdef DUET_DEVELOPMENT_BUILD
    true;
#else
    false;
#endif

/** Who an entry in the conversation is from, which is what decides how it is
    drawn: the producer's messages plain, the Collaborator's commentary in an
    accent bubble, and the two endings a Task Run can have as plain lines.
*/
enum class EntryKind : std::uint8_t
{
    producer,
    commentary,
    notice,
    failure,

    /** A Suggestion the Collaborator has made: the card the producer ticks,
        auditions, accepts or rejects, in the conversation where it was asked
        for.
    */
    suggestion
};

struct ConversationEntry
{
    EntryKind kind = EntryKind::producer;
    std::string text;

    /** The chip, frozen at the moment the message was sent. Empty on everything
        but a producer message sent with something selected.
    */
    std::string context;

    /** Which Suggestion this entry is the card of. Empty on every other kind of
        entry.
    */
    std::string suggestion;

    /** The ledger of the run that said this, when what it says rests on a
        guess, and empty when it does not.

        Non-empty *is* the estimate mark: the service reads it off the run's own
        ledger rather than off anything the model said about itself (spec
        js437t), so nothing can say a marked thing unmarked.
    */
    std::vector<EstimateMarkLine> estimates;

    /** Whether the producer has opened the mark onto the ledger behind it. */
    bool estimatesOpen = false;
};

/** The Collaborator panel, without the painting.

    The conversation, the composer under it, and the Task Run card over that: the
    panel's own state, and nothing about how the Collaborator works. What runs a
    Task Run lives behind `Source`, which the Collaborator service is what
    implements: the panel holds what was said and the source runs the runs, so
    neither knows how the other works.
*/
class CollaboratorPanel
{
public:
    CollaboratorPanel() = default;

    //==============================================================================
    /** What answers the producer: the Collaborator service, through whatever
        puts a Task Run's events on the message thread.
    */
    class Source
    {
    public:
        virtual ~Source() = default;

        Source (const Source& other) = delete;
        Source& operator= (const Source& other) = delete;

        /** The producer has sent a message, which is already in the panel. */
        virtual void producerSent (CollaboratorPanel& panel, const std::string& message) = 0;

        /** The producer has canceled the run. The panel has already ended it. */
        virtual void taskRunCanceled (CollaboratorPanel& panel) = 0;

    protected:
        Source() = default;
    };

    /** The source is read and never owned, and none is a panel that shows
        whatever it is told and answers nothing.
    */
    void setSource (Source* newSource) { source = newSource; }

    //==============================================================================
    /** Everything said so far, oldest first. */
    [[nodiscard]] const std::vector<ConversationEntry>& conversation() const { return entries; }

    //==============================================================================
    /** What the shell says is selected right now. */
    void setSelectionContext (SelectionContext context);
    [[nodiscard]] const SelectionContext& selectionContext() const { return selection; }

    //==============================================================================
    /** The keyboard belongs in the composer: what an ask from a clip's or a
        track's context menu leaves behind, so that the producer types straight
        into it and nothing is sent for them (spec js437t, story 9).

        The panel keeps the intent and the window keeps the focus: the canvas
        hands the keyboard over while this is true, and says when the producer
        has taken it somewhere else.
    */
    void focusComposer();
    void composerLostKeyboard();
    [[nodiscard]] bool composerWantsKeyboard() const { return composerFocus; }

    //==============================================================================
    /** The composer's text, as the producer has it. */
    void setComposerText (std::string text);
    [[nodiscard]] const std::string& composerText() const { return composer; }

    /** Whether sending would do anything: a composer holding more than blanks,
        no run holding it, and a model to run it on. One run at a time is the
        spec's rule and this is where the panel keeps it — a send while a run is
        on starts nothing, and neither does a send with nothing set up.
    */
    [[nodiscard]] bool canSend() const;

    //==============================================================================
    /** Whether the producer has to set a model provider up before the
        Collaborator can answer them at all.

        Nothing configured is not an error and is not a failed run: it is a state
        the panel shows, with the way to the settings surface in it, and the
        composer held while it lasts — sending is not offered as an action that
        would fail (spec js437t). The picker is what decides this; the panel is
        told.
    */
    void setSetupRequired (bool nothingConfigured);
    [[nodiscard]] bool setupRequired() const { return setup; }

    /** What the setup state says, and what the way out of it is called. */
    static constexpr const char* setupNotice =
        "The Collaborator needs a model provider before it can answer.";
    static constexpr const char* setupAction = "Set up a provider";

    /** Sends what the composer holds, chipped with what is selected now. Does
        nothing when there is nothing to send.
    */
    void send();

    /** One more piece of the Collaborator's commentary, as it streams.

        The first delta of a run opens the entry and every one after it extends
        the same entry, so the producer reads the answer while it is being
        written rather than when it is finished.

        `estimates` is that run's ledger as the service read it when the delta
        was sent: empty while the run has been handed no guess, and the mark on
        the entry — with what the mark opens onto — once it has. A run that is
        handed a guess halfway through marks the whole of what it said, the
        entry being one thing the producer reads.
    */
    void streamCommentary (const std::string& delta, std::vector<EstimateMarkLine> estimates);

    /** Opens or closes the estimate mark on one entry, which is what showing the
        ledger behind it is. An entry that carries no mark ignores this.
    */
    void toggleEstimates (std::size_t entry);

    /** Puts a Suggestion's card in the conversation, where the producer asked
        for it. What the card then shows is the Suggestion's, not the panel's:
        this is the place it sits in and nothing more.

        `revises` names the Suggestion this one was asked to improve on, and a
        card of that Suggestion is replaced where it stands rather than joined:
        a producer who rejected one Suggestion with a reason, or asked for the
        same thing again, asked one question and is owed one card (spec
        js437t). Empty, or naming a Suggestion this conversation never showed,
        and the card is a new one at the end.
    */
    void showSuggestion (std::string id, std::string summary, const std::string& revises = {});

    //==============================================================================
    /** The raw tool-call trace of the run on now, or of the last one there was.

        Every call the Collaborator made, in the order it made them, with what it
        asked and what it was answered. It is the developer's window into a run
        and nobody else's: the Target Producer reads the status phrases instead
        (spec js437t), so an ordinary build keeps none of it.
    */
    [[nodiscard]] const std::vector<ToolTraceEntry>& toolTrace() const { return trace; }

    /** Keeps one call in the trace. Keeps nothing at all when the trace is off,
        which is what makes it absent from an ordinary build rather than merely
        unshown.
    */
    void recordToolCall (std::string tool, std::string arguments, std::string result);

    /** Whether this panel keeps a trace, which a development build is what
        decides. A test is the other thing that sets it, because the rule is
        about both kinds of build and only one of them is ever compiled here.
    */
    void setToolTraceEnabled (bool shouldKeepTrace);
    [[nodiscard]] bool toolTraceEnabled() const { return tracing; }

    //==============================================================================
    /** The History section: the Suggestions of this app session the producer has
        finished with, oldest first.

        It is the Suggestion manager's resolved list and nothing of the panel's
        own, so it is handed over rather than worked out here, and it dies with
        the app like everything else about a Suggestion (spec js437t).
    */
    void setHistory (std::vector<ResolvedSuggestion> resolvedSuggestions);
    [[nodiscard]] const std::vector<ResolvedSuggestion>& history() const { return resolved; }

    //==============================================================================
    /** The chips above the composer: the openings that are worth offering for
        what is selected, which is why they change with it — a clip selection,
        a track selection and an empty selection each have their own set.
    */
    [[nodiscard]] std::vector<std::string> quickPrompts() const;

    /** Puts a chip's text in the composer, and stops there: a quick prompt is
        an opening the producer edits, not a message.
    */
    void useQuickPrompt (std::size_t index);

    //==============================================================================
    /** The Task Run card: a run the producer can keep editing around and can
        cancel, and the friendly phrases that stand in for a progress bar while
        it lasts.

        The panel does not run anything: a run is begun and ended from outside
        it, by whatever carries the service's events here, so the card is the
        record of a run and never the run itself.
    */
    void beginTaskRun();

    /** The three ways a run ends. Each re-enables the composer; the last two
        leave a plain line saying so.
    */
    void finishTaskRun();
    void cancelTaskRun();
    void failTaskRun (const std::string& reason);

    /** What the Cancel button on the card does: ends the run and tells the
        source, so that a run the producer stopped never comes back to finish
        itself.
    */
    void requestCancel();

    [[nodiscard]] bool taskRunning() const { return running; }

    /** The phrase on the card, and empty when no run is on. */
    [[nodiscard]] const std::string& statusPhrase() const { return phrase; }

    /** Carries the run's clock forward, which is what rotates the phrase. The
        panel keeps no clock of its own so that a rotation can be asserted with
        no window on screen and no waiting.
    */
    void advance (double seconds);

    /** Whether the producer can type and send: a Task Run holds the composer,
        and everything else in the app stays theirs while it runs.
    */
    [[nodiscard]] bool composerEnabled() const { return ! running; }

    /** How long one status phrase stands before the next one takes its turn. */
    static constexpr double statusPhraseSeconds = 2.5;

    /** What a canceled run leaves behind. */
    static constexpr const char* cancelNotice = "Task canceled, nothing changed.";

    /** What a failed run leaves behind when nothing said why. A run that
        reached the Collaborator carries its own sentence instead.
    */
    static constexpr const char* failureNotice = "That task failed. Nothing changed.";

    /** What the card says the producer may do while a run is on. */
    static constexpr const char* keepEditingHint = "You can keep editing while this runs.";

private:
    void endRun();

    std::vector<ConversationEntry> entries;
    std::vector<ToolTraceEntry> trace;
    std::vector<ResolvedSuggestion> resolved;
    SelectionContext selection;
    std::string composer;
    bool tracing = developmentBuild;
    bool running = false;
    bool setup = false;
    bool composerFocus = false;

    /** Which entry the commentary streaming now is extending, and none when the
        run has said nothing yet.
    */
    std::size_t streamingInto = 0;
    bool streaming = false;
    std::size_t phraseIndex = 0;
    double phraseElapsed = 0.0;
    std::string phrase;
    Source* source = nullptr;
};
} // namespace duet::gui
