#include <duet/gui/Browser.h>
#include <duet/gui/PluginScan.h>

#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

using duet::gui::Browser;
using duet::gui::PluginScan;
using duet::model::Session;
using duet::testing::StoredSettings;
using duet::testing::TempProject;

/** The plugin-scan dialog's view-model (issue zm174o).

    A scan is stepped rather than run, so that the producer watches it: every
    case here is that stepping, and what the dialog reads between the steps.
    That the scan happens out of process, and that a crashing plugin lands in
    the results rather than taking the app down, is asserted at the model seam
    in PluginHostingTests; what this file adds is that the dialog reports it.
*/
namespace
{
std::filesystem::path copyFixture (const std::filesystem::path& fixture,
                                   const std::filesystem::path& directory)
{
    const auto copy = directory / fixture.filename();
    std::filesystem::copy (fixture,
                           copy,
                           std::filesystem::copy_options::recursive
                               | std::filesystem::copy_options::overwrite_existing);

    return copy;
}

/** How many steps a scan of two fixture bundles is given before a case gives up
    on it. Far more than either needs, and short enough that a scan that never
    ends fails rather than hangs.
*/
constexpr int mostSteps = 200;

/** Runs the scan to its end and answers how many steps it took.

    A step scans the plugin it was on and says whether there is another after
    it, so the last plugin of a directory is scanned by the step that answers
    no: the count is of steps taken, not of steps that said yes.
*/
int runToTheEnd (PluginScan& scan)
{
    int steps = 0;

    while (steps < mostSteps)
    {
        ++steps;

        if (! scan.step())
            break;
    }

    return steps;
}

/** How many times one plugin file is in what the scan has found.

    The known plugin list is app-global — that is what lets a scanned plugin
    survive a restart — so its size counts what every other scan in the same
    process put there as well. What a case here means is about its own fixture,
    and this is that question rather than the size of the list.
*/
[[nodiscard]] std::size_t timesFound (const PluginScan& scan, const std::filesystem::path& file)
{
    const auto found = scan.found();

    return static_cast<std::size_t> (std::ranges::count_if (
        found, [&file] (const auto& plugin) { return plugin.file == file; }));
}
} // namespace

//==============================================================================
TEST_CASE ("a scan steps through a directory, saying which plugin it is looking at", "[plugins]")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);

    const auto good = copyFixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);

    Session session { temp.editFile() };
    PluginScan scan;
    scan.setSession (&session);
    scan.setDirectories ({ pluginDirectory });

    REQUIRE_FALSE (scan.isRunning());
    REQUIRE (scan.scansOutOfProcess());
    REQUIRE (scan.start());
    REQUIRE (scan.isRunning());

    // The plugin about to be looked at, which is what the dialog shows while it
    // is being looked at.
    REQUIRE (scan.scanningNow() == good);
    REQUIRE (scan.progress() < 1.0);

    REQUIRE (runToTheEnd (scan) > 0);

    REQUIRE_FALSE (scan.isRunning());
    REQUIRE (scan.hasFinished());
    REQUIRE (scan.progress() == 1.0);
    REQUIRE (scan.scanningNow().empty());

    const auto found = scan.found();
    REQUIRE (std::ranges::any_of (found, [&] (const auto& plugin) { return plugin.file == good; }));
}

TEST_CASE ("a plugin that crashes the scanner is in the results rather than missing", "[plugins]")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);

    const auto good = copyFixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);
    const auto crashing = copyFixture (DUET_CRASHING_VST3_FIXTURE, pluginDirectory);

    Session session { temp.editFile() };
    PluginScan scan;
    scan.setSession (&session);
    scan.setDirectories ({ pluginDirectory });

    REQUIRE (scan.start());
    static_cast<void> (runToTheEnd (scan));

    REQUIRE (scan.hasFinished());

    // The app is still here to be asked, which is the whole point of scanning
    // out of process — and the plugin that killed the scanner is named.
    REQUIRE (std::ranges::find (scan.rejected(), crashing) != scan.rejected().end());
    REQUIRE (std::ranges::any_of (scan.found(),
                                  [&] (const auto& plugin) { return plugin.file == good; }));
}

TEST_CASE ("a finished scan puts what it found in the browser, without a restart", "[plugins]")
{
    const TempProject temp;
    const auto pluginDirectory = temp.folder() / "vst3";
    std::filesystem::create_directory (pluginDirectory);

    const auto good = copyFixture (DUET_GOOD_VST3_FIXTURE, pluginDirectory);

    Session session { temp.editFile() };
    StoredSettings store;
    Browser dock { store };
    dock.setSession (&session);
    dock.refresh();

    const auto pluginsIn = [] (const Browser& browser)
    {
        for (const auto& section : browser.sections())
            if (section.kind == duet::gui::BrowserSectionKind::plugins)
                return section.items.size();

        return std::size_t { 0 };
    };

    REQUIRE (pluginsIn (dock) == 0);

    PluginScan scan;
    scan.setSession (&session);
    scan.setDirectories ({ pluginDirectory });
    scan.onFinished ([&dock] { dock.refresh(); });

    REQUIRE (scan.start());
    static_cast<void> (runToTheEnd (scan));

    // The dock read the list again when the scan ended: nothing was restarted.
    REQUIRE (pluginsIn (dock) == 1);

    // And scanning again finds the same one plugin rather than a second copy of
    // it.
    REQUIRE (scan.start());
    static_cast<void> (runToTheEnd (scan));

    REQUIRE (pluginsIn (dock) == 1);
    REQUIRE (timesFound (scan, good) == 1);
}

TEST_CASE ("a cancelled scan keeps what it had found by then", "[plugins]")
{
    const TempProject temp;

    // Two directories with a plugin in each, so that there is a moment at which
    // the scan has found one of them and not the other.
    const auto first = temp.folder() / "vst3-first";
    const auto second = temp.folder() / "vst3-second";
    std::filesystem::create_directory (first);
    std::filesystem::create_directory (second);

    const auto inFirst = copyFixture (DUET_GOOD_VST3_FIXTURE, first);
    const auto inSecond = copyFixture (DUET_GOOD_VST3_FIXTURE, second);

    Session session { temp.editFile() };
    PluginScan scan;
    scan.setSession (&session);
    scan.setDirectories ({ first, second });

    bool ended = false;
    scan.onFinished ([&ended] { ended = true; });

    REQUIRE (scan.start());

    // The first directory's one plugin, and the second directory opened.
    REQUIRE (scan.step());
    REQUIRE (timesFound (scan, inFirst) == 1);
    REQUIRE (timesFound (scan, inSecond) == 0);

    scan.cancel();

    REQUIRE_FALSE (scan.isRunning());
    REQUIRE (ended);
    REQUIRE (timesFound (scan, inFirst) == 1);
    REQUIRE (timesFound (scan, inSecond) == 0);
}

TEST_CASE ("a scan with nowhere to look does nothing", "[plugins]")
{
    const TempProject temp;
    Session session { temp.editFile() };

    PluginScan scan;
    scan.setSession (&session);
    scan.setDirectories ({ temp.folder() / "there-is-no-such-folder" });

    bool ended = false;
    scan.onFinished ([&ended] { ended = true; });

    REQUIRE_FALSE (scan.start());
    REQUIRE_FALSE (scan.isRunning());
    REQUIRE (ended);

    PluginScan withNoProject;
    REQUIRE_FALSE (withNoProject.start());
    REQUIRE (withNoProject.directories().empty());
    REQUIRE (withNoProject.found().empty());
}

TEST_CASE ("a scan the producer did not point anywhere looks where the machine keeps VST3s",
           "[plugins]")
{
    const TempProject temp;
    Session session { temp.editFile() };

    PluginScan scan;
    scan.setSession (&session);

    // The format's own answer, and the model's: what the machine has is the
    // machine's business, so the case asserts the two agree rather than what
    // they say.
    REQUIRE (scan.directories() == session.vst3Directories());
}
