#include "SessionImpl.h"

#include <utility>

/** Scanning the producer's machine for VST3 plugins.

    Out of process, always: the engine launches a child to ask a plugin what it
    is, so a plugin that crashes on being asked kills the child and lands in the
    scan's results as a bad file rather than taking the DAW down with it. The
    walk itself is stepped rather than run, because a scan is something the
    producer watches.
*/
namespace duet::model
{
namespace
{
    /** The engine's VST3 format, or nothing in a build that has none. */
    juce::AudioPluginFormat* vst3FormatOf (te::Engine& engine)
    {
        auto& formats = engine.getPluginManager().pluginFormatManager;

        for (int index = 0; index < formats.getNumFormats(); ++index)
            if (auto* format = formats.getFormat (index); format->getName() == "VST3")
                return format;

        return nullptr;
    }
} // namespace

//==============================================================================
struct Vst3Scan::Impl
{
    Impl (te::Engine& itsEngine,
          juce::AudioPluginFormat& format,
          const std::filesystem::path& folder)
        : engine (itsEngine), directory (folder.lexically_normal()),
          scanner (engine.getPluginManager().knownPluginList,
                   format,
                   juce::FileSearchPath { toJuceFile (folder).getFullPathName() },
                   true,
                   engine.getPropertyStorage().getAppPrefsFolder().getChildFile (
                       "PluginScanDeadMansPedal.txt"))
    {
    }

    /** Writes the completed list to disk.

        The plugin manager normally does this through an asynchronous change
        listener. A scan the producer is watching ends when they see it end, so
        the list goes on disk here: a restart immediately after a scan must not
        scan known-good plugins again.
    */
    void keepWhatWasFound() const
    {
        if (const auto xml = engine.getPluginManager().knownPluginList.createXml())
        {
#if JUCE_64BIT
            constexpr auto knownPluginsSetting = te::SettingID::knownPluginList64;
#else
            constexpr auto knownPluginsSetting = te::SettingID::knownPluginList;
#endif

            auto& storage = engine.getPropertyStorage();
            storage.setXmlProperty (knownPluginsSetting, *xml);
            storage.getPropertiesFile().saveIfNeeded();
        }
    }

    te::Engine& engine;
    std::filesystem::path directory;
    juce::PluginDirectoryScanner scanner;
    bool walked = false;
};

//==============================================================================
Vst3Scan::Vst3Scan (std::unique_ptr<Impl> made) : impl (std::move (made)) {}

Vst3Scan::~Vst3Scan() = default;

std::filesystem::path Vst3Scan::nextPlugin() const
{
    const auto next = impl->scanner.getNextPluginFileThatWillBeScanned();

    return next.isEmpty() ? std::filesystem::path {} : toPath (juce::File { next });
}

double Vst3Scan::progress() const
{
    return impl->walked ? 1.0 : static_cast<double> (impl->scanner.getProgress());
}

bool Vst3Scan::step()
{
    if (impl->walked)
        return false;

    juce::String scanning;

    if (impl->scanner.scanNextFile (true, scanning))
        return true;

    // The walk is over. What it found goes on disk here, once.
    impl->walked = true;
    impl->keepWhatWasFound();

    return false;
}

PluginScanResult Vst3Scan::result() const
{
    PluginScanResult found;
    found.completed = impl->walked;

    for (const auto& failed : impl->scanner.getFailedFiles())
        found.failedFiles.push_back (toPath (juce::File { failed }));

    // The blacklist is app-global, so only the files under this scan's own
    // directory are this scan's to report.
    for (const auto& bad : impl->engine.getPluginManager().knownPluginList.getBlacklistedFiles())
    {
        const auto path = toPath (juce::File { bad }).lexically_normal();
        const auto relative = path.lexically_relative (impl->directory);

        if (! relative.empty() && *relative.begin() != "..")
            found.badFiles.push_back (path);
    }

    return found;
}

//==============================================================================
bool Session::canHostVst3() const { return vst3FormatOf (impl->engine) != nullptr; }

bool Session::scansPluginsOutOfProcess() const
{
    return impl->engine.getPluginManager().usesSeparateProcessForScanning();
}

std::unique_ptr<Vst3Scan> Session::beginVst3Scan (const std::filesystem::path& directory)
{
    if (! std::filesystem::is_directory (directory))
        return nullptr;

    auto* format = vst3FormatOf (impl->engine);

    if (format == nullptr)
        return nullptr;

    return std::unique_ptr<Vst3Scan> { new Vst3Scan {
        std::make_unique<Vst3Scan::Impl> (impl->engine, *format, directory) } };
}

std::vector<std::filesystem::path> Session::vst3Directories() const
{
    std::vector<std::filesystem::path> directories;
    auto* format = vst3FormatOf (impl->engine);

    if (format == nullptr)
        return directories;

    const auto locations = format->getDefaultLocationsToSearch();

    for (int index = 0; index < locations.getNumPaths(); ++index)
        if (const auto folder = locations[index]; folder.isDirectory())
            directories.push_back (toPath (folder));

    return directories;
}

PluginScanResult Session::scanVst3Plugins (const std::filesystem::path& directory)
{
    const auto scan = beginVst3Scan (directory);

    if (scan == nullptr)
        return {};

    while (scan->step())
    {
    }

    return scan->result();
}

std::vector<KnownPluginInfo> Session::knownVst3Plugins() const
{
    std::vector<KnownPluginInfo> known;

    for (const auto& description : impl->engine.getPluginManager().knownPluginList.getTypes())
        if (description.pluginFormatName == "VST3")
            known.push_back ({ description.createIdentifierString().toStdString(),
                               description.name.toStdString(),
                               description.manufacturerName.toStdString(),
                               toPath (juce::File { description.fileOrIdentifier }),
                               description.isInstrument,
                               juce::File { description.fileOrIdentifier }.exists() });

    return known;
}
} // namespace duet::model
