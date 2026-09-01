#include <duet/app/SampleFolderScanner.h>
#include <duet/gui/Browser.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <vector>

using duet::app::SampleFolderScanner;
using duet::gui::Browser;
using duet::testing::StoredSettings;
using duet::testing::TempProject;

namespace
{
/** Queues work until the test runs it. Never sleeps. */
class CommandableExecutor
{
public:
    void operator() (std::function<void()> work) { queue.push_back (std::move (work)); }

    [[nodiscard]] std::size_t pending() const { return queue.size(); }

    void runNext()
    {
        REQUIRE_FALSE (queue.empty());
        auto work = std::move (queue.front());
        queue.erase (queue.begin());
        work();
    }

    void runLast()
    {
        REQUIRE_FALSE (queue.empty());
        auto work = std::move (queue.back());
        queue.pop_back();
        work();
    }

private:
    std::vector<std::function<void()>> queue;
};

[[nodiscard]] std::optional<duet::gui::BrowserSection>
    folderSection (const Browser& browser, const std::filesystem::path& folder)
{
    const auto sections = browser.sections();
    const auto found = std::ranges::find (sections, folder, &duet::gui::BrowserSection::folder);

    return found == sections.end() ? std::nullopt : std::optional { *found };
}

[[nodiscard]] std::vector<std::string>
    namesOf (const std::optional<duet::gui::BrowserSection>& section)
{
    std::vector<std::string> names;

    if (! section.has_value())
        return names;

    names.reserve (section->items.size());

    for (const auto& item : section->items)
        names.push_back (item.name);

    return names;
}

void writeSample (const std::filesystem::path& folder, const std::string& fileName)
{
    std::filesystem::create_directories (folder);
    std::ofstream stream { folder / fileName, std::ios::binary };
    stream << "not really audio";
}
} // namespace

TEST_CASE ("a commandable scanner delivers progress then rows, without sleeping")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    const auto loops = temp.folder() / "loops";
    writeSample (loops, "kick.wav");

    CommandableExecutor worker;
    CommandableExecutor poster;
    SampleFolderScanner scanner { [&worker] (auto work) { worker (std::move (work)); },
                                  [&poster] (auto work) { poster (std::move (work)); } };
    scanner.attach (browser);

    browser.addSampleFolder (loops);

    REQUIRE (browser.scanSnapshot().busy);
    REQUIRE (browser.scanSnapshot().message == "Scanning… 0/1");
    REQUIRE (namesOf (folderSection (browser, loops)).empty());
    REQUIRE (worker.pending() == 1);
    REQUIRE (poster.pending() == 0);

    worker.runNext();
    REQUIRE (worker.pending() == 0);
    REQUIRE (poster.pending() >= 1);

    while (poster.pending() > 0)
        poster.runNext();

    REQUIRE_FALSE (browser.scanSnapshot().busy);
    REQUIRE (namesOf (folderSection (browser, loops)) == std::vector<std::string> { "kick.wav" });
}

TEST_CASE ("a blocked old generation cannot overwrite a completed newer generation")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    const auto loops = temp.folder() / "loops";
    writeSample (loops, "old.wav");

    CommandableExecutor worker;
    CommandableExecutor poster;
    SampleFolderScanner scanner { [&worker] (auto work) { worker (std::move (work)); },
                                  [&poster] (auto work) { poster (std::move (work)); } };
    scanner.attach (browser);

    browser.addSampleFolder (loops);
    REQUIRE (worker.pending() == 1);
    worker.runNext();
    // The old walk is finished and waiting on the poster: that is the block.
    REQUIRE (poster.pending() > 0);

    writeSample (loops, "new.wav");
    browser.refresh();
    REQUIRE (worker.pending() == 1);
    worker.runNext();

    poster.runLast();

    REQUIRE (namesOf (folderSection (browser, loops))
             == std::vector<std::string> { "new.wav", "old.wav" });

    while (poster.pending() > 0)
        poster.runNext();

    REQUIRE (namesOf (folderSection (browser, loops))
             == std::vector<std::string> { "new.wav", "old.wav" });
}

TEST_CASE ("a superseded queued scan posts nothing and is otherwise silent")
{
    const TempProject temp;
    StoredSettings store;
    Browser browser { store };
    const auto loops = temp.folder() / "loops";
    writeSample (loops, "kick.wav");

    CommandableExecutor worker;
    CommandableExecutor poster;
    SampleFolderScanner scanner { [&worker] (auto work) { worker (std::move (work)); },
                                  [&poster] (auto work) { poster (std::move (work)); } };
    scanner.attach (browser);

    browser.addSampleFolder (loops);
    browser.refresh();
    REQUIRE (worker.pending() == 2);
    REQUIRE (browser.scanSnapshot().busy);
    REQUIRE (browser.scanSnapshot().message == "Scanning… 0/1");

    worker.runNext();
    REQUIRE (poster.pending() == 0);

    worker.runNext();

    while (poster.pending() > 0)
        poster.runNext();

    REQUIRE_FALSE (browser.scanSnapshot().busy);
    REQUIRE (browser.scanSnapshot().message.empty());
    REQUIRE (namesOf (folderSection (browser, loops)) == std::vector<std::string> { "kick.wav" });
}
