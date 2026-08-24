#include <duet/gui/TransportBar.h>
#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>
#include <duet/persistence/Project.h>
#include <duet/persistence/ProjectLayout.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

using duet::gui::CpuHealth;
using duet::gui::GridSize;
using duet::gui::TransportBar;
using duet::gui::ViewState;

namespace
{
struct OpenTransport
{
    OpenTransport()
    {
        project = duet::persistence::Project::create (temp.folder() / "Untitled 1");
        REQUIRE (project != nullptr);
        project->session().suppressDeviceRebuild();
        project->session().useNoAudioDevice();
        transport.setSession (&project->session());
    }

    duet::testing::TempProject temp;
    std::unique_ptr<duet::persistence::Project> project;
    ViewState view;
    TransportBar transport { view };
};
} // namespace

TEST_CASE ("the transport readouts express one fixed-width instant in musical and wall time")
{
    OpenTransport open;
    open.project->session().setPlaybackPositionSeconds (62.25);

    REQUIRE (open.transport.musicalPosition().size() == std::string { "001.01.000" }.size());
    REQUIRE (open.transport.wallTime() == "00:01:02.250");

    open.project->session().setPlaybackPositionSeconds (0.5);
    REQUIRE (open.transport.musicalPosition().size() == std::string { "001.01.000" }.size());
    REQUIRE (open.transport.wallTime().size() == std::string { "00:01:02.250" }.size());
}

TEST_CASE ("tempo and metre edits are Actions and undo follows them")
{
    OpenTransport open;

    open.transport.setTempo (140.0);
    REQUIRE (open.project->session().tempoBpm() == 140.0);
    REQUIRE (open.project->session().undoNames().front() == "Set Tempo");

    open.transport.setTimeSignature (3, 4);
    REQUIRE (open.project->session().timeSignature().numerator == 3);
    REQUIRE (open.project->session().undoNames().front() == "Set Time Signature");

    REQUIRE (open.transport.undo());
    REQUIRE (open.transport.undo());
    REQUIRE (open.project->session().tempoBpm() == 120.0);
}

TEST_CASE ("transport controls never add an Action")
{
    OpenTransport open;
    const auto before = open.project->session().undoNames();

    open.transport.toggleLoop();
    open.transport.toggleMetronome();
    open.transport.goToEnd();
    open.transport.goToStart();
    open.transport.togglePlayback();
    open.transport.togglePlayback();

    REQUIRE (open.project->session().undoNames() == before);
    REQUIRE (open.project->session().isLooping());
    REQUIRE (open.project->session().metronomeEnabled());
}

TEST_CASE ("undo labels and project status are the bar's producer-facing words")
{
    OpenTransport open;
    open.transport.setProjectStatus ("Untitled 1", false);

    REQUIRE (open.transport.projectLabel() == "Untitled 1");
    REQUIRE_FALSE (open.transport.canUndo());

    open.transport.setTempo (140.0);
    open.transport.setProjectStatus ("Untitled 1", true);
    REQUIRE (open.transport.undoLabel() == "Undo Set Tempo");
    REQUIRE (open.transport.projectLabel() == "Untitled 1 *");

    REQUIRE (open.transport.undo());
    REQUIRE (open.transport.redoLabel() == "Redo Set Tempo");
}

TEST_CASE ("grid choice and follow state live in the project view")
{
    OpenTransport open;
    open.transport.setGridSize (GridSize::eighth);
    open.transport.toggleFollowPlayhead();

    REQUIRE (open.view.gridSize() == GridSize::eighth);
    REQUIRE_FALSE (open.view.followPlayhead());

    open.transport.setGridSize (GridSize::adaptive);
    REQUIRE (open.view.gridSize() == GridSize::adaptive);
}

TEST_CASE ("Record from the bar lands a take inside an untitled project's audio folder")
{
    OpenTransport open;
    auto& session = open.project->session();
    const auto track = session.tracks().front().track;
    duet::model::InputRef audioInput = duet::model::noInput;

    for (const auto& input : session.availableInputs())
        if (input.kind == duet::model::InputKind::audio)
        {
            audioInput = input.input;
            break;
        }

    REQUIRE (audioInput != duet::model::noInput);
    session.setTrackInput (track, audioInput);
    session.setTrackRecordArmed (track, true);

    open.transport.toggleRecording();
    session.runWithoutAudioDevice (0.25, { {}, 220.0, 0.5 });
    open.transport.toggleRecording();

    const auto clips = session.track (track).clips;
    REQUIRE (clips.size() == 1);
    REQUIRE (clips.front().sourceFile.parent_path()
             == duet::persistence::audioDirectory (open.project->folder()));
    REQUIRE (std::filesystem::is_regular_file (clips.front().sourceFile));
}

TEST_CASE ("sustained overload changes CPU health without smoothing on the audio path")
{
    OpenTransport open;

    for (int sample = 0; sample < 29; ++sample)
        open.transport.observeCpuLoad (0.95);
    REQUIRE (open.transport.cpuHealth() == CpuHealth::healthy);

    open.transport.observeCpuLoad (0.97);
    REQUIRE (open.transport.cpuHealth() == CpuHealth::overloaded);
    REQUIRE (open.transport.cpuPercent() == 97);

    open.transport.observeCpuLoad (0.2);
    REQUIRE (open.transport.cpuHealth() == CpuHealth::healthy);
}
