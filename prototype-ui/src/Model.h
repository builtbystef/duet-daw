// PROTOTYPE (r4m858) — in-memory demo project. No persistence, no engine.
#pragma once
#include "Theme.h"
#include <vector>
#include <functional>

struct Note
{
    int pitch = 60;          // MIDI
    double start = 0, len = 1; // beats, clip-relative
    bool selected = false;
};

struct Clip
{
    juce::uint32 id = 0;
    juce::String name;
    double start = 0, len = 4; // beats, timeline
    bool isMidi = true;
    bool proposal = false;     // ghost clip owned by the pending Proposal
    bool selected = false;
    std::vector<Note> notes;
};

struct Track
{
    juce::String name;
    int colorIndex = 0;
    bool isMidi = true;
    std::vector<Clip> clips;
    float fader = 0.78f;       // 0..1
    float pan = 0.0f;          // -1..1
    bool mute = false, solo = false;
    // Proposal-owned mixer change (ghost fader)
    bool hasProposalFader = false;
    float proposalFader = 0.0f;
};

struct ProposalElement
{
    juce::String description;
    bool included = true;      // per-element cherry-pick
    int trackIndex = -1;
    juce::uint32 clipId = 0;   // 0 → mixer element
};

struct Proposal
{
    enum State { Pending, Auditioning, Accepted, Rejected };
    State state = Pending;
    juce::String title, summary;
    std::vector<ProposalElement> elements;
};

struct HistoryEntry { juce::String title; bool stale = false; };

struct ProjectModel
{
    double bpm = 120.0;
    int beatsPerBar = 4;
    std::vector<Track> tracks;
    Proposal proposal;
    std::vector<HistoryEntry> history;

    juce::uint32 nextId = 1;
    juce::uint32 newId() { return nextId++; }

    Clip* findClip (juce::uint32 id, int* trackIndexOut = nullptr)
    {
        for (int ti = 0; ti < (int) tracks.size(); ++ti)
            for (auto& c : tracks[ti].clips)
                if (c.id == id) { if (trackIndexOut) *trackIndexOut = ti; return &c; }
        return nullptr;
    }

    void clearSelection()
    {
        for (auto& t : tracks)
            for (auto& c : t.clips)
            {
                c.selected = false;
                for (auto& n : c.notes) n.selected = false;
            }
    }

    juce::String selectionSummary() const
    {
        int clips = 0, notes = 0;
        for (auto& t : tracks)
            for (auto& c : t.clips)
            {
                if (c.selected) ++clips;
                for (auto& n : c.notes) if (n.selected) ++notes;
            }
        if (clips == 1)
            for (auto& t : tracks)
                for (auto& c : t.clips)
                    if (c.selected) return c.name + " (" + t.name + ")";
        if (clips > 1) return juce::String (clips) + " clips";
        if (notes > 0) return juce::String (notes) + " notes";
        return "No selection";
    }
};

// Shared app state passed to every view.
struct AppState
{
    ProjectModel model;
    Theme theme = Theme::darkTheme();
    GraphiteLNF lnf;

    // transport
    bool playing = false, recording = false, loopOn = true, metronomeOn = false, followOn = true;
    double playheadBeats = 0.0;
    double loopStartBeats = 0.0, loopLenBeats = 32.0;

    // grid: index into gridChoices; 0 = Auto (adapts to zoom)
    int gridChoice = 0;

    // bottom panel
    bool browserVisible = true, collabVisible = true, bottomVisible = true;
    int bottomTab = 0; // 0 = piano roll, 1 = mixer
    juce::uint32 editingClipId = 0; // clip open in the piano roll

    // piano roll options
    bool scaleHighlight = true, fold = false;
    int noteLenChoiceBeats4 = 4; // default note length in quarter-beats (4 = one beat)

    std::function<void()> refreshAll; // set by MainComponent

    void refresh() { if (refreshAll) refreshAll(); }

    static double gridChoiceBeats (int choice)
    {
        // 1=1 bar, then 1/2 .. 1/32 note (beats, 4/4)
        static const double v[] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        return v[juce::jlimit (0, 6, choice)];
    }

    static juce::StringArray gridChoiceNames()
    {
        return { "Auto", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32" };
    }
};

inline void seedDemoProject (ProjectModel& m)
{
    auto midiClip = [&m] (const char* name, double start, double len, std::initializer_list<std::tuple<int, double, double>> ns)
    {
        Clip c; c.id = m.newId(); c.name = name; c.start = start; c.len = len; c.isMidi = true;
        for (auto& [p, s, l] : ns) c.notes.push_back ({ p, s, l });
        return c;
    };
    auto audioClip = [&m] (const char* name, double start, double len)
    {
        Clip c; c.id = m.newId(); c.name = name; c.start = start; c.len = len; c.isMidi = false;
        return c;
    };

    { Track t; t.name = "Drums"; t.colorIndex = 0; t.isMidi = false; t.fader = 0.82f;
      t.clips = { audioClip ("Drum Loop", 0, 16), audioClip ("Drum Loop", 16, 16), audioClip ("Fill", 32, 4) };
      m.tracks.push_back (t); }

    { Track t; t.name = "Bass"; t.colorIndex = 7; t.isMidi = true; t.fader = 0.74f;
      t.clips = { midiClip ("Bassline", 0, 8, { {36, 0, 0.75}, {36, 1.5, 0.5}, {43, 2.5, 0.5}, {36, 4, 0.75}, {38, 5.5, 0.5}, {41, 6.5, 1.0} }),
                  midiClip ("Bassline", 8, 8, { {36, 0, 0.75}, {36, 1.5, 0.5}, {43, 2.5, 0.5}, {36, 4, 0.75}, {38, 5.5, 0.5}, {41, 6.5, 1.0} }),
                  midiClip ("Bass Var", 16, 16, { {36, 0, 1}, {41, 2, 1}, {43, 4, 1}, {36, 6, 2}, {38, 8, 1}, {36, 10, 1}, {31, 12, 3} }) };
      m.tracks.push_back (t); }

    { Track t; t.name = "Keys"; t.colorIndex = 2; t.isMidi = true; t.fader = 0.66f;
      t.clips = { midiClip ("Chords", 0, 16, { {60, 0, 4}, {63, 0, 4}, {67, 0, 4}, {58, 4, 4}, {62, 4, 4}, {65, 4, 4},
                                               {60, 8, 4}, {63, 8, 4}, {67, 8, 4}, {56, 12, 4}, {60, 12, 4}, {63, 12, 4} }),
                  midiClip ("Chords B", 24, 8, { {60, 0, 4}, {63, 0, 4}, {67, 0, 4}, {62, 4, 4}, {65, 4, 4}, {69, 4, 4} }) };
      m.tracks.push_back (t); }

    { Track t; t.name = "Lead"; t.colorIndex = 4; t.isMidi = true; t.fader = 0.7f;
      t.clips = { midiClip ("Hook", 16, 8, { {72, 0, 0.5}, {75, 0.5, 0.5}, {79, 1, 1}, {77, 2.5, 0.5}, {75, 3, 1},
                                             {72, 4.5, 0.5}, {70, 5, 1}, {72, 6, 2} }) };
      m.tracks.push_back (t); }

    { Track t; t.name = "Vox"; t.colorIndex = 1; t.isMidi = false; t.fader = 0.8f;
      t.clips = { audioClip ("Verse Vox", 8, 14), audioClip ("Chorus Vox", 24, 8) };
      m.tracks.push_back (t); }

    { Track t; t.name = "Pad"; t.colorIndex = 6; t.isMidi = true; t.fader = 0.55f;
      t.clips = { midiClip ("Warm Pad", 0, 8, { {55, 0, 8}, {60, 0, 8}, {63, 0, 8} }) };
      m.tracks.push_back (t); }

    { Track t; t.name = "Perc"; t.colorIndex = 3; t.isMidi = false; t.fader = 0.6f;
      t.clips = { audioClip ("Shaker", 8, 8), audioClip ("Shaker", 24, 8) };
      m.tracks.push_back (t); }

    { Track t; t.name = "FX"; t.colorIndex = 5; t.isMidi = false; t.fader = 0.5f;
      t.clips = { audioClip ("Riser", 14, 2), audioClip ("Impact", 16, 1) };
      m.tracks.push_back (t); }

    // Pending Proposal: two ghost clips on Pad + a fader change on Bass.
    auto& prop = m.proposal;
    prop.title = "Fill the empty back half of Pad";
    prop.summary = "The pad drops out after bar 3 while Keys keep sustaining. Extending it under the "
                   "hook (bars 5–8) and the outro keeps the low-mid bed continuous; Bass comes up "
                   "1.5 dB to hold the low end under the new pad layer.";

    {
        Clip g; g.id = m.newId(); g.name = "Warm Pad (ext)"; g.start = 16; g.len = 8; g.isMidi = true; g.proposal = true;
        g.notes = { {53, 0, 8}, {58, 0, 8}, {62, 0, 8} };
        m.tracks[5].clips.push_back (g);
        prop.elements.push_back ({ "Add clip “Warm Pad (ext)” — Pad, bars 5–7", true, 5, g.id });
    }
    {
        Clip g; g.id = m.newId(); g.name = "Pad Outro"; g.start = 28; g.len = 4; g.isMidi = true; g.proposal = true;
        g.notes = { {55, 0, 4}, {60, 0, 4}, {65, 0, 4} };
        m.tracks[5].clips.push_back (g);
        prop.elements.push_back ({ "Add clip “Pad Outro” — Pad, bar 8", true, 5, g.id });
    }
    {
        m.tracks[1].hasProposalFader = true;
        m.tracks[1].proposalFader = 0.82f;
        prop.elements.push_back ({ "Bass fader +1.5 dB", true, 1, 0 });
    }

    m.history.push_back ({ "Tighten drum timing (bars 1–4)", false });
    m.history.push_back ({ "EQ: cut 300 Hz mud on Keys", true });
}
