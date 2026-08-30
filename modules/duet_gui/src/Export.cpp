#include <duet/gui/Export.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace duet::gui
{
namespace
{
    /** The depths each format can be written at, deepest last.

        Stated here rather than asked of the format, because this module links
        no engine and no JUCE: what a format can hold is a fact about the format
        and not about the machine. The model clamps to the same table on its own
        side, so a depth that got past this one still could not reach a file
        that cannot hold it.
    */
    const std::vector<int>& depthsFor (duet::model::ExportFormat format)
    {
        static const std::vector<int> uncompressed { 16, 24, 32 };
        static const std::vector<int> lossless { 16, 24 };

        return format == duet::model::ExportFormat::flac ? lossless : uncompressed;
    }

    /** What the file is called after the name the producer typed. */
    const char* extensionFor (duet::model::ExportFormat format)
    {
        switch (format)
        {
            case duet::model::ExportFormat::aiff:
                return ".aiff";
            case duet::model::ExportFormat::flac:
                return ".flac";
            case duet::model::ExportFormat::wav:
                break;
        }

        return ".wav";
    }

    /** The depth the format can write that is nearest below the one asked for,
        and its shallowest when it can write nothing that shallow.
    */
    int nearestDepth (duet::model::ExportFormat format, int wanted)
    {
        const auto& depths = depthsFor (format);

        if (std::ranges::find (depths, wanted) != depths.end())
            return wanted;

        auto best = depths.front();

        for (const auto depth : depths)
            if (depth <= wanted && depth > best)
                best = depth;

        return best;
    }

    /** Where a run still in flight goes when the interface that started it does.

        Nothing here is ever put down. A render's copy of the project is taken
        down on the message thread, so a worker still inside one is a worker the
        message thread has to serve — and this is the message thread on its way
        out, with no loop left to serve it. Waiting would hang the quit and
        putting the run down would free, on the rendering thread, a project only
        the message thread may free. The process is ending, and what is left
        here goes with it.
    */
    std::vector<std::shared_ptr<void>>& abandonedRuns()
    {
        static auto* abandoned = new std::vector<std::shared_ptr<void>>;

        return *abandoned;
    }
} // namespace

//==============================================================================
Export::~Export()
{
    cancel();
    putDownWhatIsFinishedWith();

    if (current != nullptr)
        retired.push_back (std::exchange (current, nullptr));

    for (auto& run : retired)
        abandonedRuns().push_back (std::move (run));
}

void Export::setProject (std::shared_ptr<duet::model::Session> openProject,
                         std::string_view name,
                         std::filesystem::path folderOfProject)
{
    // An export of the project that is going goes on reading the project it
    // took: it holds one, and this returns to the producer at once.
    retireCurrentRun();

    session = std::move (openProject);
    projectName = std::string { name };
    projectFolder = std::move (folderOfProject);

    restoreDefaults();
}

void Export::restoreDefaults()
{
    retireCurrentRun();

    fileName = projectName;
    folder = projectFolder;
    chosenFormat = duet::model::ExportFormat::wav;
    chosenBitDepth = 24;
    chosenSampleRate = duet::model::renderSampleRate;
    normalising = false;

    rangeFirstBar = 1;
    rangeLastBar = 1;

    if (session == nullptr)
        return;

    // The bars the content actually reaches: the last bar it plays into, and
    // never fewer than the one it starts in. A project with nothing in it is one
    // bar long, which is what a producer who exports an empty project gets.
    const auto reaches = session->barAtSeconds (session->editLengthSeconds());
    rangeLastBar = std::max (1, static_cast<int> (std::ceil (reaches)) - 1);
}

//==============================================================================
void Export::setName (std::string_view newName) { fileName = std::string { newName }; }

void Export::setDestinationFolder (std::filesystem::path newFolder)
{
    folder = std::move (newFolder);
}

std::filesystem::path Export::destination() const
{
    if (fileName.empty() || folder.empty())
        return {};

    return folder / (fileName + extensionFor (chosenFormat));
}

void Export::setFormat (duet::model::ExportFormat newFormat)
{
    chosenFormat = newFormat;

    // A depth the new format cannot write is not a choice the producer made: it
    // is the one they made about the old format, carried where it does not fit.
    chosenBitDepth = nearestDepth (chosenFormat, chosenBitDepth);
}

std::vector<int> Export::availableBitDepths() const { return depthsFor (chosenFormat); }

void Export::setBitDepth (int newBitDepth)
{
    chosenBitDepth = nearestDepth (chosenFormat, newBitDepth);
}

const std::vector<double>& Export::availableSampleRates()
{
    static const std::vector<double> rates { 44100.0, 48000.0, 88200.0, 96000.0 };

    return rates;
}

void Export::setSampleRate (double newSampleRate)
{
    const auto& rates = availableSampleRates();

    if (std::ranges::find (rates, newSampleRate) != rates.end())
        chosenSampleRate = newSampleRate;
}

//==============================================================================
void Export::setRange (int first, int last)
{
    rangeFirstBar = std::max (1, first);
    rangeLastBar = std::max (rangeFirstBar, last);
}

double Export::startSeconds() const
{
    return session != nullptr ? session->barStartSeconds (rangeFirstBar) : 0.0;
}

double Export::endSeconds() const
{
    // The last bar is in the range, so the range ends where the bar after it
    // starts: an export of bars 1 to 4 holds all four.
    return session != nullptr ? session->barStartSeconds (rangeLastBar + 1) : 0.0;
}

duet::model::ExportOptions Export::options() const
{
    duet::model::ExportOptions asked;
    asked.destination = destination();
    asked.format = chosenFormat;
    asked.bitDepth = chosenBitDepth;
    asked.sampleRate = chosenSampleRate;
    asked.startSeconds = startSeconds();
    asked.endSeconds = endSeconds();
    asked.normalise = normalising;

    return asked;
}

//==============================================================================
bool Export::start()
{
    if (isRunning() || session == nullptr)
        return false;

    const auto asked = options();

    if (asked.destination.empty() || ! (asked.endSeconds > asked.startSeconds))
        return false;

    putDownWhatIsFinishedWith();

    auto run = std::make_shared<Run>();
    run->project = session;
    run->asked = asked;
    current = run;

    // Detached, and holding everything it reads. Nothing here joins a render:
    // the copy it renders is taken down on the message thread, so a wait for one
    // is the message thread waiting for itself (issue 9tdwdq).
    std::thread {
        [run]
        {
            const auto wrote = run->project->exportToFile (run->asked,
                                                           [&run] (double proportion)
                                                           {
                                                               run->howFar = proportion;

                                                               return ! run->cancelling;
                                                           });

            if (wrote)
            {
                run->written = run->asked.destination;
                run->howFar = 1.0;
            }

            // The state is set last, and the running flag after it: a dialog
            // that saw "written" before the file's name was in place would draw
            // a moment that never happened.
            if (wrote)
                run->state = ExportState::written;
            else if (run->cancelling)
                run->state = ExportState::cancelled;
            else
                run->state = ExportState::failed;

            run->running = false;
        }
    }.detach();

    return true;
}

void Export::cancel()
{
    if (current != nullptr)
        current->cancelling = true;

    for (const auto& run : retired)
        run->cancelling = true;
}

bool Export::isRunning() const { return current != nullptr && current->running; }

ExportState Export::state() const
{
    return current != nullptr ? current->state.load() : ExportState::idle;
}

double Export::progress() const { return current != nullptr ? current->howFar.load() : 0.0; }

std::filesystem::path Export::writtenFile() const
{
    if (current == nullptr || current->state != ExportState::written)
        return {};

    return current->written;
}

//==============================================================================
void Export::retireCurrentRun()
{
    putDownWhatIsFinishedWith();

    if (current == nullptr)
        return;

    current->cancelling = true;
    retired.push_back (std::exchange (current, nullptr));
}

void Export::putDownWhatIsFinishedWith()
{
    // This list is the only hold left on a run whose worker has let go of it,
    // so a count of one is what says the render is over.
    const auto finished = std::ranges::remove_if (
        retired, [] (const std::shared_ptr<Run>& run) { return run.use_count() == 1; });

    retired.erase (finished.begin(), finished.end());
}
} // namespace duet::gui
