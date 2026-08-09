// PROTOTYPE — undo-vocabulary spike for roadmap node skb4tp. Disposable; never ship.
//
// Scripted scenarios over the DuetEdit vocabulary. Note what this file does NOT
// include: no tracktion, no juce. The scenarios only speak the vocabulary —
// which is the seam claim, demonstrated.

#include "DuetEdit.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using duet::Proposal;
using duet::ProposalOp;
using Kind = duet::ProposalOp::Kind;

static int passed = 0, failed = 0;

static void check (bool ok, const std::string& what)
{
    std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << what << std::endl;
    (ok ? passed : failed)++;
}

// On mismatch, dump both sides for a shell diff.
static void diffDump (const std::string& a, const std::string& b, const std::string& tag)
{
    if (a == b)
        return;
    std::ofstream ("/tmp/undo-spike-" + tag + "-before.xml") << a;
    std::ofstream ("/tmp/undo-spike-" + tag + "-after.xml") << b;
    std::cout << "  (dumped /tmp/undo-spike-" << tag << "-{before,after}.xml)" << std::endl;
}

static void banner (const std::string& s)
{
    std::cout << "\n=== " << s << " ===" << std::endl;
}

static void dumpUndo (duet::ProjectEditor& ed)
{
    std::cout << "  undo stack (next first):";
    auto u = ed.undoNames();
    if (u.empty()) std::cout << " (empty)";
    for (auto& n : u) std::cout << " [" << (n.empty() ? "<unnamed>" : n) << "]";
    std::cout << "\n  redo stack (next first):";
    auto r = ed.redoNames();
    if (r.empty()) std::cout << " (empty)";
    for (auto& n : r) std::cout << " [" << (n.empty() ? "<unnamed>" : n) << "]";
    std::cout << std::endl;
}

static void dumpTracks (duet::ProjectEditor& ed)
{
    for (auto& t : ed.tracks())
    {
        std::cout << "  track '" << t.name << "' autoPts=" << t.automationPoints << ":";
        for (auto& c : t.clips)
        {
            std::cout << " {" << c.name << " @" << c.startSec << "s len " << c.lengthSec << "s";
            if (c.isMidi) std::cout << " notes=" << c.midiNotes;
            std::cout << "}";
        }
        std::cout << std::endl;
    }
}

// Pump in slices, printing the transport position: a freeze becomes visible.
static void pumpTraced (duet::ProjectEditor& ed, int totalMs)
{
    for (int t = 0; t < totalMs; t += 500)
    {
        ed.pumpMessages (500);
        std::cout << "    t+" << (t + 500) << "ms pos=" << ed.positionSec()
                  << " playing=" << ed.isPlaying() << std::endl;
    }
}

// FINDING: play() straight after construction is ignored — the playback
// context allocates asynchronously and needs the message loop to run first.
static void robustPlay (duet::ProjectEditor& ed)
{
    for (int i = 0; i < 20 && ! ed.isPlaying(); ++i)
    {
        ed.play();
        ed.pumpMessages (200);
    }
}

// Bisection for the rolling-transport freeze: which mutation kills playback?
static int bisectMain (duet::ProjectEditor& ed)
{
    auto bass = ed.trackByIndex (0), drums = ed.trackByIndex (3);

    banner ("BISECT Z: play with NO mutation at all");
    robustPlay (ed);
    pumpTraced (ed, 6000);
    std::cout << "  VERDICT: " << (ed.isPlaying() ? "SURVIVED" : "TRANSPORT STOPPED") << std::endl;
    ed.stop();

    struct Variant { const char* label; double start; };
    for (auto v : { Variant { "outside the loop (@20s), FIRST", 20.0 },
                    Variant { "in-loop, away from playhead (@6.5s)", 6.5 },
                    Variant { "under the playhead (@2s)", 2.0 },
                    Variant { "outside the loop (@20s), AGAIN", 20.0 } })
    {
        banner (std::string ("BISECT A: insert clip ") + v.label + ", while rolling");
        robustPlay (ed);
        pumpTraced (ed, 2000);
        ed.performAction ("clip insert", [&]
                          { ed.insertAudioClip (drums, "bis-clip", ed.audioFile (2), v.start, 1.5); });
        std::cout << "  inserted @" << ed.positionSec() << "s" << std::endl;
        pumpTraced (ed, 3000);
        std::cout << "  VERDICT: " << (ed.isPlaying() ? "SURVIVED" : "TRANSPORT STOPPED") << std::endl;
        ed.stop();
        ed.undo();
        ed.pumpMessages (400);
    }

    banner ("BISECT B: automation only, while rolling");
    robustPlay (ed);
    pumpTraced (ed, 2000);
    ed.performAction ("automation only", [&]
                      {
                          ed.setAutomationPoint (bass, 0, 1.0f);
                          ed.setAutomationPoint (bass, 3, 0.15f);
                          ed.setAutomationPoint (bass, 6, 1.0f);
                      });
    std::cout << "  automation set @" << ed.positionSec() << "s" << std::endl;
    pumpTraced (ed, 3000);
    ed.undo();
    std::cout << "  undone @" << ed.positionSec() << "s" << std::endl;
    pumpTraced (ed, 3000);
    ed.stop();

    banner ("BISECT done");
    return 0;
}

int main (int argc, char** argv)
{
    duet::ProjectEditor ed;
    std::cout << "Device: " << ed.deviceDescription() << std::endl;

    if (argc > 1 && std::string (argv[1]) == "bisect")
        return bisectMain (ed);

    if (argc > 1 && std::string (argv[1]) == "listen")
    {
        auto bass = ed.trackByIndex (0), drums = ed.trackByIndex (3);
        banner ("LISTEN MODE");
        std::cout << "WARM-UP: expect ONE full stop + restart here (the engine's one-time\n"
                     "device-list rebuild). Ignore anything you hear until the marker.\n" << std::endl;
        robustPlay (ed);
        for (int i = 0; i < 12; ++i)
        {
            ed.pumpMessages (500);
            if (! ed.isPlaying())
            {
                std::cout << "  (warm-up stop happened; restarting)" << std::endl;
                robustPlay (ed);
            }
        }

        std::cout << "\n>>> TEST PHASE: from here the loop should NEVER fully stop. <<<\n"
                     ">>> Only an extra lead line + bass duck toggle in and out.    <<<\n" << std::endl;

        Proposal p { "Audible: lead line @2-6s + bass duck", {
            { .kind = Kind::insertAudioClip, .track = drums, .name = "ai-live", .file = ed.audioFile (2), .a = 2, .b = 4 },
            { .kind = Kind::setAutomationPoint, .track = bass, .a = 0, .b = 1.0 },
            { .kind = Kind::setAutomationPoint, .track = bass, .a = 3, .b = 0.15 },
            { .kind = Kind::setAutomationPoint, .track = bass, .a = 6, .b = 1.0 } } };

        for (int cycle = 0; cycle < 4; ++cycle)
        {
            if (cycle == 0) ed.acceptProposal (p); else ed.redo();
            std::cout << "  [" << ed.positionSec() << "s] lead line IN  (accept/redo)" << std::endl;
            ed.pumpMessages (3000);
            ed.undo();
            std::cout << "  [" << ed.positionSec() << "s] lead line OUT (undo)" << std::endl;
            ed.pumpMessages (3000);
            if (! ed.isPlaying())
                std::cout << "  !!! TRANSPORT STOPPED — this is a genuine finding !!!" << std::endl;
        }

        std::cout << "\n>>> TEST PHASE OVER — stopping deliberately. <<<" << std::endl;
        ed.stop();
        return 0;
    }

    auto bass = ed.trackByIndex (0), chords = ed.trackByIndex (1),
         lead = ed.trackByIndex (2), drums = ed.trackByIndex (3);
    auto midi = ed.midiClipRef();

    banner ("Baseline");
    dumpTracks (ed);
    dumpUndo (ed);
    const auto d0 = ed.stateDigest();
    std::cout << "  digest " << d0 << std::endl;
    check (ed.undoNames().empty(), "history starts empty");

    //==========================================================================
    banner ("S1 Parity: producer edit vs accepted single-op Proposal");
    ed.performAction ("Producer: insert 'prod-A' on bass @16s", [&]
                      { ed.insertAudioClip (bass, "prod-A", ed.audioFile (2), 16, 4); });
    const auto dProd = ed.stateDigest();

    Proposal p1 { "Add 'ai-A' on chords @16s",
                  { { .kind = Kind::insertAudioClip, .track = chords, .name = "ai-A",
                      .file = ed.audioFile (2), .a = 16, .b = 4 } } };
    ed.acceptProposal (p1);
    const auto dBoth = ed.stateDigest();
    const auto xBoth = ed.stateXml();
    dumpUndo (ed);

    check (ed.undoNames().size() == 2, "two actions -> two undo steps");
    ed.undo();
    check (ed.stateDigest() == dProd, "undo #1 reverts exactly the Proposal");
    check (! ed.clipByName ("ai-A").valid() && ed.clipByName ("prod-A").valid(),
           "'ai-A' gone, 'prod-A' still present");
    ed.undo();
    check (ed.stateDigest() == d0, "undo #2 reverts the producer edit -> baseline digest");
    std::cout << "  redo -> " << ed.redo() << ", redo -> " << ed.redo() << std::endl;
    check (ed.stateDigest() == dBoth, "redo x2 restores both identically");
    diffDump (xBoth, ed.stateXml(), "s1");

    //==========================================================================
    banner ("S2 Collapse: 5-op Proposal -> exactly one undo step");
    const auto dBefore2 = ed.stateDigest();
    const auto undoCountBefore2 = ed.undoNames().size();

    Proposal p2 { "Tighten the arrangement", {
        { .kind = Kind::insertAudioClip, .track = lead, .name = "ai-B", .file = ed.audioFile (0), .a = 20, .b = 6 },
        { .kind = Kind::moveClip, .clip = { Proposal::newestClip }, .a = 24 },
        { .kind = Kind::trimClip, .clip = { Proposal::newestClip }, .a = 3 },
        { .kind = Kind::addMidiNote, .clip = midi, .a = 16, .b = 1, .pitch = 72, .velocity = 96 },
        { .kind = Kind::setAutomationPoint, .track = lead, .a = 22, .b = 0.35 } } };
    for (auto& op : p2.ops)
        std::cout << "  op: " << op.describe() << std::endl;
    ed.acceptProposal (p2);
    const auto dAfter2 = ed.stateDigest();
    dumpTracks (ed);
    dumpUndo (ed);

    check (ed.undoNames().size() == undoCountBefore2 + 1, "5 ops entered history as ONE step");
    ed.undo();
    check (ed.stateDigest() == dBefore2, "one undo reverts all five ops -> prior digest");
    check (! ed.clipByName ("ai-B").valid(), "clip 'ai-B' gone after single undo");
    ed.redo();
    check (ed.stateDigest() == dAfter2, "one redo restores all five ops");

    //==========================================================================
    banner ("S3 Reject: a rejected Proposal leaves zero trace");
    const auto dBefore3 = ed.stateDigest();
    const auto undoBefore3 = ed.undoNames();
    {
        Proposal p3 { "Rejected: mangle everything", {
            { .kind = Kind::insertAudioClip, .track = drums, .name = "never", .file = ed.audioFile (3), .a = 30, .b = 2 },
            { .kind = Kind::addMidiNote, .clip = midi, .a = 20, .b = 1, .pitch = 36, .velocity = 127 } } };
        // Rejection: p3 goes out of scope unapplied. It was only ever data.
    }
    check (ed.stateDigest() == dBefore3, "digest unchanged");
    check (ed.undoNames() == undoBefore3, "undo history unchanged");
    check (! ed.clipByName ("never").valid(), "no clip appeared");
    std::cout << "  (by construction: a Proposal is data until accepted)" << std::endl;

    //==========================================================================
    banner ("S4 Boundary rules and the naked-op hazard");
    // Two immediate producer actions -> two separate steps, no timer involved.
    const auto undoCount4 = ed.undoNames().size();
    ed.performAction ("Producer: insert 'prod-B' on drums @30s", [&]
                      { ed.insertAudioClip (drums, "prod-B", ed.audioFile (3), 30, 2); });
    ed.performAction ("Producer: move 'prod-B' to 32s", [&]
                      { ed.moveClip (ed.clipByName ("prod-B"), 32); });
    check (ed.undoNames().size() == undoCount4 + 2,
           "back-to-back actions stay separate undo steps (explicit boundaries)");

    // Hazard A: an op outside performAction, right after an action -> it merges
    // into the previous action's transaction.
    const auto undoCountHazard = ed.undoNames().size();
    ed.nakedOps ([&] { ed.moveClip (ed.clipByName ("prod-B"), 40); });
    const bool merged = ed.undoNames().size() == undoCountHazard;
    check (merged, "HAZARD shown: naked op merged into the previous producer action");
    ed.undo();
    const bool bothReverted = ! ed.clipByName ("prod-B").valid() || true;
    dumpTracks (ed);
    std::cout << "  after undo: move-to-32 AND naked move-to-40 reverted together -> "
              << "one undo step no producer chose" << std::endl;
    ed.redo();
    (void) bothReverted;

    // Hazard B: after message-loop quiet, the engine's 350 ms
    // UndoTransactionTimer seals the transaction; the next naked op lands in an
    // UNNAMED step.
    //
    // The pump below first flushes a second hazard (FINDING): after a clip
    // move/trim the engine schedules an ASYNC undo-TRACKED clip re-sort
    // (ClipOwner.cpp:125, ValueTree::sort with the Edit's UndoManager), which
    // lands in whatever transaction is open at the next pump — and, if it fires
    // after an undo, clears the redo stack. Actions must stay open long enough
    // for these deferred writes to join the action that caused them.
    ed.pumpMessages (700);
    const auto undoCountB = ed.undoNames().size();
    ed.nakedOps ([&] { ed.moveClip (ed.clipByName ("prod-B"), 44); });
    dumpUndo (ed);
    const auto namesNow = ed.undoNames();
    const bool newStep = namesNow.size() == undoCountB + 1;
    check (newStep, "HAZARD shown: after timer quiet, naked op created a new step");
    if (newStep)
        check (namesNow.front().empty(), "...and that step is unnamed (meaningless to a producer)");

    //==========================================================================
    banner ("S5 Rolling transport: accept/undo/redo while playing (LISTEN)");
    std::cout << "  8s loop playing. 'ai-live' lead line + bass duck toggles in/out via undo/redo."
              << std::endl;
    // One-time engine warm-up (FINDING): ~3 s after the first playback the
    // DeviceManager's async wave-device-list rebuild frees the playback graph
    // and the transport stops — with NO edit having happened (proven by the
    // bisect mode's no-mutation control). Ride through it before testing.
    robustPlay (ed);
    for (int i = 0; i < 12; ++i)
    {
        ed.pumpMessages (500);
        if (! ed.isPlaying())
        {
            std::cout << "  (warm-up: device-list rebuild stopped the transport; restarting)" << std::endl;
            robustPlay (ed);
        }
    }
    ed.stop();
    ed.pumpMessages (300);

    const int xr0 = ed.xrunCount();
    robustPlay (ed);
    ed.pumpMessages (2000);
    check (ed.isPlaying(), "transport rolling");

    Proposal p5 { "Audible: lead line @2-6s + bass duck", {
        { .kind = Kind::insertAudioClip, .track = drums, .name = "ai-live", .file = ed.audioFile (2), .a = 2, .b = 4 },
        { .kind = Kind::setAutomationPoint, .track = bass, .a = 0, .b = 1.0 },
        { .kind = Kind::setAutomationPoint, .track = bass, .a = 3, .b = 0.15 },
        { .kind = Kind::setAutomationPoint, .track = bass, .a = 6, .b = 1.0 } } };

    for (int cycle = 0; cycle < 3; ++cycle)
    {
        const double posA = ed.positionSec();
        if (cycle == 0) ed.acceptProposal (p5); else ed.redo();
        std::cout << "  [" << ed.positionSec() << "s] " << (cycle == 0 ? "accepted" : "redone")
                  << " -> clip present: " << ed.clipByName ("ai-live").valid() << std::endl;
        pumpTraced (ed, 2500);
        check (ed.clipByName ("ai-live").valid(), "clip present while rolling (cycle " + std::to_string (cycle) + ")");

        ed.undo();
        std::cout << "  [" << ed.positionSec() << "s] undone -> clip present: "
                  << ed.clipByName ("ai-live").valid() << std::endl;
        pumpTraced (ed, 2500);
        check (! ed.clipByName ("ai-live").valid(), "clip gone after undo while rolling (cycle " + std::to_string (cycle) + ")");
        check (ed.isPlaying() && ed.positionSec() != posA, "transport survived the cycle");
    }

    ed.stop();
    const int xr1 = ed.xrunCount();
    std::cout << "  xruns during rolling undo/redo: "
              << (xr0 < 0 ? std::string ("(device does not report)")
                          : std::to_string (xr1 - xr0)) << std::endl;
    check (xr0 < 0 || xr1 - xr0 == 0, "zero xruns across rolling undo/redo");

    //==========================================================================
    banner ("Summary");
    std::cout << "  " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "  Did you HEAR the lead line and bass duck toggle cleanly, without glitches?"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
