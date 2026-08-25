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

    /** Which track, when a track is what is selected. */
    std::string trackName;

    friend bool operator== (const SelectionContext& first,
                            const SelectionContext& second) = default;
};

[[nodiscard]] SelectionContext noSelection();
[[nodiscard]] SelectionContext clipsSelected (int count);
[[nodiscard]] SelectionContext trackSelected (std::string name);

/** What the chip on a sent message reads, and empty when nothing was selected. */
[[nodiscard]] std::string contextChipText (const SelectionContext& context);

/** Who an entry in the conversation is from, which is what decides how it is
    drawn: the producer's messages plain, the Collaborator's commentary in an
    accent bubble, and the two endings a Task Run can have as plain lines.
*/
enum class EntryKind : std::uint8_t
{
    producer,
    commentary,
    notice,
    failure
};

struct ConversationEntry
{
    EntryKind kind = EntryKind::producer;
    std::string text;

    /** The chip, frozen at the moment the message was sent. Empty on everything
        but a producer message sent with something selected.
    */
    std::string context;
};

/** The Collaborator panel, without the painting.

    The conversation, the composer under it, and the Task Run card over that: the
    panel's own state, and nothing about how the Collaborator works. What runs a
    Task Run lives behind `Source`, so that the panel is complete and reviewable
    before the service of spec js437t exists.
*/
class CollaboratorPanel
{
public:
    CollaboratorPanel() = default;

    //==============================================================================
    /** What answers the producer: the Collaborator, once spec js437t's service
        exists, and until then the development source beneath this class.

        The panel holds the conversation and the source runs the Task Runs, so
        neither knows how the other works and the panel is complete without one.
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

        /** Carries the source's own clock forward, alongside the panel's. */
        virtual void advance (CollaboratorPanel& panel, double seconds) = 0;

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
    /** The composer's text, as the producer has it. */
    void setComposerText (std::string text);
    [[nodiscard]] const std::string& composerText() const { return composer; }

    /** Whether sending would do anything: a composer holding more than blanks. */
    [[nodiscard]] bool canSend() const;

    /** Sends what the composer holds, chipped with what is selected now. Does
        nothing when there is nothing to send.
    */
    void send();

    /** The Collaborator's commentary. */
    void say (std::string commentary);

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

        The panel does not run anything — a run is begun and ended from outside
        it, which is what lets the whole card be reviewed before the service of
        spec js437t exists.
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

    /** What the card says the producer may do while a run is on. */
    static constexpr const char* keepEditingHint = "You can keep editing while this runs.";

private:
    void endRun();

    std::vector<ConversationEntry> entries;
    SelectionContext selection;
    std::string composer;
    bool running = false;
    std::size_t phraseIndex = 0;
    double phraseElapsed = 0.0;
    std::string phrase;
    Source* source = nullptr;
};

/** The development-only source that stands in for the Collaborator.

    It speaks to no AI backend, opens no socket and reaches no network: it
    answers a sent message with a Task Run that lasts `taskRunSeconds` and then
    either says something or fails, alternating between the two so that both
    endings — and, with the Cancel button, the third — are reachable by hand
    before the service of spec js437t exists. It goes when that service arrives.
*/
class ScriptedCollaborator final : public CollaboratorPanel::Source
{
public:
    ScriptedCollaborator() = default;

    void producerSent (CollaboratorPanel& panel, const std::string& message) override;
    void taskRunCanceled (CollaboratorPanel& panel) override;
    void advance (CollaboratorPanel& panel, double seconds) override;

    /** How long a scripted Task Run lasts: long enough to read the card and to
        cancel it, short enough that a reviewer is not kept waiting.
    */
    static constexpr double taskRunSeconds = 6.0;

private:
    double remaining = 0.0;
    bool running = false;
    std::size_t runsSoFar = 0;
};
} // namespace duet::gui
