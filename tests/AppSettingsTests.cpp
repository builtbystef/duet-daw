#include <duet/app/PropertyStorageSettings.h>
#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE ("a setting written while a project is open outlives the project")
{
    // App-global settings and a session's engine settings are one file, and
    // whatever writes it writes the whole set it holds. So the producer
    // changing a setting with a project open, and the project closing
    // afterwards, is the case that says whether one store is behind both: a
    // second one would still hold the set it read before the change, and put it
    // back over this one on its way out.
    const std::string key = "interface.theme";
    const std::string chosen = "light";

    {
        const duet::testing::TempProject project;
        duet::model::Session session { project.editFile() };

        // An app-global setting of the engine's own, so that its side of the
        // store has something to write as the session closes.
        session.suppressDeviceRebuild();

        duet::app::PropertyStorageSettings settings;
        settings.setValue (key, chosen);
    }

    // Nothing holds the store open now, so this is the next launch reading what
    // the last one left on disk.
    const duet::app::PropertyStorageSettings nextLaunch;

    REQUIRE (nextLaunch.value (key) == chosen);
}
