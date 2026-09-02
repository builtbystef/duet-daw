#pragma once

#include <duet/gui/TimelineGeometry.h>

#include <duet/model/Session.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace duet::gui
{
class Settings;

/** What putting an item into the project does.

    A sample becomes a clip; an instrument is what a MIDI track plays; an effect
    goes into a chain. Whether Duet ships the device or the producer's machine
    does is not this: a scanned VST3 is inserted exactly as a built-in is (spec
    535bbo), so both are the same two kinds here.
*/
enum class BrowserItemKind : std::uint8_t
{
    sample,
    instrument,
    effect
};

/** Which of the browser's sections one is. */
enum class BrowserSectionKind : std::uint8_t
{
    favourites,
    instruments,
    effects,
    plugins,
    samples
};

/** One thing the producer can put into the project, as the dock shows it. */
struct BrowserItem
{
    BrowserItemKind kind = BrowserItemKind::sample;

    /** What the dock calls it, and what a search matches against. */
    std::string name;

    /** What a favourite is remembered by and what a drag carries: stable for as
        long as the thing it names is where it is.
    */
    std::string identity;

    /** Which built-in this is, and nothing for a sample or a scanned VST3. */
    std::optional<duet::model::BuiltinPlugin> builtin;

    /** The app-global identifier of a scanned VST3, and empty for the rest. */
    std::string pluginIdentifier;

    /** The audio file, for a sample, and empty for the rest. */
    std::filesystem::path file;

    bool favourite = false;
    bool selected = false;
};

/** One of the dock's sections and what is under it. */
struct BrowserSection
{
    BrowserSectionKind kind = BrowserSectionKind::samples;
    std::string name;

    /** What the producer's choice to open or close this section is remembered
        by, so that clearing a search restores the tree they had.
    */
    std::string identity;

    /** The folder this section lists, for a sample folder, and empty for the
        rest.
    */
    std::filesystem::path folder;

    bool expanded = false;
    std::vector<BrowserItem> items;

    /** A local status for this section — an unreadable subtree — and empty
        when the last scan of it was clean.
    */
    std::string status;
};

/** One sample folder as a walk found it. */
struct SampleFolderScan
{
    std::filesystem::path folder;
    std::vector<BrowserItem> items;

    /** One local status for an unreadable subtree; empty when the walk was
        clean. Readable siblings are still in items.
    */
    std::string status;
};

/** What a host-supplied worker is asked to scan.

    The generation tags the request so a result that outlived a newer refresh
    is dropped rather than drawn.
*/
struct SampleFolderScanRequest
{
    std::uint64_t generation = 0;
    std::vector<std::filesystem::path> folders;
};

/** How far one generation has got, as the worker reports it on the message
    thread.
*/
struct SampleFolderScanProgress
{
    std::uint64_t generation = 0;
    std::size_t completed = 0;
    std::size_t known = 0;
};

/** The finished walk of one generation. */
struct SampleFolderScanOutcome
{
    std::uint64_t generation = 0;
    std::vector<SampleFolderScan> folders;
};

/** Busy, progress and the Scanning… message, as the dock reads them.

    Existing rows stay up while this is busy; cancellation does not write a
    status of its own.
*/
struct BrowserScanSnapshot
{
    bool busy = false;
    std::size_t completed = 0;
    std::size_t known = 0;
    std::string message;
};

/** What Source audition is doing, as the Browser and its surface read it.

    This is not Suggestion Audition: it names a Browser sample playing through
    the main output, independent of the project transport and undo history.
*/
enum class SourceAuditionState : std::uint8_t
{
    stopped,
    loading,
    playing,
    error
};

struct SourceAuditionStatus
{
    SourceAuditionState state = SourceAuditionState::stopped;
    double progress = 0.0;
    std::string error;
    std::string identity;
};

/** The engine-free controller the Browser talks to for Source audition.

    duet_app owns the one player behind this; the dock never names an engine
    type. Decode happens off this interface; the player reports loading,
    playing, progress, stopped, and a plain row-local error.
*/
class SourceAudition
{
public:
    virtual ~SourceAudition() = default;

    SourceAudition (const SourceAudition&) = delete;
    SourceAudition& operator= (const SourceAudition&) = delete;

    virtual void play (std::filesystem::path file, std::string identity) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual SourceAuditionStatus status() const = 0;
    virtual void onChanged (std::function<void()> callback) = 0;

protected:
    SourceAudition() = default;
};

/** Walks one sample folder: ordinary directories, no directory-symlink
    following, only sampleExtensions() regular files, sorted
    case-insensitively by relative path with the bytewise path as the
    tie-breaker.
*/
[[nodiscard]] SampleFolderScan scanSampleFolder (const std::filesystem::path& folder,
                                                 const std::function<bool()>& cancelled = {});

/** The left dock, without the painting: everything the producer can put into a
    project, and what dropping one of them does.

    The sample folders and the favourites are the producer's rather than the
    project's, so they live in the app-global store and follow them between
    projects (spec 535bbo). What is on disk is read by a host-supplied worker
    and installed on the message thread, never while a surface paints: a sample
    folder is a tree of the producer's own, and reading it is the one expensive
    thing here.

    Every drop ends in one Action on the vocabulary layer, and a drop that has
    nowhere to land does nothing at all — an instrument has no meaning on an
    audio track, and cancelling is the whole of what happens (ADR 0004).
*/
class Browser
{
public:
    /** @param store  the app-global store the folders and the favourites live in
    */
    explicit Browser (Settings& store);

    ~Browser() = default;

    Browser (const Browser& other) = delete;
    Browser& operator= (const Browser& other) = delete;

    //==============================================================================
    /** The project a drop edits, or nothing while none is open. */
    void setSession (duet::model::Session* openProject);

    /** How a dropped sample gets into the project folder, so that the clip made
        of it refers to the project's own copy and the folder stays
        self-contained (ADR 0005).

        The persistence facade is what copies it, and the browser is a view-model
        that does not know one, so the host hands the import over as this. A
        browser with none inserts the file where it lies.
    */
    void setSampleImporter (
        std::function<std::filesystem::path (const std::filesystem::path&)> importIntoProject);

    /** The host-supplied walk. A refresh asks it to scan and does not walk the
        disk itself; the worker delivers a generation-tagged result on the
        message thread. A browser with none walks on the caller's thread, which
        is what a test without a worker needs.
    */
    void setScanWorker (std::function<void (const SampleFolderScanRequest&)> worker);

    /** Installs progress from the worker. A generation that is no longer the
        current refresh is ignored.
    */
    void applyScanProgress (const SampleFolderScanProgress& progress);

    /** Installs a finished walk. A stale generation does not replace a newer
        one; cancellation is otherwise silent.
    */
    void applyScanOutcome (const SampleFolderScanOutcome& outcome);

    /** Busy, the completed/known count, and the Scanning… message. */
    [[nodiscard]] BrowserScanSnapshot scanSnapshot() const;

    /** The one Source audition player, or nothing while the host has not
        handed one over. The dock never owns it.
    */
    void setSourceAudition (SourceAudition* player);

    /** The selected item, by identity. Selecting a sample while Source audition
        is playing switches to that sample; selecting anything else stops it.
    */
    void select (std::string_view identity);
    [[nodiscard]] const std::string& selected() const { return selectedIdentity; }

    /** Browser-focused Space and the visible Play/Stop button: plays the
        selected sample, or stops it if that sample is already playing.
    */
    void toggleSourceAudition();

    /** Stops Source audition without changing the selection: project
        replacement, Browser close, device loss, and shutdown all land here.
    */
    void stopSourceAudition();

    /** Loading, playing, progress 0..1, stopped, or a plain row-local error. */
    [[nodiscard]] SourceAuditionStatus sourceAuditionStatus() const;

    /** Calls back after every change to what the browser shows — a folder, a
        favourite, a search, a rescan — so that the surface drawing it does not
        have to be told twice by whoever made the change. One callback at a
        time, the dock being the one surface that draws this.
    */
    void onChanged (std::function<void()> callback);

    /** Reads the scanned plugin list again and starts a new sample-folder
        generation.

        What is on disk changes without Duet asking, and so does the plugin list
        — a rescan is a producer gesture — so this is how the dock learns of it,
        and it is why a rescan needs no restart. Existing sample rows stay up
        until the new generation lands.
    */
    void refresh();

    //==============================================================================
    /** The folders the producer has chosen, in the order they chose them. */
    [[nodiscard]] std::vector<std::filesystem::path> sampleFolders() const;

    /** Adds a folder to the browser and to the next launch. A folder that is
        already there is not added twice.
    */
    void addSampleFolder (const std::filesystem::path& folder);

    void removeSampleFolder (const std::filesystem::path& folder);

    //==============================================================================
    /** Filters every section down to what matches, and hides the sections that
        match nothing. An empty search is the whole tree again, opened as the
        producer had it.
    */
    void setSearch (std::string_view text);
    [[nodiscard]] const std::string& search() const { return searchText; }

    /** The sections to draw, in the order the dock shows them. */
    [[nodiscard]] std::vector<BrowserSection> sections() const;

    /** The item one identity names, and nothing when the browser no longer has
        it — which is what a drag that outlived a refresh carries.
    */
    [[nodiscard]] std::optional<BrowserItem> item (std::string_view identity) const;

    //==============================================================================
    /** Opens or closes a section. A sample folder starts closed — a sample
        library is the one section that can hold thousands of things — and the
        four device sections start open. What the producer changes stands for
        the run, so clearing a search puts the tree back as they had it.
    */
    void setExpanded (std::string_view sectionIdentity, bool shouldBeExpanded);
    [[nodiscard]] bool isExpanded (std::string_view sectionIdentity) const;

    /** Puts an item in the favourites section, or takes it out again. Both
        outlive the project and the app.
    */
    void toggleFavourite (std::string_view itemIdentity);
    [[nodiscard]] bool isFavourite (std::string_view itemIdentity) const;

    //==============================================================================
    /** Whether dropping this item on this track would do anything. An
        instrument needs a MIDI track, a sample needs an audio track, and an
        effect goes on any of them and on the Master.
    */
    [[nodiscard]] bool canDropOnTrack (const BrowserItem& item, duet::model::TrackRef track) const;

    /** Drops a sample on a track at a place on the timeline, as one Action.

        The file is imported into the project first, so the clip refers to the
        project's own copy, and the clip starts on the grid unless Alt is held.
        noClip when the drop had nowhere to land, and then nothing was done.
    */
    duet::model::ClipRef dropSample (const BrowserItem& item,
                                     duet::model::TrackRef track,
                                     double atBeats,
                                     GridSpec grid,
                                     bool altHeld);

    /** Drops an instrument or an effect on a track, as one Action: an
        instrument becomes what the track plays, in place of the one it had, and
        an effect goes on the end of the chain.
    */
    duet::model::PluginRef dropDevice (const BrowserItem& item, duet::model::TrackRef track);

    /** The same, at a position in a strip's insert chain — which is what
        dropping between two plugins means. An instrument ignores the position:
        what a MIDI track plays is at the head of its chain.
    */
    duet::model::PluginRef
        dropDeviceAt (const BrowserItem& item, duet::model::TrackRef track, int position);

    //==============================================================================
    /** The extensions a sample folder's readable content has. */
    [[nodiscard]] static const std::vector<std::string>& sampleExtensions();

private:
    [[nodiscard]] std::vector<duet::model::PluginInfo> chainOf (duet::model::TrackRef track) const;
    [[nodiscard]] std::vector<BrowserItem> pluginItems() const;
    [[nodiscard]] std::vector<std::string> favouriteIdentities() const;
    void writeFolders (const std::vector<std::filesystem::path>& folders);
    [[nodiscard]] std::vector<BrowserItem> matching (const std::vector<BrowserItem>& items) const;
    void notifyChanged() const;
    [[nodiscard]] static bool expandedByDefault (BrowserSectionKind kind);
    void appendSection (std::vector<BrowserSection>& into,
                        BrowserSectionKind kind,
                        std::string name,
                        const std::string& identity,
                        std::filesystem::path folder,
                        const std::vector<BrowserItem>& items,
                        std::string status = {}) const;

    /** One sample folder as the last installed generation left it. */
    struct ScannedFolder
    {
        std::filesystem::path folder;
        std::vector<BrowserItem> items;
        std::string status;
    };

    Settings* settings = nullptr;
    duet::model::Session* session = nullptr;
    SourceAudition* sourceAuditionPlayer = nullptr;
    std::string selectedIdentity;
    std::function<std::filesystem::path (const std::filesystem::path&)> importer;
    std::function<void (const SampleFolderScanRequest&)> scanWorker;
    std::function<void()> changed;
    std::vector<ScannedFolder> scanned;
    std::vector<duet::model::KnownPluginInfo> plugins;
    std::map<std::string, bool, std::less<>> expansion;
    std::string searchText;
    std::uint64_t scanGeneration = 0;
    bool scanBusy = false;
    std::size_t scanCompleted = 0;
    std::size_t scanKnown = 0;
};
} // namespace duet::gui
