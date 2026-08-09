// PROTOTYPE — undo-vocabulary spike for roadmap node skb4tp. Disposable; never ship.
//
// The seam under test: this header IS Duet's edit vocabulary, and it exposes no
// tracktion:: or juce:: type. If this interface holds, a future engine swap
// re-implements DuetEdit.cpp and nothing that includes this header changes.
// The scenario runner (Main.cpp) is written entirely against this interface.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace duet
{
// Opaque handles wrapping the engine's EditItemID. 0 = invalid.
struct TrackRef { std::uint64_t raw = 0; bool valid() const { return raw != 0; } };
struct ClipRef  { std::uint64_t raw = 0; bool valid() const { return raw != 0; } };

struct ClipInfo
{
    ClipRef clip;
    std::string name;
    double startSec = 0, lengthSec = 0;
    bool isMidi = false;
    int midiNotes = 0;
};

struct TrackInfo
{
    TrackRef track;
    std::string name;
    std::vector<ClipInfo> clips;
    int automationPoints = 0;   // on the volume parameter's curve (spike-minimal)
};

//==============================================================================
// One operation of a Proposal, as data. A Proposal never touches the project
// until it is accepted; rejecting one is discarding this data.
struct ProposalOp
{
    enum class Kind { insertAudioClip, moveClip, trimClip, addMidiNote, setAutomationPoint };
    Kind kind;

    TrackRef track;       // insertAudioClip, setAutomationPoint
    ClipRef  clip;        // moveClip, trimClip, addMidiNote
    std::string name;     // insertAudioClip (clip name)
    std::string file;     // insertAudioClip (absolute path)
    double a = 0, b = 0;  // insert: start/length; move: newStart; trim: newLength;
                          // midi: startBeats/lengthBeats; automation: timeSec/value
    int pitch = 0, velocity = 0;

    std::string describe() const;
};

struct Proposal
{
    std::string title;
    std::vector<ProposalOp> ops;

    // An op may target the clip created by the nearest preceding insertAudioClip
    // in the same proposal, before that clip exists, via this placeholder.
    static constexpr std::uint64_t newestClip = ~0ull;
};

//==============================================================================
class ProjectEditor
{
public:
    ProjectEditor();    // builds the demo project: 4 tone audio tracks + 1 MIDI/4OSC track, 8 s loop
    ~ProjectEditor();

    //-- the vocabulary: each edit operation is expressed exactly once, here --
    ClipRef insertAudioClip (TrackRef, const std::string& name, const std::string& absFile,
                             double startSec, double lengthSec);
    void    moveClip (ClipRef, double newStartSec);
    void    trimClip (ClipRef, double newLengthSec);
    void    addMidiNote (ClipRef midiClip, int pitch, double startBeats, double lengthBeats, int velocity);
    void    setAutomationPoint (TrackRef, double timeSec, float normValue);  // volume curve

    //-- actions: the ONLY place an undo transaction begins -------------------
    // A producer gesture and an accepted Proposal both come through here.
    void performAction (const std::string& name, const std::function<void()>& ops);
    void acceptProposal (const Proposal&);   // one performAction wrapping all ops

    // Hazard demo: run ops with no action wrapper — what the layer must forbid.
    void nakedOps (const std::function<void()>& ops) { ops(); }

    //-- undo surface ---------------------------------------------------------
    bool undo();
    bool redo();
    std::vector<std::string> undoNames() const;  // element 0 = what undo() reverts next
    std::vector<std::string> redoNames() const;  // element 0 = what redo() re-applies next

    //-- queries / scenario support ------------------------------------------
    std::vector<TrackInfo> tracks() const;
    TrackRef trackByIndex (int) const;
    ClipRef  clipByName (const std::string&) const;
    ClipRef  midiClipRef() const;
    std::string audioFile (int index) const;     // demo tone files, absolute paths
    std::string stateDigest() const;             // hash of flushed state, volatile nodes stripped
    std::string stateXml() const;

    //-- transport, for the rolling-undo scenario ----------------------------
    void play();
    void stop();
    bool isPlaying() const;
    double positionSec() const;
    int xrunCount() const;
    std::string deviceDescription() const;

    void pumpMessages (int ms) const;            // run the message loop for ~ms

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet
