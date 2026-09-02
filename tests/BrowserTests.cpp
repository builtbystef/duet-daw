#include <duet/gui/Browser.h>
#include <duet/model/Session.h>
#include <duet/persistence/Project.h>

#include <duet/testing/RenderHarness.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::gui::Browser;
using duet::gui::BrowserItem;
using duet::gui::BrowserItemKind;
using duet::gui::BrowserSection;
using duet::gui::BrowserSectionKind;
using duet::gui::GridSpec;
using duet::gui::SampleFolderScanRequest;
using duet::gui::scanSampleFolder;
using duet::gui::SourceAudition;
using duet::gui::SourceAuditionState;
using duet::model::BuiltinPlugin;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::StoredSettings;
using duet::testing::TempProject;

namespace
{
/** The section one kind names, and none when the browser is not showing it. */
[[nodiscard]] std::optional<BrowserSection> sectionOf (const Browser& browser,
                                                       BrowserSectionKind kind)
{
    const auto sections = browser.sections();
    const auto found = std::ranges::find (sections, kind, &BrowserSection::kind);

    return found == sections.end() ? std::nullopt : std::optional { *found };
}

/** The section one sample folder has, and none when the browser is not showing
    that folder.
*/
[[nodiscard]] std::optional<BrowserSection> folderSection (const Browser& browser,
                                                           const std::filesystem::path& folder)
{
    const auto sections = browser.sections();
    const auto found = std::ranges::find (sections, folder, &BrowserSection::folder);

    return found == sections.end() ? std::nullopt : std::optional { *found };
}

/** What a section holds, and nothing for a section the browser is not showing:
    every helper here answers for an absent section rather than reaching into
    one, so that a missing section fails the assertion that named it.
*/
[[nodiscard]] std::vector<BrowserItem> itemsOf (const std::optional<BrowserSection>& section)
{
    return section.has_value() ? section->items : std::vector<BrowserItem> {};
}

[[nodiscard]] std::vector<std::string> namesOf (const std::vector<BrowserItem>& items)
{
    std::vector<std::string> names;
    names.reserve (items.size());

    for (const auto& item : items)
        names.push_back (item.name);

    return names;
}

[[nodiscard]] std::vector<std::string> namesOf (const std::optional<BrowserSection>& section)
{
    return namesOf (itemsOf (section));
}

[[nodiscard]] std::string identityOf (const std::optional<BrowserSection>& section)
{
    return section.has_value() ? section->identity : std::string {};
}

[[nodiscard]] std::string statusOf (const std::optional<BrowserSection>& section)
{
    return section.has_value() ? section->status : std::string {};
}

[[nodiscard]] bool isExpanded (const std::optional<BrowserSection>& section)
{
    return section.has_value() && section->expanded;
}

[[nodiscard]] bool holds (const std::optional<BrowserSection>& section, const std::string& name)
{
    const auto names = namesOf (section);

    return std::ranges::find (names, name) != names.end();
}

/** The item a section shows under a name. */
[[nodiscard]] BrowserItem itemNamed (const std::optional<BrowserSection>& section,
                                     const std::string& name)
{
    const auto items = itemsOf (section);
    const auto found = std::ranges::find (items, name, &BrowserItem::name);

    REQUIRE (found != items.end());
    return *found;
}

/** A file in a sample folder of the producer's, outside any project. What the
    browser shows is what a folder holds, so a name and an extension are the
    whole of what a listed file has to be.
*/
void writeSample (const std::filesystem::path& folder, const std::string& fileName)
{
    std::filesystem::create_directories (folder);
    std::ofstream stream { folder / fileName, std::ios::binary };
    stream << "not really audio";
}

/** A browser over a real project, with the import the host gives it. */
struct OpenBrowser
{
    explicit OpenBrowser (const std::filesystem::path& folder)
        : project (duet::persistence::Project::create (folder))
    {
        REQUIRE (project != nullptr);
        browser.setSession (&project->session());
        browser.setSampleImporter ([this] (const std::filesystem::path& file)
                                   { return project->importAudioFile (file); });
    }

    [[nodiscard]] Session& session() const { return project->session(); }

    StoredSettings store;
    std::unique_ptr<duet::persistence::Project> project;
    Browser browser { store };
};
} // namespace

TEST_CASE ("the browser lists the devices Duet ships and each chosen sample folder")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    const auto loops = temp.folder() / "loops";
    const auto empty = temp.folder() / "empty";
    writeSample (loops, "reverse crash.wav");
    std::filesystem::create_directories (empty);

    browser.addSampleFolder (loops);
    browser.addSampleFolder (empty);

    REQUIRE (namesOf (sectionOf (browser, BrowserSectionKind::instruments))
             == std::vector<std::string> { "4OSC", "Sampler" });
    REQUIRE (namesOf (sectionOf (browser, BrowserSectionKind::effects))
             == std::vector<std::string> { "EQ", "Compressor", "Reverb" });
    REQUIRE (sectionOf (browser, BrowserSectionKind::plugins).has_value());
    REQUIRE (sectionOf (browser, BrowserSectionKind::favourites).has_value());
    REQUIRE (namesOf (folderSection (browser, loops))
             == std::vector<std::string> { "reverse crash.wav" });

    // A folder with nothing readable under it is a section with nothing in it,
    // which is what tells the producer the folder is theirs and empty.
    const auto nothing = folderSection (browser, empty);
    REQUIRE (nothing.has_value());
    REQUIRE (namesOf (nothing).empty());
}

TEST_CASE ("a search filters every section and hides the ones that match nothing")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    const auto loops = temp.folder() / "loops";
    writeSample (loops, "reverse crash.wav");
    writeSample (loops, "kick.wav");
    browser.addSampleFolder (loops);

    browser.setExpanded (identityOf (folderSection (browser, loops)), true);
    browser.setExpanded (identityOf (sectionOf (browser, BrowserSectionKind::effects)), false);

    browser.setSearch ("rev");

    REQUIRE (namesOf (sectionOf (browser, BrowserSectionKind::effects))
             == std::vector<std::string> { "Reverb" });
    REQUIRE (namesOf (folderSection (browser, loops))
             == std::vector<std::string> { "reverse crash.wav" });

    // Nothing among the instruments is called anything like "rev", so that
    // section is not there at all rather than there and empty.
    REQUIRE_FALSE (sectionOf (browser, BrowserSectionKind::instruments).has_value());

    browser.setSearch ("");

    REQUIRE (sectionOf (browser, BrowserSectionKind::instruments).has_value());
    REQUIRE (isExpanded (folderSection (browser, loops)));
    REQUIRE_FALSE (isExpanded (sectionOf (browser, BrowserSectionKind::effects)));
    REQUIRE (namesOf (folderSection (browser, loops)).size() == 2);
}

TEST_CASE ("a favourite outlives the app and the project it was made in")
{
    const TempProject temp;
    StoredSettings store;
    const auto loops = temp.folder() / "loops";
    writeSample (loops, "kick.wav");
    std::string sample;

    {
        Browser browser { store };
        browser.addSampleFolder (loops);
        const auto reverb = itemNamed (sectionOf (browser, BrowserSectionKind::effects), "Reverb");
        sample = itemNamed (folderSection (browser, loops), "kick.wav").identity;

        browser.toggleFavourite (reverb.identity);
        browser.toggleFavourite (sample);

        REQUIRE (namesOf (sectionOf (browser, BrowserSectionKind::favourites))
                 == std::vector<std::string> { "Reverb", "kick.wav" });
        REQUIRE (itemNamed (sectionOf (browser, BrowserSectionKind::effects), "Reverb").favourite);
    }

    // The same store read again is the next launch, in whatever project the
    // producer opens next.
    Browser relaunched { store };
    relaunched.refresh();
    REQUIRE (relaunched.sampleFolders() == std::vector<std::filesystem::path> { loops });
    REQUIRE (namesOf (sectionOf (relaunched, BrowserSectionKind::favourites))
             == std::vector<std::string> { "Reverb", "kick.wav" });

    relaunched.toggleFavourite (sample);
    REQUIRE (namesOf (sectionOf (relaunched, BrowserSectionKind::favourites))
             == std::vector<std::string> { "Reverb" });
}

TEST_CASE ("adding and removing a sample folder shows at once and at the next launch")
{
    const TempProject temp;
    StoredSettings store;
    const auto loops = temp.folder() / "loops";
    const auto stabs = temp.folder() / "stabs";
    writeSample (loops, "kick.wav");
    writeSample (stabs, "stab.wav");
    Browser browser { store };

    browser.addSampleFolder (loops);
    REQUIRE (folderSection (browser, loops).has_value());

    browser.addSampleFolder (stabs);
    browser.addSampleFolder (stabs);
    REQUIRE (browser.sampleFolders() == std::vector<std::filesystem::path> { loops, stabs });

    browser.removeSampleFolder (loops);
    REQUIRE_FALSE (folderSection (browser, loops).has_value());
    REQUIRE (folderSection (browser, stabs).has_value());

    const Browser relaunched { store };
    REQUIRE (relaunched.sampleFolders() == std::vector<std::filesystem::path> { stabs });
}

TEST_CASE ("a sample dropped on a track lands on the grid, as one Action, inside the project")
{
    const TempProject temp;
    OpenBrowser open { temp.folder() / "project" };
    const auto loops = temp.folder() / "loops";
    std::filesystem::create_directories (loops);
    std::filesystem::copy_file (temp.writeTone ("loop.wav", 2.0, 440.0), loops / "loop.wav");
    open.browser.addSampleFolder (loops);

    TrackRef track = duet::model::noTrack;
    open.session().performAction (
        "Track", [&] (auto& ops) { track = ops.createTrack (TrackKind::audio, "Audio"); });
    const auto actionsBefore = open.session().undoNames().size();

    const auto item = itemNamed (folderSection (open.browser, loops), "loop.wav");
    const auto clip = open.browser.dropSample (item, track, 3.4, GridSpec { 1.0, 4.0 }, false);

    REQUIRE (clip != duet::model::noClip);
    REQUIRE (open.session().undoNames().size() == actionsBefore + 1);

    const auto clips = open.session().track (track).clips;
    REQUIRE (clips.size() == 1);
    REQUIRE_THAT (open.session().beatsAtSeconds (clips.front().startSeconds),
                  WithinAbs (3.0, 0.001));
    INFO ("stored source reference: " << clips.front().sourceReference);
    REQUIRE (clips.front().sourceReference == "audio/loop.wav");
    REQUIRE_THAT (clips.front().lengthSeconds, WithinAbs (2.0, 0.05));

    REQUIRE (open.session().undo());
    REQUIRE (open.session().track (track).clips.empty());
}

TEST_CASE ("a sample dropped with Alt held lands where it was dropped")
{
    const TempProject temp;
    OpenBrowser open { temp.folder() / "project" };
    const auto loops = temp.folder() / "loops";
    std::filesystem::create_directories (loops);
    std::filesystem::copy_file (temp.writeTone ("loop.wav", 1.0, 440.0), loops / "loop.wav");
    open.browser.addSampleFolder (loops);

    TrackRef track = duet::model::noTrack;
    open.session().performAction (
        "Track", [&] (auto& ops) { track = ops.createTrack (TrackKind::audio, "Audio"); });

    const auto item = itemNamed (folderSection (open.browser, loops), "loop.wav");
    REQUIRE (open.browser.dropSample (item, track, 3.4, GridSpec { 1.0, 4.0 }, true)
             != duet::model::noClip);

    const auto clips = open.session().track (track).clips;
    REQUIRE (clips.size() == 1);
    REQUIRE_THAT (open.session().beatsAtSeconds (clips.front().startSeconds),
                  WithinAbs (3.4, 0.001));
}

TEST_CASE ("an instrument dropped on a MIDI track becomes what it plays, in one Action")
{
    const TempProject temp;
    OpenBrowser open { temp.folder() / "project" };
    TrackRef midi = duet::model::noTrack;
    TrackRef audio = duet::model::noTrack;
    open.session().performAction ("Tracks",
                                  [&] (auto& ops)
                                  {
                                      midi = ops.createTrack (TrackKind::midi, "Keys");
                                      audio = ops.createTrack (TrackKind::audio, "Audio");
                                  });

    const auto synth =
        itemNamed (sectionOf (open.browser, BrowserSectionKind::instruments), "4OSC");
    const auto actionsBefore = open.session().undoNames().size();

    REQUIRE (open.browser.canDropOnTrack (synth, midi));
    REQUIRE (open.browser.dropDevice (synth, midi) != duet::model::noPlugin);
    REQUIRE (open.session().undoNames().size() == actionsBefore + 1);

    auto plugins = open.session().track (midi).plugins;
    REQUIRE (plugins.size() == 1);
    REQUIRE (plugins.front().builtin == BuiltinPlugin::synth);

    // A second instrument is what the track plays now, and not a second
    // instrument in the chain.
    const auto sampler =
        itemNamed (sectionOf (open.browser, BrowserSectionKind::instruments), "Sampler");
    REQUIRE (open.browser.dropDevice (sampler, midi) != duet::model::noPlugin);
    plugins = open.session().track (midi).plugins;
    REQUIRE (plugins.size() == 1);
    REQUIRE (plugins.front().builtin == BuiltinPlugin::sampler);

    // An audio track has nothing to drive an instrument, so the drop is
    // cancelled and the project is left as it was.
    const auto actionsBeforeInvalid = open.session().undoNames().size();
    REQUIRE_FALSE (open.browser.canDropOnTrack (synth, audio));
    REQUIRE (open.browser.dropDevice (synth, audio) == duet::model::noPlugin);
    REQUIRE (open.browser.dropDevice (synth, duet::model::noTrack) == duet::model::noPlugin);
    REQUIRE (open.session().undoNames().size() == actionsBeforeInvalid);
    REQUIRE (open.session().track (audio).plugins.empty());
}

TEST_CASE ("an effect dropped between two plugins of a chain goes in at that position")
{
    const TempProject temp;
    OpenBrowser open { temp.folder() / "project" };
    TrackRef track = duet::model::noTrack;
    open.session().performAction ("Chain",
                                  [&] (auto& ops)
                                  {
                                      track = ops.createTrack (TrackKind::audio, "Audio");
                                      ops.addPlugin (track, BuiltinPlugin::eq, 0);
                                      ops.addPlugin (track, BuiltinPlugin::compressor, 1);
                                  });

    const auto reverb = itemNamed (sectionOf (open.browser, BrowserSectionKind::effects), "Reverb");
    const auto actionsBefore = open.session().undoNames().size();

    REQUIRE (open.browser.dropDeviceAt (reverb, track, 1) != duet::model::noPlugin);
    REQUIRE (open.session().undoNames().size() == actionsBefore + 1);

    const auto plugins = open.session().track (track).plugins;
    REQUIRE (plugins.size() == 3);
    REQUIRE (plugins[0].builtin == BuiltinPlugin::eq);
    REQUIRE (plugins[1].builtin == BuiltinPlugin::reverb);
    REQUIRE (plugins[2].builtin == BuiltinPlugin::compressor);

    // Dropped on the track rather than into its chain, an effect goes last.
    const auto eq = itemNamed (sectionOf (open.browser, BrowserSectionKind::effects), "EQ");
    REQUIRE (open.browser.dropDevice (eq, track) != duet::model::noPlugin);
    REQUIRE (open.session().track (track).plugins.back().builtin == BuiltinPlugin::eq);

    // The Master is a strip with a chain like any other, and an instrument is
    // the one thing that has no meaning on it.
    REQUIRE (open.browser.dropDevice (reverb, duet::model::masterChannel) != duet::model::noPlugin);
    REQUIRE (open.session().master().plugins.back().builtin == BuiltinPlugin::reverb);
    REQUIRE (open.browser.dropDevice (
                 itemNamed (sectionOf (open.browser, BrowserSectionKind::instruments), "4OSC"),
                 duet::model::masterChannel)
             == duet::model::noPlugin);
}

TEST_CASE ("a dropped instrument plays what its track is sent, and a dropped effect is heard")
{
    const TempProject temp;
    Session session { temp.editFile() };
    session.useNoAudioDevice();
    session.suppressDeviceRebuild();
    StoredSettings store;
    Browser browser { store };
    browser.setSession (&session);

    // Two beats at the project's own 120 bpm is one second in, and the clip
    // runs a second past the note, so that what a reverb adds has somewhere to
    // be heard.
    TrackRef track = duet::model::noTrack;
    session.performAction ("Track",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::midi, "Keys");
                               const auto clip = ops.insertMidiClip (track, "notes", 0.0, 3.0);
                               ops.addNote (clip, 69, 2.0, 2.0, 100);
                           });

    const auto synth = itemNamed (sectionOf (browser, BrowserSectionKind::instruments), "4OSC");
    REQUIRE (browser.dropDevice (synth, track) != duet::model::noPlugin);

    const auto dry = duet::testing::renderProject (session, temp.folder());
    REQUIRE (dry.readable());
    REQUIRE_THAT (dry.pitchHzBetween (1.2, 1.9), WithinAbs (440.0, 2.0));

    // The tail is what says the effect is in the chain: sound after the note
    // has stopped, where the instrument on its own left silence.
    REQUIRE (dry.isSilentBetween (2.4, 2.9));

    const auto reverb = itemNamed (sectionOf (browser, BrowserSectionKind::effects), "Reverb");
    REQUIRE (browser.dropDevice (reverb, track) != duet::model::noPlugin);

    const auto wet = duet::testing::renderProject (session, temp.folder());
    REQUIRE (wet.readable());
    REQUIRE_FALSE (wet.isSilentBetween (2.4, 2.9));
}

TEST_CASE ("the browser shows what the scan found, reflects a rescan, and inserts a VST3")
{
    const TempProject temp;
    OpenBrowser open { temp.folder() / "project" };
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);

    REQUIRE_FALSE (
        holds (sectionOf (open.browser, BrowserSectionKind::plugins), "Duet Good VST3 Fixture"));

    for (const auto* fixture : { DUET_GOOD_VST3_FIXTURE, DUET_CRASHING_VST3_FIXTURE })
        std::filesystem::copy (fixture,
                               pluginDirectory / std::filesystem::path { fixture }.filename(),
                               std::filesystem::copy_options::recursive
                                   | std::filesystem::copy_options::overwrite_existing);

    REQUIRE (open.session().scanVst3Plugins (pluginDirectory).completed);

    // The scan happened under the browser, and a refresh is all it takes: no
    // restart, and what the scan rejected never appears.
    open.browser.refresh();

    REQUIRE (
        holds (sectionOf (open.browser, BrowserSectionKind::plugins), "Duet Good VST3 Fixture"));
    REQUIRE_FALSE (holds (sectionOf (open.browser, BrowserSectionKind::plugins),
                          "Duet Crashing VST3 Fixture"));

    TrackRef track = duet::model::noTrack;
    open.session().performAction (
        "Track", [&] (auto& ops) { track = ops.createTrack (TrackKind::audio, "Audio"); });
    const auto fixture =
        itemNamed (sectionOf (open.browser, BrowserSectionKind::plugins), "Duet Good VST3 Fixture");
    REQUIRE (fixture.kind == BrowserItemKind::effect);
    const auto actionsBefore = open.session().undoNames().size();

    REQUIRE (open.browser.dropDevice (fixture, track) != duet::model::noPlugin);
    REQUIRE (open.session().undoNames().size() == actionsBefore + 1);

    const auto plugins = open.session().track (track).plugins;
    REQUIRE (plugins.size() == 1);
    REQUIRE (plugins.front().externalIdentifier == fixture.pluginIdentifier);
    REQUIRE_FALSE (plugins.front().missing);
}

TEST_CASE ("a deep sample folder lists a deterministic tree and ignores directory symlink cycles")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    const auto root = temp.folder() / "library";

    writeSample (root / "a" / "nested", "snare.aiff");
    writeSample (root / "a", "Z.wav");
    writeSample (root / "a", "z.wav");
    writeSample (root / "B", "a.wav");

    {
        std::ofstream stream { root / "notes.txt" };
        stream << "not audio";
    }

    std::filesystem::create_directory_symlink (root / "a", root / "link-to-a");
    std::filesystem::create_directory_symlink (root, root / "cycle");

    browser.addSampleFolder (root);

    REQUIRE (namesOf (folderSection (browser, root))
             == std::vector<std::string> {
                 "a/nested/snare.aiff",
                 "a/Z.wav",
                 "a/z.wav",
                 "B/a.wav",
             });
}

TEST_CASE ("an unreadable subtree is one local status and does not drop readable siblings")
{
    const TempProject temp;
    const auto root = temp.folder() / "library";
    writeSample (root, "ok.wav");
    writeSample (root / "other", "yes.wav");
    const auto hidden = root / "hidden";
    writeSample (hidden, "secret.wav");

    std::filesystem::permissions (hidden, std::filesystem::perms::none);

    struct Restore
    {
        explicit Restore (std::filesystem::path path) : path (std::move (path)) {}
        Restore (const Restore&) = delete;
        Restore& operator= (const Restore&) = delete;
        Restore (Restore&&) = delete;
        Restore& operator= (Restore&&) = delete;
        ~Restore()
        {
            std::error_code ignored;
            std::filesystem::permissions (path, std::filesystem::perms::owner_all, ignored);
        }

        std::filesystem::path path;
    };
    const Restore restore { hidden };

    const auto scan = scanSampleFolder (root);

    REQUIRE (namesOf (scan.items) == std::vector<std::string> { "ok.wav", "other/yes.wav" });
    REQUIRE (scan.status == "Could not read hidden");

    StoredSettings store;
    Browser browser { store };
    browser.addSampleFolder (root);
    const auto section = folderSection (browser, root);
    REQUIRE (statusOf (section) == "Could not read hidden");
    REQUIRE (namesOf (section) == std::vector<std::string> { "ok.wav", "other/yes.wav" });
}

TEST_CASE ("existing sample rows remain while a refresh is in flight")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    const auto loops = temp.folder() / "loops";
    writeSample (loops, "kick.wav");

    std::vector<SampleFolderScanRequest> pending;
    browser.setScanWorker ([&] (const SampleFolderScanRequest& request)
                           { pending.push_back (request); });

    browser.addSampleFolder (loops);

    REQUIRE (browser.scanSnapshot().busy);
    REQUIRE (browser.scanSnapshot().completed == 0);
    REQUIRE (browser.scanSnapshot().known == 1);
    REQUIRE (browser.scanSnapshot().message == "Scanning… 0/1");
    REQUIRE (folderSection (browser, loops).has_value());
    REQUIRE (namesOf (folderSection (browser, loops)).empty());

    const auto first = pending.back();
    browser.applyScanOutcome ({ first.generation, { scanSampleFolder (loops) } });

    REQUIRE_FALSE (browser.scanSnapshot().busy);
    REQUIRE (browser.scanSnapshot().message.empty());
    REQUIRE (namesOf (folderSection (browser, loops)) == std::vector<std::string> { "kick.wav" });

    writeSample (loops, "snare.wav");
    browser.refresh();

    REQUIRE (browser.scanSnapshot().busy);
    REQUIRE (browser.scanSnapshot().message == "Scanning… 0/1");
    REQUIRE (namesOf (folderSection (browser, loops)) == std::vector<std::string> { "kick.wav" });

    const auto second = pending.back();
    browser.applyScanOutcome ({ second.generation, { scanSampleFolder (loops) } });

    REQUIRE_FALSE (browser.scanSnapshot().busy);
    REQUIRE (namesOf (folderSection (browser, loops))
             == std::vector<std::string> { "kick.wav", "snare.wav" });
}

TEST_CASE ("a stale scan generation does not replace a newer one")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    const auto loops = temp.folder() / "loops";
    writeSample (loops, "old.wav");

    std::vector<SampleFolderScanRequest> pending;
    browser.setScanWorker ([&] (const SampleFolderScanRequest& request)
                           { pending.push_back (request); });

    browser.addSampleFolder (loops);
    const auto first = pending.back();
    const auto oldResult = scanSampleFolder (loops);

    writeSample (loops, "new.wav");
    browser.refresh();
    const auto second = pending.back();
    const auto newResult = scanSampleFolder (loops);

    browser.applyScanOutcome ({ second.generation, { newResult } });
    browser.applyScanOutcome ({ first.generation, { oldResult } });

    REQUIRE_FALSE (browser.scanSnapshot().busy);
    REQUIRE (namesOf (folderSection (browser, loops))
             == std::vector<std::string> { "new.wav", "old.wav" });
}

namespace
{
class FakeSourceAudition final : public SourceAudition
{
public:
    void play (std::filesystem::path file, std::string identity) override
    {
        playing = std::move (file);
        current.identity = std::move (identity);
        current.state = SourceAuditionState::playing;
        current.progress = 0.0;
        current.error.clear();
        ++plays;
        notify();
    }

    void stop() override
    {
        playing.reset();
        current = {};
        ++stops;
        notify();
    }

    [[nodiscard]] duet::gui::SourceAuditionStatus status() const override { return current; }

    void onChanged (std::function<void()> callback) override { changed = std::move (callback); }

    [[nodiscard]] const std::optional<std::filesystem::path>& playingFile() const
    {
        return playing;
    }
    [[nodiscard]] int playCount() const { return plays; }
    [[nodiscard]] int stopCount() const { return stops; }

private:
    void notify() const
    {
        if (changed)
            changed();
    }

    std::optional<std::filesystem::path> playing;
    duet::gui::SourceAuditionStatus current;
    int plays = 0;
    int stops = 0;
    std::function<void()> changed;
};
} // namespace

TEST_CASE ("selecting a sample marks it, and Space toggles Source audition of it")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    FakeSourceAudition audition;
    const auto loops = temp.folder() / "loops";
    writeSample (loops, "kick.wav");
    writeSample (loops, "snare.wav");
    browser.addSampleFolder (loops);
    browser.setSourceAudition (&audition);

    const auto kick = itemNamed (folderSection (browser, loops), "kick.wav");
    const auto snare = itemNamed (folderSection (browser, loops), "snare.wav");
    browser.select (kick.identity);

    REQUIRE (browser.selected() == kick.identity);
    REQUIRE (itemNamed (folderSection (browser, loops), "kick.wav").selected);
    REQUIRE_FALSE (itemNamed (folderSection (browser, loops), "snare.wav").selected);

    browser.toggleSourceAudition();
    REQUIRE (audition.playCount() == 1);
    REQUIRE (audition.status().identity == kick.identity);
    REQUIRE (audition.playingFile() == kick.file);
    REQUIRE (browser.sourceAuditionStatus().state == SourceAuditionState::playing);

    browser.toggleSourceAudition();
    REQUIRE (audition.stopCount() == 1);
    REQUIRE (browser.sourceAuditionStatus().state == SourceAuditionState::stopped);
}

TEST_CASE ("selecting another sample switches Source audition to it")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    FakeSourceAudition audition;
    const auto loops = temp.folder() / "loops";
    writeSample (loops, "kick.wav");
    writeSample (loops, "snare.wav");
    browser.addSampleFolder (loops);
    browser.setSourceAudition (&audition);

    const auto kick = itemNamed (folderSection (browser, loops), "kick.wav");
    const auto snare = itemNamed (folderSection (browser, loops), "snare.wav");
    browser.select (kick.identity);
    browser.toggleSourceAudition();
    browser.select (snare.identity);

    REQUIRE (audition.playCount() == 2);
    REQUIRE (audition.status().identity == snare.identity);
    REQUIRE (audition.playingFile() == snare.file);
    REQUIRE (itemNamed (folderSection (browser, loops), "snare.wav").selected);
}

TEST_CASE ("Source audition does not start for a non-sample, and replacing the project stops it")
{
    const TempProject temp;
    OpenBrowser open { temp.folder() / "project" };
    FakeSourceAudition audition;
    open.browser.setSourceAudition (&audition);

    const auto reverb = itemNamed (sectionOf (open.browser, BrowserSectionKind::effects), "Reverb");
    open.browser.select (reverb.identity);
    open.browser.toggleSourceAudition();
    REQUIRE (audition.playCount() == 0);

    const auto loops = temp.folder() / "loops";
    writeSample (loops, "kick.wav");
    open.browser.addSampleFolder (loops);
    const auto kick = itemNamed (folderSection (open.browser, loops), "kick.wav");
    open.browser.select (kick.identity);
    open.browser.toggleSourceAudition();
    REQUIRE (audition.playCount() == 1);

    const auto actionsBefore = open.session().undoNames().size();
    const auto playingBefore = open.session().isPlaying();
    const auto positionBefore = open.session().playbackPositionSeconds();

    open.browser.setSession (nullptr);
    REQUIRE (audition.stopCount() >= 1);
    REQUIRE (open.session().undoNames().size() == actionsBefore);
    REQUIRE (open.session().isPlaying() == playingBefore);
    REQUIRE (open.session().playbackPositionSeconds() == positionBefore);
}
