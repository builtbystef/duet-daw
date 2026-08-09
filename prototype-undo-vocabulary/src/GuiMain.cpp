// PROTOTYPE — undo-vocabulary spike for roadmap node skb4tp. Disposable; never ship.
//
// Small UI over the SAME vocabulary layer the scripted scenarios exercise.
// Every button goes through duet::ProjectEditor — no tracktion:: in this file.

#include "DuetEdit.h"

#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent()
    {
        auto initButton = [this] (juce::Button& b) { addAndMakeVisible (b); };
        for (auto* b : { &playButton, &stopButton, &acceptButton, &undoButton,
                         &redoButton, &rejectButton, &prodInsertButton, &prodMoveButton })
            initButton (*b);

        playButton.onClick = [this] { ed.play(); log ("Play. (The FIRST stop moments after first play is the engine's one-time device-list rebuild — press Play again.)"); };
        stopButton.onClick = [this] { ed.stop(); log ("Stop."); };

        acceptButton.onClick = [this]
        {
            ++proposalCount;
            auto name = "ai-" + std::to_string (proposalCount);
            duet::Proposal p { "lead line '" + name + "' @2-6s + bass duck", {
                { .kind = duet::ProposalOp::Kind::insertAudioClip, .track = ed.trackByIndex (3),
                  .name = name, .file = ed.audioFile (2), .a = 2, .b = 4 },
                { .kind = duet::ProposalOp::Kind::setAutomationPoint, .track = ed.trackByIndex (0), .a = 0, .b = 1.0 },
                { .kind = duet::ProposalOp::Kind::setAutomationPoint, .track = ed.trackByIndex (0), .a = 3, .b = 0.15 },
                { .kind = duet::ProposalOp::Kind::setAutomationPoint, .track = ed.trackByIndex (0), .a = 6, .b = 1.0 } } };
            ed.acceptProposal (p);
            log ("Accepted Proposal '" + p.title + "' (4 ops -> ONE undo step).");
        };

        rejectButton.onClick = [this]
        {
            duet::Proposal p { "rejected proposal", {
                { .kind = duet::ProposalOp::Kind::insertAudioClip, .track = ed.trackByIndex (3),
                  .name = "never", .file = ed.audioFile (3), .a = 2, .b = 2 } } };
            // Rejection: discard the data. Watch the stacks NOT change.
            log ("Built a Proposal, rejected it. Undo stack and project untouched.");
        };

        prodInsertButton.onClick = [this]
        {
            ++prodCount;
            auto name = "prod-" + std::to_string (prodCount);
            const double start = 4.5 + (prodCount % 3) * 0.6;
            ed.performAction ("Producer: insert '" + name + "'", [this, name, start]
                              { ed.insertAudioClip (ed.trackByIndex (3), name, ed.audioFile (3), start, 1.0); });
            log ("Producer inserted '" + name + "' — same vocabulary, one undo step.");
        };

        prodMoveButton.onClick = [this]
        {
            if (prodCount == 0) { log ("No producer clip yet."); return; }
            auto name = "prod-" + std::to_string (prodCount);
            auto clip = ed.clipByName (name);
            if (! clip.valid()) { log ("'" + name + "' is gone (undone?)."); return; }
            const double start = 2.0 + (prodCount % 4) * 1.1;
            ed.performAction ("Producer: move '" + name + "'", [this, clip, start]
                              { ed.moveClip (clip, start); });
            log ("Producer moved '" + name + "'.");
        };

        undoButton.onClick = [this] { log (ed.undo() ? "Undo." : "Nothing to undo."); };
        redoButton.onClick = [this] { log (ed.redo() ? "Redo." : "Nothing to redo."); };

        auto initView = [this] (juce::TextEditor& t)
        {
            t.setMultiLine (true);
            t.setReadOnly (true);
            t.setCaretVisible (false);
            t.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
            addAndMakeVisible (t);
        };
        initView (undoView);
        initView (trackView);

        addAndMakeVisible (statusLabel);
        addAndMakeVisible (logLabel);
        statusLabel.setFont (juce::FontOptions (14.0f));
        logLabel.setFont (juce::FontOptions (13.0f));

        log ("Press Play. Then Accept / Undo / Redo while it rolls and listen + watch.");
        startTimerHz (10);
        setSize (860, 640);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);

        auto row1 = r.removeFromTop (32);
        for (auto* b : { &playButton, &stopButton, &prodInsertButton, &prodMoveButton })
            b->setBounds (row1.removeFromLeft (row1.getWidth() / 5).reduced (2, 0));
        statusLabel.setBounds (row1);

        auto row2 = r.removeFromTop (32);
        for (auto* b : { &acceptButton, &rejectButton, &undoButton, &redoButton })
            b->setBounds (row2.removeFromLeft (row2.getWidth() / 4).reduced (2, 0));

        logLabel.setBounds (r.removeFromBottom (44));
        undoView.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (2));
        trackView.setBounds (r.reduced (2));
    }

private:
    void timerCallback() override
    {
        statusLabel.setText (juce::String (ed.isPlaying() ? "PLAYING " : "stopped ")
                                 + juce::String (ed.positionSec(), 2) + "s | xruns "
                                 + juce::String (ed.xrunCount()),
                             juce::dontSendNotification);

        juce::String u ("UNDO STACK (next first):\n");
        for (auto& n : ed.undoNames())
            u << "  [" << (n.empty() ? juce::String ("<unnamed>") : juce::String (n)) << "]\n";
        u << "\nREDO STACK (next first):\n";
        for (auto& n : ed.redoNames())
            u << "  [" << (n.empty() ? juce::String ("<unnamed>") : juce::String (n)) << "]\n";
        if (u != undoView.getText())
            undoView.setText (u);

        juce::String t;
        for (auto& tr : ed.tracks())
        {
            t << tr.name << "  (autoPts " << tr.automationPoints << ")\n";
            for (auto& c : tr.clips)
            {
                t << "   " << c.name << " @" << juce::String (c.startSec, 2)
                  << "s len " << juce::String (c.lengthSec, 2) << "s";
                if (c.isMidi) t << "  notes=" << c.midiNotes;
                t << "\n";
            }
        }
        if (t != trackView.getText())
            trackView.setText (t);
    }

    void log (const std::string& s)
    {
        logLabel.setText (juce::String (s), juce::dontSendNotification);
    }

    duet::ProjectEditor ed;
    int proposalCount = 0, prodCount = 0;

    juce::TextButton playButton { "Play" }, stopButton { "Stop" },
                     acceptButton { "Accept Proposal" }, undoButton { "Undo" },
                     redoButton { "Redo" }, rejectButton { "Reject Proposal" },
                     prodInsertButton { "Producer: insert" }, prodMoveButton { "Producer: move" };
    juce::TextEditor undoView, trackView;
    juce::Label statusLabel, logLabel;
};

//==============================================================================
class SpikeGuiApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Duet Undo Vocabulary PROTOTYPE"; }
    const juce::String getApplicationVersion() override { return "0.0.1"; }

    void initialise (const juce::String&) override
    {
        window = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override { window = nullptr; }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colours::black, allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (SpikeGuiApp)
