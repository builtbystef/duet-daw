#include <duet/gui/ArrangementView.h>
#include <duet/gui/Export.h>
#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>

using duet::gui::Export;
using duet::gui::ExportState;
using duet::model::ExportFormat;
using duet::model::Session;
using duet::model::TrackKind;
using duet::testing::pumpUntil;
using duet::testing::TempProject;

/** The Export/Bounce dialog's view-model (issue zm174o).

    The paintless half: what the producer chooses, what the project makes of
    those choices by default, and the render they start. What the render itself
    produces is asserted at the model seam, in ExportTests.
*/
namespace
{
/** A project called something, holding a tone over its first four bars at the
    project's own 120 bpm — eight seconds of content, and so four bars of it.
*/
struct ExportableProject
{
    ExportableProject()
    {
        const auto tone = project.writeTone ("tone.wav", 8.0, 440.0);

        session.useNoAudioDevice();
        session.suppressDeviceRebuild();
        session.performAction ("Add a tone",
                               [&] (auto& ops)
                               {
                                   const auto track = ops.createTrack (TrackKind::audio, "Tone");
                                   ops.insertAudioClip (track, "tone", tone, 0.0, 8.0);
                               });
    }

    /** An Export already pointed at this project, the way the host points one as
        a project opens.
    */
    void open (Export& dialog)
    {
        dialog.setProject (duet::testing::lent (session), "Night Drive", project.folder());
    }

    TempProject project;
    Session session { project.editFile() };
};

/** How long a whole export of eight seconds is given before a case gives up on
    it. Far longer than one costs on an idle machine, and short enough that a
    broken one fails rather than hangs.
*/
constexpr int exportTimeoutMs = 30000;
} // namespace

//==============================================================================
TEST_CASE ("an export defaults to the project's name, its folder and the bars it reaches")
{
    ExportableProject content;
    Export dialog;
    content.open (dialog);

    REQUIRE (dialog.name() == "Night Drive");
    REQUIRE (dialog.destinationFolder() == content.project.folder());

    // Eight seconds of content at 120 bpm in 4/4 is four two-second bars, and
    // the producer counts them from one.
    REQUIRE (dialog.firstBar() == 1);
    REQUIRE (dialog.lastBar() == 4);

    REQUIRE (dialog.startSeconds() == Catch::Approx (0.0));
    REQUIRE (dialog.endSeconds() == Catch::Approx (8.0));
}

TEST_CASE ("an export offers a format, a depth, a rate and normalise, and defaults them")
{
    ExportableProject content;
    Export dialog;
    content.open (dialog);

    REQUIRE (dialog.format() == ExportFormat::wav);
    REQUIRE (dialog.bitDepth() == 24);
    REQUIRE (dialog.sampleRate() == Catch::Approx (44100.0));
    REQUIRE_FALSE (dialog.normalise());

    const auto& rates = Export::availableSampleRates();
    REQUIRE (std::ranges::find (rates, 44100.0) != rates.end());
    REQUIRE (std::ranges::find (rates, 48000.0) != rates.end());

    dialog.setFormat (ExportFormat::flac);
    dialog.setSampleRate (48000.0);
    dialog.setNormalise (true);

    REQUIRE (dialog.format() == ExportFormat::flac);
    REQUIRE (dialog.sampleRate() == Catch::Approx (48000.0));
    REQUIRE (dialog.normalise());
}

TEST_CASE ("the destination is the folder, the name and the format's own extension")
{
    ExportableProject content;
    Export dialog;
    content.open (dialog);

    REQUIRE (dialog.destination() == content.project.folder() / "Night Drive.wav");

    dialog.setFormat (ExportFormat::flac);
    REQUIRE (dialog.destination() == content.project.folder() / "Night Drive.flac");

    dialog.setFormat (ExportFormat::aiff);
    REQUIRE (dialog.destination() == content.project.folder() / "Night Drive.aiff");

    dialog.setName ("Rough Mix");
    dialog.setDestinationFolder (content.project.folder() / "bounces");
    REQUIRE (dialog.destination() == content.project.folder() / "bounces" / "Rough Mix.aiff");
}

TEST_CASE ("a format that cannot be written at the chosen depth takes the nearest it can")
{
    ExportableProject content;
    Export dialog;
    content.open (dialog);

    dialog.setBitDepth (32);
    REQUIRE (dialog.bitDepth() == 32);

    // FLAC stops at 24 bits, so the producer who switches to it keeps a depth
    // that exists rather than one the file could not hold.
    dialog.setFormat (ExportFormat::flac);
    REQUIRE (dialog.bitDepth() == 24);

    const auto depths = dialog.availableBitDepths();
    REQUIRE (std::ranges::find (depths, 24) != depths.end());
    REQUIRE (std::ranges::find (depths, 32) == depths.end());
}

TEST_CASE ("a range the producer types is a range something can be written from")
{
    ExportableProject content;
    Export dialog;
    content.open (dialog);

    dialog.setRange (2, 3);
    REQUIRE (dialog.firstBar() == 2);
    REQUIRE (dialog.lastBar() == 3);
    REQUIRE (dialog.startSeconds() == Catch::Approx (2.0));
    REQUIRE (dialog.endSeconds() == Catch::Approx (6.0));

    // A last bar before the first is the one bar the range starts in, and a bar
    // below one is bar one: neither is a reason to write nothing.
    dialog.setRange (4, 2);
    REQUIRE (dialog.firstBar() == 4);
    REQUIRE (dialog.lastBar() == 4);
    REQUIRE (dialog.endSeconds() > dialog.startSeconds());

    dialog.setRange (0, 0);
    REQUIRE (dialog.firstBar() == 1);
    REQUIRE (dialog.lastBar() == 1);
}

TEST_CASE ("what the producer chose is what the model is asked for")
{
    ExportableProject content;
    Export dialog;
    content.open (dialog);

    dialog.setName ("Bounce");
    dialog.setFormat (ExportFormat::aiff);
    dialog.setBitDepth (16);
    dialog.setSampleRate (48000.0);
    dialog.setNormalise (true);
    dialog.setRange (1, 2);

    const auto options = dialog.options();

    REQUIRE (options.destination == content.project.folder() / "Bounce.aiff");
    REQUIRE (options.format == ExportFormat::aiff);
    REQUIRE (options.bitDepth == 16);
    REQUIRE (options.sampleRate == Catch::Approx (48000.0));
    REQUIRE (options.normalise);
    REQUIRE (options.startSeconds == Catch::Approx (0.0));
    REQUIRE (options.endSeconds == Catch::Approx (4.0));
}

TEST_CASE ("an export runs somewhere else, reports its progress, and says when it is written")
{
    ExportableProject content;
    Export dialog;
    content.open (dialog);

    dialog.setName ("Whole");

    REQUIRE (dialog.state() == ExportState::idle);
    REQUIRE (dialog.start());
    REQUIRE (dialog.state() == ExportState::running);

    // The message thread is free while the render runs, which is the whole point
    // of putting it somewhere else: the loop below is that thread going about
    // its business.
    REQUIRE (pumpUntil ([&] { return ! dialog.isRunning(); }, exportTimeoutMs));

    REQUIRE (dialog.state() == ExportState::written);
    REQUIRE (dialog.progress() == Catch::Approx (1.0).margin (0.001));
    REQUIRE (dialog.writtenFile() == content.project.folder() / "Whole.wav");
    REQUIRE (std::filesystem::exists (dialog.writtenFile()));
}

TEST_CASE ("a cancelled export ends cancelled and leaves no file behind")
{
    ExportableProject content;
    Export dialog;
    content.open (dialog);

    dialog.setName ("Abandoned");

    const auto destination = dialog.destination();

    REQUIRE (dialog.start());
    dialog.cancel();

    REQUIRE (pumpUntil ([&] { return ! dialog.isRunning(); }, exportTimeoutMs));

    REQUIRE (dialog.state() == ExportState::cancelled);
    REQUIRE (dialog.writtenFile().empty());
    REQUIRE_FALSE (std::filesystem::exists (destination));
}

TEST_CASE ("an export with no project open does nothing")
{
    Export dialog;

    REQUIRE_FALSE (dialog.start());
    REQUIRE (dialog.state() == ExportState::idle);
    REQUIRE (dialog.destination().empty());
}

TEST_CASE ("exporting leaves the producer's selection where it was")
{
    ExportableProject content;
    duet::gui::ViewState view;
    duet::gui::ArrangementView arrangement { view };
    arrangement.setSession (&content.session);

    const auto tracks = content.session.tracks();
    REQUIRE_FALSE (tracks.empty());

    const auto tone =
        std::ranges::find_if (tracks, [] (const auto& info) { return info.name == "Tone"; });
    REQUIRE (tone != tracks.end());
    REQUIRE_FALSE (tone->clips.empty());

    const duet::gui::SelectedItem clip { duet::gui::SelectionKind::clip, tone->clips.front().clip };
    arrangement.selection().click (clip, { clip }, false, false);
    REQUIRE (arrangement.selection().items().size() == 1);

    Export dialog;
    content.open (dialog);
    dialog.setName ("Beside The Selection");

    REQUIRE (dialog.start());
    REQUIRE (pumpUntil ([&] { return ! dialog.isRunning(); }, exportTimeoutMs));
    REQUIRE (dialog.state() == ExportState::written);

    // The export never touched it, and could not have: it is the arrangement's,
    // and an export is a read of the project.
    REQUIRE (arrangement.selection().items().size() == 1);
    REQUIRE (arrangement.selection().contains (clip));
}
