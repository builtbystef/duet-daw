#include <duet/gui/Browser.h>

#include <duet/gui/Settings.h>
#include <duet/gui/Snap.h>

#include <duet/model/AudioFile.h>

#include <algorithm>
#include <cctype>
#include <functional>
#include <system_error>
#include <vector>

namespace duet::gui
{
namespace
{
    constexpr std::string_view sampleFoldersKey = "browser.sampleFolders";
    constexpr std::string_view favouritesKey = "browser.favourites";

    constexpr std::string_view favouritesSection = "section:favourites";
    constexpr std::string_view instrumentsSection = "section:instruments";
    constexpr std::string_view effectsSection = "section:effects";
    constexpr std::string_view pluginsSection = "section:plugins";
    constexpr std::string_view folderSectionPrefix = "folder:";

    /** A list of strings in one settings value.

        The entries are paths and identities made of paths, and a file name may
        hold anything a file name may hold — a newline included — so the
        separator is escaped rather than assumed away.
    */
    [[nodiscard]] std::string encodeList (const std::vector<std::string>& entries)
    {
        std::string encoded;

        for (const auto& entry : entries)
        {
            for (const auto character : entry)
            {
                if (character == '\\' || character == '\n')
                    encoded.push_back ('\\');
                encoded.push_back (character == '\n' ? 'n' : character);
            }
            encoded.push_back ('\n');
        }

        return encoded;
    }

    [[nodiscard]] std::vector<std::string> decodeList (std::string_view encoded)
    {
        std::vector<std::string> entries;
        std::string entry;
        auto escaped = false;

        for (const auto character : encoded)
        {
            if (escaped)
            {
                entry.push_back (character == 'n' ? '\n' : character);
                escaped = false;
            }
            else if (character == '\\')
                escaped = true;
            else if (character == '\n')
            {
                entries.push_back (entry);
                entry.clear();
            }
            else
                entry.push_back (character);
        }

        return entries;
    }

    [[nodiscard]] std::string folded (std::string_view text)
    {
        std::string result { text };
        std::ranges::transform (result,
                                result.begin(),
                                [] (unsigned char character)
                                { return static_cast<char> (std::tolower (character)); });
        return result;
    }

    [[nodiscard]] std::string nameOf (duet::model::BuiltinPlugin plugin)
    {
        switch (plugin)
        {
            case duet::model::BuiltinPlugin::eq:
                return "EQ";
            case duet::model::BuiltinPlugin::compressor:
                return "Compressor";
            case duet::model::BuiltinPlugin::reverb:
                return "Reverb";
            case duet::model::BuiltinPlugin::synth:
                return "4OSC";
            case duet::model::BuiltinPlugin::sampler:
                return "Sampler";
        }

        return {};
    }

    [[nodiscard]] std::string identityOf (duet::model::BuiltinPlugin plugin)
    {
        return "builtin:" + folded (nameOf (plugin));
    }

    /** The built-ins of one kind, in the order the dock shows them. */
    [[nodiscard]] std::vector<BrowserItem> builtinItems (BrowserItemKind kind)
    {
        const auto wanted = kind == BrowserItemKind::instrument
                                ? std::vector { duet::model::BuiltinPlugin::synth,
                                                duet::model::BuiltinPlugin::sampler }
                                : std::vector { duet::model::BuiltinPlugin::eq,
                                                duet::model::BuiltinPlugin::compressor,
                                                duet::model::BuiltinPlugin::reverb };
        std::vector<BrowserItem> items;
        items.reserve (wanted.size());

        for (const auto builtin : wanted)
            items.push_back (
                { kind, nameOf (builtin), identityOf (builtin), builtin, {}, {}, false });

        return items;
    }

    /** Whether a chain entry is what the track plays rather than something in
        the way of it. */
    [[nodiscard]] bool isInstrument (const duet::model::PluginInfo& plugin,
                                     const std::vector<duet::model::KnownPluginInfo>& known)
    {
        if (plugin.builtin.has_value())
            return *plugin.builtin == duet::model::BuiltinPlugin::synth
                   || *plugin.builtin == duet::model::BuiltinPlugin::sampler;

        const auto found = std::ranges::find (
            known, plugin.externalIdentifier, &duet::model::KnownPluginInfo::identifier);

        return found != known.end() && found->isInstrument;
    }

    [[nodiscard]] bool stopRequested (const std::function<bool()>& stop)
    {
        return static_cast<bool> (stop) && stop();
    }

    /** The relative path a status names, or "this folder" when the walk could
        not even place the directory under the root.
    */
    [[nodiscard]] std::string statusLabel (const std::filesystem::path& directory,
                                           const std::filesystem::path& root)
    {
        std::error_code notRelative;
        const auto relative = std::filesystem::relative (directory, root, notRelative);

        if (notRelative || relative.empty() || relative == ".")
            return "this folder";

        return relative.generic_string();
    }

    void recordUnreadable (SampleFolderScan& result,
                           const std::filesystem::path& directory,
                           const std::filesystem::path& root)
    {
        if (result.status.empty())
            result.status = "Could not read " + statusLabel (directory, root);
    }

    [[nodiscard]] std::string sampleName (const std::filesystem::path& file,
                                          const std::filesystem::path& root)
    {
        std::error_code notRelative;
        const auto relative = std::filesystem::relative (file, root, notRelative);

        if (notRelative)
            return file.filename().string();

        return relative.generic_string();
    }

    void appendSample (SampleFolderScan& result,
                       const std::filesystem::directory_entry& entry,
                       const std::filesystem::path& root)
    {
        std::error_code error;

        if (! entry.is_regular_file (error) || error)
            return;

        const auto extension = folded (entry.path().extension().string());
        const auto& known = Browser::sampleExtensions();

        if (std::ranges::find (known, extension) == known.end())
            return;

        result.items.push_back ({ BrowserItemKind::sample,
                                  sampleName (entry.path(), root),
                                  "sample:" + entry.path().string(),
                                  {},
                                  {},
                                  entry.path(),
                                  false });
    }

    void takeEntry (std::vector<std::filesystem::path>& pending,
                    SampleFolderScan& result,
                    const std::filesystem::directory_entry& entry,
                    const std::filesystem::path& root)
    {
        std::error_code error;
        const auto symlink = entry.is_symlink (error);

        if (error)
            return;

        // Directory symlinks are not followed, which is also what breaks a
        // cycle. A symlink to a regular sample is itself a listed file.
        if (entry.is_directory (error) && ! error)
        {
            if (! symlink)
                pending.push_back (entry.path());

            return;
        }

        appendSample (result, entry, root);
    }

    void listDirectory (const std::filesystem::path& directory,
                        const std::filesystem::path& root,
                        std::vector<std::filesystem::path>& pending,
                        SampleFolderScan& result,
                        const std::function<bool()>& stop)
    {
        std::error_code error;
        std::filesystem::directory_iterator iterator { directory,
                                                       std::filesystem::directory_options::none,
                                                       error };

        if (error)
        {
            recordUnreadable (result, directory, root);
            return;
        }

        const std::filesystem::directory_iterator end {};

        for (; iterator != end; iterator.increment (error))
        {
            if (error)
            {
                recordUnreadable (result, directory, root);
                return;
            }

            if (stopRequested (stop))
                return;

            takeEntry (pending, result, *iterator, root);
        }
    }

    void walkSampleFolder (const std::filesystem::path& root,
                           SampleFolderScan& result,
                           const std::function<bool()>& stop)
    {
        std::vector<std::filesystem::path> pending { root };

        while (! pending.empty())
        {
            if (stopRequested (stop))
                return;

            const auto directory = std::move (pending.back());
            pending.pop_back();
            listDirectory (directory, root, pending, result, stop);
        }
    }

    [[nodiscard]] bool itemNameLess (const BrowserItem& first, const BrowserItem& second)
    {
        const auto foldedFirst = folded (first.name);
        const auto foldedSecond = folded (second.name);

        if (foldedFirst != foldedSecond)
            return foldedFirst < foldedSecond;

        return first.name < second.name;
    }
} // namespace

SampleFolderScan scanSampleFolder (const std::filesystem::path& folder,
                                   const std::function<bool()>& cancelled)
{
    SampleFolderScan result;
    result.folder = folder;
    walkSampleFolder (folder, result, cancelled);
    std::ranges::sort (result.items, itemNameLess);
    return result;
}

//==============================================================================
Browser::Browser (Settings& store) : settings (&store)
{
    // Paths only: walking the trees is a refresh, and a host that supplies a
    // worker does that after attach rather than on construction.
    for (const auto& folder : sampleFolders())
        scanned.push_back ({ folder, {}, {} });
}

void Browser::setSession (duet::model::Session* openProject)
{
    session = openProject;
    refresh();
}

void Browser::onChanged (std::function<void()> callback) { changed = std::move (callback); }

void Browser::notifyChanged() const
{
    if (changed)
        changed();
}

void Browser::setSampleImporter (
    std::function<std::filesystem::path (const std::filesystem::path&)> importIntoProject)
{
    importer = std::move (importIntoProject);
}

void Browser::setScanWorker (std::function<void (const SampleFolderScanRequest&)> worker)
{
    scanWorker = std::move (worker);
}

void Browser::applyScanProgress (const SampleFolderScanProgress& progress)
{
    if (progress.generation != scanGeneration || ! scanBusy)
        return;

    scanCompleted = progress.completed;
    scanKnown = progress.known;
    notifyChanged();
}

void Browser::applyScanOutcome (const SampleFolderScanOutcome& outcome)
{
    if (outcome.generation != scanGeneration)
        return;

    scanned.clear();
    scanned.reserve (outcome.folders.size());

    for (const auto& folder : outcome.folders)
        scanned.push_back ({ folder.folder, folder.items, folder.status });

    scanBusy = false;
    scanCompleted = outcome.folders.size();
    scanKnown = outcome.folders.size();
    notifyChanged();
}

BrowserScanSnapshot Browser::scanSnapshot() const
{
    BrowserScanSnapshot snapshot;
    snapshot.busy = scanBusy;
    snapshot.completed = scanCompleted;
    snapshot.known = scanKnown;

    if (scanBusy)
        snapshot.message =
            "Scanning… " + std::to_string (scanCompleted) + "/" + std::to_string (scanKnown);

    return snapshot;
}

const std::vector<std::string>& Browser::sampleExtensions()
{
    static const std::vector<std::string> extensions { ".wav",  ".aiff", ".aif",
                                                       ".flac", ".ogg",  ".mp3" };

    return extensions;
}

void Browser::refresh()
{
    plugins = session != nullptr ? session->knownVst3Plugins()
                                 : std::vector<duet::model::KnownPluginInfo> {};

    const auto folders = sampleFolders();
    std::vector<ScannedFolder> next;
    next.reserve (folders.size());

    for (const auto& folder : folders)
    {
        const auto found = std::ranges::find (scanned, folder, &ScannedFolder::folder);

        if (found != scanned.end())
            next.push_back (*found);
        else
            next.push_back ({ folder, {}, {} });
    }

    scanned = std::move (next);
    ++scanGeneration;

    if (scanWorker)
    {
        scanBusy = true;
        scanCompleted = 0;
        scanKnown = folders.size();
        notifyChanged();
        scanWorker ({ scanGeneration, folders });
        return;
    }

    SampleFolderScanOutcome outcome;
    outcome.generation = scanGeneration;
    outcome.folders.reserve (folders.size());

    for (const auto& folder : folders)
        outcome.folders.push_back (scanSampleFolder (folder));

    applyScanOutcome (outcome);
}

//==============================================================================
std::vector<std::filesystem::path> Browser::sampleFolders() const
{
    std::vector<std::filesystem::path> folders;

    for (auto& entry : decodeList (settings->value (sampleFoldersKey).value_or (std::string {})))
        if (! entry.empty())
            folders.emplace_back (entry);

    return folders;
}

void Browser::writeFolders (const std::vector<std::filesystem::path>& folders)
{
    std::vector<std::string> entries;
    entries.reserve (folders.size());

    for (const auto& folder : folders)
        entries.push_back (folder.string());

    settings->setValue (sampleFoldersKey, encodeList (entries));
}

void Browser::addSampleFolder (const std::filesystem::path& folder)
{
    if (folder.empty())
        return;

    auto folders = sampleFolders();

    if (std::ranges::find (folders, folder) != folders.end())
        return;

    folders.push_back (folder);
    writeFolders (folders);
    refresh();
}

void Browser::removeSampleFolder (const std::filesystem::path& folder)
{
    auto folders = sampleFolders();

    if (std::erase (folders, folder) == 0)
        return;

    writeFolders (folders);
    refresh();
}

//==============================================================================
void Browser::setSearch (std::string_view text)
{
    searchText = std::string { text };
    notifyChanged();
}

std::vector<std::string> Browser::favouriteIdentities() const
{
    auto entries = decodeList (settings->value (favouritesKey).value_or (std::string {}));
    std::erase_if (entries, [] (const auto& entry) { return entry.empty(); });

    return entries;
}

bool Browser::isFavourite (std::string_view itemIdentity) const
{
    const auto entries = favouriteIdentities();

    return std::ranges::find (entries, itemIdentity) != entries.end();
}

void Browser::toggleFavourite (std::string_view itemIdentity)
{
    if (itemIdentity.empty())
        return;

    auto entries = favouriteIdentities();
    const auto found = std::ranges::find (entries, itemIdentity);

    if (found != entries.end())
        entries.erase (found);
    else
        entries.emplace_back (itemIdentity);

    settings->setValue (favouritesKey, encodeList (entries));
    notifyChanged();
}

//==============================================================================
bool Browser::expandedByDefault (BrowserSectionKind kind)
{
    // A sample folder is the one section that can hold thousands of things, so
    // it is the one the producer opens rather than closes.
    return kind != BrowserSectionKind::samples;
}

void Browser::setExpanded (std::string_view sectionIdentity, bool shouldBeExpanded)
{
    expansion[std::string { sectionIdentity }] = shouldBeExpanded;
    notifyChanged();
}

bool Browser::isExpanded (std::string_view sectionIdentity) const
{
    const auto found = expansion.find (sectionIdentity);

    if (found != expansion.end())
        return found->second;

    return expandedByDefault (sectionIdentity.starts_with (folderSectionPrefix)
                                  ? BrowserSectionKind::samples
                                  : BrowserSectionKind::instruments);
}

//==============================================================================
std::vector<BrowserItem> Browser::pluginItems() const
{
    std::vector<BrowserItem> items;

    for (const auto& plugin : plugins)
    {
        if (! plugin.isAvailable)
            continue;

        items.push_back (
            { plugin.isInstrument ? BrowserItemKind::instrument : BrowserItemKind::effect,
              plugin.name,
              "vst3:" + plugin.identifier,
              {},
              plugin.identifier,
              {},
              false });
    }

    std::ranges::sort (items,
                       [] (const auto& first, const auto& second)
                       { return folded (first.name) < folded (second.name); });

    return items;
}

std::vector<BrowserItem> Browser::matching (const std::vector<BrowserItem>& items) const
{
    if (searchText.empty())
        return items;

    const auto wanted = folded (searchText);
    auto kept = items;
    std::erase_if (kept,
                   [&wanted] (const auto& item)
                   { return folded (item.name).find (wanted) == std::string::npos; });

    return kept;
}

void Browser::appendSection (std::vector<BrowserSection>& into,
                             BrowserSectionKind kind,
                             std::string name,
                             const std::string& identity,
                             std::filesystem::path folder,
                             const std::vector<BrowserItem>& items,
                             std::string status) const
{
    auto kept = matching (items);

    // A search is a question about the whole tree, so a section with no answer
    // is not shown at all — and what is shown is open, whatever the producer
    // had closed before they asked.
    if (! searchText.empty() && kept.empty())
        return;

    const auto favourites = favouriteIdentities();

    for (auto& item : kept)
        item.favourite = std::ranges::find (favourites, item.identity) != favourites.end();

    into.push_back ({ kind,
                      std::move (name),
                      identity,
                      std::move (folder),
                      searchText.empty() ? isExpanded (identity) : true,
                      std::move (kept),
                      std::move (status) });
}

std::vector<BrowserSection> Browser::sections() const
{
    std::vector<BrowserSection> result;
    std::vector<BrowserItem> everything;

    for (const auto& item : builtinItems (BrowserItemKind::instrument))
        everything.push_back (item);
    for (const auto& item : builtinItems (BrowserItemKind::effect))
        everything.push_back (item);
    for (const auto& item : pluginItems())
        everything.push_back (item);
    for (const auto& folder : scanned)
        for (const auto& item : folder.items)
            everything.push_back (item);

    // The favourites keep the order the producer favourited them in: it is the
    // only order they chose.
    std::vector<BrowserItem> favourites;

    for (const auto& identity : favouriteIdentities())
    {
        const auto found = std::ranges::find (everything, identity, &BrowserItem::identity);

        if (found != everything.end())
            favourites.push_back (*found);
    }

    appendSection (result,
                   BrowserSectionKind::favourites,
                   "Favourites",
                   std::string { favouritesSection },
                   {},
                   favourites);
    appendSection (result,
                   BrowserSectionKind::instruments,
                   "Instruments",
                   std::string { instrumentsSection },
                   {},
                   builtinItems (BrowserItemKind::instrument));
    appendSection (result,
                   BrowserSectionKind::effects,
                   "Effects",
                   std::string { effectsSection },
                   {},
                   builtinItems (BrowserItemKind::effect));
    appendSection (result,
                   BrowserSectionKind::plugins,
                   "VST3",
                   std::string { pluginsSection },
                   {},
                   pluginItems());

    for (const auto& folder : scanned)
        appendSection (result,
                       BrowserSectionKind::samples,
                       folder.folder.filename().empty() ? folder.folder.string()
                                                        : folder.folder.filename().string(),
                       std::string { folderSectionPrefix } + folder.folder.string(),
                       folder.folder,
                       folder.items,
                       folder.status);

    return result;
}

std::optional<BrowserItem> Browser::item (std::string_view identity) const
{
    for (const auto& section : sections())
    {
        const auto found = std::ranges::find (section.items, identity, &BrowserItem::identity);

        if (found != section.items.end())
            return *found;
    }

    return std::nullopt;
}

//==============================================================================
std::vector<duet::model::PluginInfo> Browser::chainOf (duet::model::TrackRef track) const
{
    if (session == nullptr)
        return {};

    return track == duet::model::masterChannel ? session->master().plugins
                                               : session->track (track).plugins;
}

bool Browser::canDropOnTrack (const BrowserItem& item, duet::model::TrackRef track) const
{
    if (session == nullptr || track == duet::model::noTrack)
        return false;

    // The Master is a strip and not a track: an effect belongs in its chain
    // like any other, and nothing else the dock holds has a meaning there.
    if (track == duet::model::masterChannel)
        return item.kind == BrowserItemKind::effect;

    const auto info = session->track (track);

    if (info.track == duet::model::noTrack)
        return false;

    switch (item.kind)
    {
        case BrowserItemKind::instrument:
            return info.kind == duet::model::TrackKind::midi;
        case BrowserItemKind::sample:
            return info.kind == duet::model::TrackKind::audio;
        case BrowserItemKind::effect:
            return true;
    }

    return false;
}

duet::model::ClipRef Browser::dropSample (const BrowserItem& item,
                                          duet::model::TrackRef track,
                                          double atBeats,
                                          GridSpec grid,
                                          bool altHeld)
{
    if (item.kind != BrowserItemKind::sample || ! canDropOnTrack (item, track))
        return duet::model::noClip;

    const auto samples = duet::model::readAudioFile (item.file);

    // A file nothing can read is a clip that would play silence, so the drop
    // ends here rather than in an Action.
    if (! samples.readable())
        return duet::model::noClip;

    const auto lengthSeconds =
        static_cast<double> (samples.channels.front().size()) / samples.sampleRate;
    const auto inTheProject = importer ? importer (item.file) : item.file;

    if (inTheProject.empty())
        return duet::model::noClip;

    const auto startBeats = std::max (0.0, snapBeats (atBeats, grid, altHeld));
    const auto startSeconds = session->secondsAtBeats (startBeats);
    duet::model::ClipRef clip = duet::model::noClip;

    session->performAction (
        "Insert Audio Clip",
        [&] (auto& ops)
        {
            clip = ops.insertAudioClip (
                track, item.file.stem().string(), inTheProject, startSeconds, lengthSeconds);
        });

    return clip;
}

duet::model::PluginRef Browser::dropDevice (const BrowserItem& item, duet::model::TrackRef track)
{
    if (! canDropOnTrack (item, track))
        return duet::model::noPlugin;

    return dropDeviceAt (item, track, static_cast<int> (chainOf (track).size()));
}

duet::model::PluginRef
    Browser::dropDeviceAt (const BrowserItem& item, duet::model::TrackRef track, int position)
{
    if (item.kind == BrowserItemKind::sample || ! canDropOnTrack (item, track))
        return duet::model::noPlugin;

    const auto instrument = item.kind == BrowserItemKind::instrument;
    const auto chain = chainOf (track);
    duet::model::PluginRef inserted = duet::model::noPlugin;

    session->performAction (
        instrument ? "Set Track Instrument" : "Insert Plugin",
        [&] (auto& ops)
        {
            // What a MIDI track plays is one thing at the head of its chain, so
            // an instrument dropped on a track holding one replaces it.
            if (instrument)
                for (const auto& existing : chain)
                    if (isInstrument (existing, plugins))
                        ops.removePlugin (existing.plugin);

            const auto at =
                instrument ? 0 : std::clamp (position, 0, static_cast<int> (chain.size()));

            inserted = item.builtin.has_value() ? ops.addPlugin (track, *item.builtin, at)
                                                : ops.addPlugin (track, item.pluginIdentifier, at);
        });

    return inserted;
}
} // namespace duet::gui
