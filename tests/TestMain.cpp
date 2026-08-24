#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_session.hpp>

#include <span>
#include <string>

int main (int argc, char* argv[])
{
    std::string commandLine;

    const std::span arguments { argv, static_cast<std::size_t> (argc) };
    bool first = true;

    for (auto* argument : arguments.subspan (1))
    {
        if (! first)
            commandLine += ' ';

        commandLine += argument;
        first = false;
    }

    if (duet::model::Session::startPluginScanChild (commandLine))
        for (;;)
            duet::testing::pumpMessages (1000);

    return Catch::Session {}.run (argc, argv);
}
