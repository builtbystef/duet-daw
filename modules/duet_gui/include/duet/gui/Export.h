#pragma once

#include <duet/model/Session.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace duet::gui
{
/** Where an export has got to.

    One value, read from the message thread while the render runs somewhere
    else, which is what the dialog draws itself from.
*/
enum class ExportState : std::uint8_t
{
    /** Nothing has been asked for yet, or the dialog has been reopened. */
    idle,

    running,

    /** The file is on disk. */
    written,

    /** The producer cancelled, and nothing was left behind. */
    cancelled,

    /** Nothing could be written — no project, a range with nothing in it, or a
        destination that could not be opened.
    */
    failed
};

/** The Export/Bounce dialog, without the painting: what the producer chooses,
    and the render that choice starts.

    The fields are the ones spec 535bbo names — name, destination, format, bit
    depth, sample rate, the range of bars, and normalise — and they default to
    the project: the name it is called, the folder it is, and the bars its
    content actually reaches. A producer who opens the dialog and presses Export
    gets their whole project under its own name.

    The render is not the message thread's. `start` hands it to a worker and
    returns, the dialog reads `progress` and `state` while it runs, and `cancel`
    is taken between blocks — which is what keeps the app responsive through an
    export and what leaves nothing behind when one is abandoned.

    **What an export holds.** A render's copy of the project is made and taken
    down on the message thread, so a message thread waiting for a render to stop
    is a message thread waiting for itself (issue 9tdwdq). Nothing here ever
    waits for one: an export holds what it reads — the project included, through
    the hold `setProject` takes — and closing the project it was started against
    lets it finish reading the project it entered. A run that has finished is put
    down here, on the message thread, which is the only thread that may put a
    project down.
*/
class Export
{
public:
    Export() = default;
    ~Export();

    Export (const Export& other) = delete;
    Export& operator= (const Export& other) = delete;

    //==============================================================================
    /** The project to export, what the producer calls it, and the folder it is.

        Called as a project opens and again as one closes, with nothing. Every
        default is taken from the project here and then, so a dialog opened
        afterwards is already the answer for this project.

        A hold and not a pointer, for the reason above: an export already
        running goes on reading the project it took, and this returns at once.
    */
    void setProject (std::shared_ptr<duet::model::Session> openProject,
                     std::string_view name,
                     std::filesystem::path folderOfProject);

    /** Puts every field back to what the open project makes of it: what opening
        the dialog on a project the producer has since edited shows.
    */
    void restoreDefaults();

    //==============================================================================
    /** What the file will be called, without the extension — the format decides
        that.
    */
    [[nodiscard]] const std::string& name() const { return fileName; }
    void setName (std::string_view newName);

    /** The folder the file goes in. */
    [[nodiscard]] const std::filesystem::path& destinationFolder() const { return folder; }
    void setDestinationFolder (std::filesystem::path newFolder);

    /** Where the file lands: the folder, the name, and the format's own
        extension. Empty while there is nothing to write.
    */
    [[nodiscard]] std::filesystem::path destination() const;

    [[nodiscard]] duet::model::ExportFormat format() const { return chosenFormat; }
    void setFormat (duet::model::ExportFormat newFormat);

    /** The depths the chosen format can be written at, deepest last. */
    [[nodiscard]] std::vector<int> availableBitDepths() const;

    [[nodiscard]] int bitDepth() const { return chosenBitDepth; }

    /** Takes the nearest depth the format can write, so a producer who moves
        from WAV to FLAC keeps a depth that exists rather than one that does not.
    */
    void setBitDepth (int newBitDepth);

    /** The rates the dialog offers. */
    [[nodiscard]] static const std::vector<double>& availableSampleRates();

    [[nodiscard]] double sampleRate() const { return chosenSampleRate; }
    void setSampleRate (double newSampleRate);

    [[nodiscard]] bool normalise() const { return normalising; }
    void setNormalise (bool shouldNormalise) { normalising = shouldNormalise; }

    //==============================================================================
    /** The range, in bars the producer counts from one, both ends included: an
        export of bars 1 to 4 holds all four of them.
    */
    [[nodiscard]] int firstBar() const { return rangeFirstBar; }
    [[nodiscard]] int lastBar() const { return rangeLastBar; }

    /** A range whose last bar is before its first is a range of the one bar it
        starts in, and a bar below one is bar one: the producer types into these
        fields, and a typed range is never a reason to write nothing.
    */
    void setRange (int first, int last);

    /** What the range is worth in seconds, which is what the render is asked
        for.
    */
    [[nodiscard]] double startSeconds() const;
    [[nodiscard]] double endSeconds() const;

    /** Everything above, as the model takes it. */
    [[nodiscard]] duet::model::ExportOptions options() const;

    //==============================================================================
    /** Starts the export on a worker thread and returns at once. False when
        there is nothing to export, or when one is already running.
    */
    bool start();

    /** Asks a running export to stop. It stops between blocks, so this returns
        before it has; `state` is what says it did.
    */
    void cancel();

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] ExportState state() const;

    /** How far the render has got, from zero to one. */
    [[nodiscard]] double progress() const;

    /** The file the last finished export wrote, and empty when the last one
        wrote nothing.
    */
    [[nodiscard]] std::filesystem::path writtenFile() const;

private:
    /** One export, and everything the worker running it reads.

        Held by the worker as well as by this object, so that what it reads
        outlives a project the producer closed under it, and so that nothing
        about it is destroyed on the thread that was rendering it.
    */
    struct Run
    {
        std::shared_ptr<duet::model::Session> project;
        duet::model::ExportOptions asked;
        std::filesystem::path written;
        std::atomic<bool> cancelling { false };
        std::atomic<bool> running { true };
        std::atomic<double> howFar { 0.0 };
        std::atomic<ExportState> state { ExportState::running };
    };

    /** Puts down every run the worker has let go of. Called from the message
        thread, which is the only thread that may put a project down.
    */
    void putDownWhatIsFinishedWith();

    /** Retires the run in flight, if there is one, and stops asking about it. */
    void retireCurrentRun();

    std::shared_ptr<duet::model::Session> session;
    std::string projectName;
    std::filesystem::path projectFolder;

    std::string fileName;
    std::filesystem::path folder;
    duet::model::ExportFormat chosenFormat = duet::model::ExportFormat::wav;
    int chosenBitDepth = 24;
    double chosenSampleRate = duet::model::renderSampleRate;
    bool normalising = false;
    int rangeFirstBar = 1;
    int rangeLastBar = 1;

    /** The export in flight, or the last one that ran. */
    std::shared_ptr<Run> current;

    /** The runs a project swap left behind, kept until their workers have let
        go of them.
    */
    std::vector<std::shared_ptr<Run>> retired;
};
} // namespace duet::gui
