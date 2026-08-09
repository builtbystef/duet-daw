// PROTOTYPE — undo-vocabulary spike for roadmap node skb4tp. Disposable; never ship.
//
// The only translation unit that sees tracktion::/juce:: — the seam under test.

#include "DuetEdit.h"

#include <tracktion_engine/tracktion_engine.h>

#include <algorithm>
#include <iostream>
#include <sstream>

namespace te = tracktion;

namespace duet
{

//==============================================================================
struct ProjectEditor::Impl
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // makes this thread the message thread
    te::Engine engine { "DuetUndoVocabPrototype" };
    std::unique_ptr<te::Edit> edit;
    juce::File workDir, editFile;
    juce::Array<juce::File> waveFiles;
    te::MidiClip::Ptr demoMidiClip;

    juce::UndoManager& um() { return edit->getUndoManager(); }

    te::AudioTrack* trackFor (TrackRef r) const
    {
        return dynamic_cast<te::AudioTrack*> (
            te::findTrackForID (*edit, te::EditItemID::fromRawID ((juce::uint64) r.raw)));
    }

    te::Clip* clipFor (ClipRef r) const
    {
        return te::findClipForID (*edit, te::EditItemID::fromRawID ((juce::uint64) r.raw));
    }

    void generateToneFiles();
    void buildDemoProject();
};

//==============================================================================
void ProjectEditor::Impl::generateToneFiles()
{
    struct Spec { const char* name; int kind; double freq; };
    const Spec specs[] = { { "bass.wav", 0, 110.0 }, { "chords.wav", 1, 220.0 },
                           { "lead.wav", 2, 440.0 }, { "drums.wav", 3, 0.0 } };

    juce::WavAudioFormat wav;
    const double sr = 44100.0;
    const int    len = (int) (sr * 8.0);

    for (auto& s : specs)
    {
        auto f = workDir.getChildFile (s.name);
        waveFiles.add (f);
        if (f.existsAsFile())
            continue;

        juce::AudioBuffer<float> buf (2, len);
        buf.clear();
        juce::Random r (7);
        double phase = 0.0;

        for (int i = 0; i < len; ++i)
        {
            const double t = i / sr;
            float v = 0.0f;
            switch (s.kind)
            {
                case 0:
                    v = (float) (std::sin (2.0 * juce::MathConstants<double>::pi * s.freq * t)
                                 * (0.5 + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * t)));
                    break;
                case 1:
                    v = (float) ((std::sin (2.0 * juce::MathConstants<double>::pi * s.freq * t)
                                  + 0.7 * std::sin (2.0 * juce::MathConstants<double>::pi * s.freq * 1.25 * t)
                                  + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * s.freq * 1.5 * t)) / 3.0
                                 * (std::fmod (t, 0.5) < 0.4 ? 1.0 : 0.0));
                    break;
                case 2:
                {
                    static const double seq[] = { 440.0, 523.25, 659.25, 587.33, 493.88, 659.25, 523.25, 587.33 };
                    const double note = seq[(int) (t * 2.0) % 8];
                    phase += 2.0 * juce::MathConstants<double>::pi * note / sr;
                    const double noteT = std::fmod (t, 0.5);
                    const double env = std::min (1.0, noteT / 0.02) * std::exp (-noteT * 4.0);
                    v = (float) (std::sin (phase) * env * 0.8);
                    break;
                }
                case 3:
                {
                    const double ph = std::fmod (t, 0.5);
                    v = (r.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-ph * 18.0);
                    break;
                }
            }
            v *= 0.4f;
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }

        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (buf, 0, len);
    }
}

void ProjectEditor::Impl::buildDemoProject()
{
    editFile = workDir.getChildFile ("undo-spike.tracktionedit");
    edit = te::createEmptyEdit (engine, editFile);
    edit->ensureNumberOfAudioTracks (5);

    auto tracks = te::getAudioTracks (*edit);
    const float pans[] = { 0.0f, -0.4f, 0.4f, 0.0f };

    for (int i = 0; i < 4; ++i)
    {
        auto* track = tracks[i];
        track->setName (juce::File (waveFiles[i]).getFileNameWithoutExtension());
        te::ClipPosition pos { { te::TimePosition(), te::TimePosition::fromSeconds (8.0) } };
        if (auto clip = track->insertWaveClip (track->getName(), waveFiles[i], pos, false))
            // ddp1qt finding: the default relative source path resolves against the edit's
            // temp subdir, yielding "../x.wav" → silence. Pin absolute, centrally.
            clip->getSourceFileReference().setToFile (waveFiles[i], te::SourceFileReference::PathStyle::alwaysAbsolute, false);
        if (auto vp = track->getVolumePlugin())
        {
            vp->setVolumeDb (i == 3 ? -8.0f : -10.0f);
            vp->setPan (pans[i]);
        }
    }

    if (auto* midiTrack = tracks[4])
    {
        midiTrack->setName ("arp (4OSC)");
        if (auto synth = edit->getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}))
            midiTrack->pluginList.insertPlugin (synth, 0, nullptr);

        if (auto midiClip = midiTrack->insertMIDIClip ({ te::TimePosition(), te::TimePosition::fromSeconds (8.0) }, nullptr))
        {
            demoMidiClip = midiClip;
            const int scale[] = { 57, 60, 64, 67, 69, 67, 64, 60 };
            auto& seq = midiClip->getSequence();
            for (int n = 0; n < 32; ++n)
                seq.addNote (scale[n % 8], te::BeatPosition::fromBeats (n * 0.5),
                             te::BeatDuration::fromBeats (0.4), 100, 0, nullptr);
        }
    }

    auto& tc = edit->getTransport();
    tc.setLoopRange ({ te::TimePosition(), te::TimePosition::fromSeconds (8.0) });
    tc.looping = true;
    tc.ensureContextAllocated();

    // The demo project setup above is not part of the test; start history clean.
    um().clearUndoHistory();
}

//==============================================================================
ProjectEditor::ProjectEditor() : impl (std::make_unique<Impl>())
{
    impl->workDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("duet-undo-spike-PROTOTYPE-wipe-me");
    impl->workDir.createDirectory();
    impl->generateToneFiles();
    impl->buildDemoProject();
}

ProjectEditor::~ProjectEditor()
{
    if (impl->edit != nullptr)
        impl->edit->getTransport().stop (false, true);
}

//==============================================================================
// The vocabulary. Rule under test: every op passes the Edit's UndoManager —
// never nullptr — and never begins a transaction itself.

ClipRef ProjectEditor::insertAudioClip (TrackRef tr, const std::string& name, const std::string& absFile,
                                        double startSec, double lengthSec)
{
    auto* track = impl->trackFor (tr);
    if (track == nullptr)
        return {};

    te::ClipPosition pos { { te::TimePosition::fromSeconds (startSec),
                             te::TimePosition::fromSeconds (startSec + lengthSec) } };
    juce::File f { juce::String (absFile) };
    if (auto clip = track->insertWaveClip (juce::String (name), f, pos, false))
    {
        clip->getSourceFileReference().setToFile (f, te::SourceFileReference::PathStyle::alwaysAbsolute, false);
        return { (std::uint64_t) clip->itemID.getRawID() };
    }
    return {};
}

void ProjectEditor::moveClip (ClipRef cr, double newStartSec)
{
    if (auto* clip = impl->clipFor (cr))
        clip->setStart (te::TimePosition::fromSeconds (newStartSec), false, true);
}

void ProjectEditor::trimClip (ClipRef cr, double newLengthSec)
{
    if (auto* clip = impl->clipFor (cr))
        clip->setLength (te::TimeDuration::fromSeconds (newLengthSec), true);
}

void ProjectEditor::addMidiNote (ClipRef cr, int pitch, double startBeats, double lengthBeats, int velocity)
{
    if (auto* clip = dynamic_cast<te::MidiClip*> (impl->clipFor (cr)))
        clip->getSequence().addNote (pitch, te::BeatPosition::fromBeats (startBeats),
                                     te::BeatDuration::fromBeats (lengthBeats), velocity, 0,
                                     &impl->um());
}

void ProjectEditor::setAutomationPoint (TrackRef tr, double timeSec, float normValue)
{
    if (auto* track = impl->trackFor (tr))
        if (auto vp = track->getVolumePlugin())
            vp->volParam->getCurve().addPoint (te::TimePosition::fromSeconds (timeSec),
                                               normValue, 0.0f, &impl->um());
}

//==============================================================================
// Actions: the ONLY place a transaction begins.

void ProjectEditor::performAction (const std::string& name, const std::function<void()>& ops)
{
    impl->um().beginNewTransaction (juce::String (name));
    ops();
    // No closing call: the next action begins its own transaction. Actions run
    // synchronously on the message thread, so the engine's 350 ms
    // UndoTransactionTimer can never fire mid-action and split one.
}

void ProjectEditor::acceptProposal (const Proposal& p)
{
    performAction ("Proposal: " + p.title, [this, &p]
    {
        ClipRef newest;
        for (auto op : p.ops)
        {
            if (op.clip.raw == Proposal::newestClip)
                op.clip = newest;

            switch (op.kind)
            {
                case ProposalOp::Kind::insertAudioClip:
                    newest = insertAudioClip (op.track, op.name, op.file, op.a, op.b);
                    break;
                case ProposalOp::Kind::moveClip:            moveClip (op.clip, op.a); break;
                case ProposalOp::Kind::trimClip:            trimClip (op.clip, op.a); break;
                case ProposalOp::Kind::addMidiNote:         addMidiNote (op.clip, op.pitch, op.a, op.b, op.velocity); break;
                case ProposalOp::Kind::setAutomationPoint:  setAutomationPoint (op.track, op.a, (float) op.b); break;
            }
        }
    });
}

std::string ProposalOp::describe() const
{
    std::ostringstream os;
    switch (kind)
    {
        case Kind::insertAudioClip:    os << "insert clip '" << name << "' @" << a << "s len " << b << "s"; break;
        case Kind::moveClip:           os << "move clip → " << a << "s"; break;
        case Kind::trimClip:           os << "trim clip → " << a << "s"; break;
        case Kind::addMidiNote:        os << "add MIDI note pitch " << pitch << " @beat " << a; break;
        case Kind::setAutomationPoint: os << "set volume automation @" << a << "s = " << b; break;
    }
    return os.str();
}

//==============================================================================
// Undo surface.

bool ProjectEditor::undo()
{
    if (! impl->um().canUndo())
        return false;
    impl->edit->undo();
    return true;
}

bool ProjectEditor::redo()
{
    if (! impl->um().canRedo())
        return false;
    impl->edit->redo();
    return true;
}

static std::vector<std::string> toVec (const juce::StringArray& a)
{
    std::vector<std::string> v;
    for (auto& s : a)
        v.push_back (s.toStdString());
    return v;
}

std::vector<std::string> ProjectEditor::undoNames() const { return toVec (impl->um().getUndoDescriptions()); }
std::vector<std::string> ProjectEditor::redoNames() const { return toVec (impl->um().getRedoDescriptions()); }

//==============================================================================
// Queries.

std::vector<TrackInfo> ProjectEditor::tracks() const
{
    std::vector<TrackInfo> out;
    for (auto* t : te::getAudioTracks (*impl->edit))
    {
        TrackInfo ti;
        ti.track = { (std::uint64_t) t->itemID.getRawID() };
        ti.name = t->getName().toStdString();
        if (auto vp = t->getVolumePlugin())
            ti.automationPoints = vp->volParam->getCurve().getNumPoints();
        for (auto* c : t->getClips())
        {
            ClipInfo ci;
            ci.clip = { (std::uint64_t) c->itemID.getRawID() };
            ci.name = c->getName().toStdString();
            ci.startSec = c->getPosition().getStart().inSeconds();
            ci.lengthSec = c->getPosition().getLength().inSeconds();
            if (auto* mc = dynamic_cast<te::MidiClip*> (c))
            {
                ci.isMidi = true;
                ci.midiNotes = mc->getSequence().getNotes().size();
            }
            ti.clips.push_back (ci);
        }
        out.push_back (std::move (ti));
    }
    return out;
}

TrackRef ProjectEditor::trackByIndex (int i) const
{
    auto ts = te::getAudioTracks (*impl->edit);
    if (i < 0 || i >= ts.size())
        return {};
    return { (std::uint64_t) ts[i]->itemID.getRawID() };
}

ClipRef ProjectEditor::clipByName (const std::string& n) const
{
    for (auto* t : te::getAudioTracks (*impl->edit))
        for (auto* c : t->getClips())
            if (c->getName() == juce::String (n))
                return { (std::uint64_t) c->itemID.getRawID() };
    return {};
}

ClipRef ProjectEditor::midiClipRef() const
{
    if (impl->demoMidiClip != nullptr)
        return { (std::uint64_t) impl->demoMidiClip->itemID.getRawID() };
    return {};
}

std::string ProjectEditor::audioFile (int i) const
{
    return impl->waveFiles[juce::jlimit (0, impl->waveFiles.size() - 1, i)].getFullPathName().toStdString();
}

// FINDING: undo/redo round-trips preserve the semantic state but can permute
// ValueTree PROPERTY ORDER (juce::ValueTree undo re-appends restored
// properties). Any state comparison must therefore be order-insensitive.
static void canonicalDump (const juce::ValueTree& v, std::string& out, int depth)
{
    out.append ((size_t) depth * 2, ' ');
    out += v.getType().toString().toStdString();
    out += '\n';

    std::vector<std::string> props;
    for (int i = 0; i < v.getNumProperties(); ++i)
    {
        auto name = v.getPropertyName (i);
        props.push_back (name.toString().toStdString() + "=" + v.getProperty (name).toString().toStdString());
    }
    std::sort (props.begin(), props.end());
    for (auto& p : props)
    {
        out.append ((size_t) depth * 2 + 2, ' ');
        out += p;
        out += '\n';
    }

    for (const auto& child : v)
        canonicalDump (child, out, depth + 1);
}

std::string ProjectEditor::stateXml() const
{
    // FINDING: no flushState() here. Edit::flushState() writes parameter blobs
    // with the Edit's UndoManager (AutomatableEditItem::saveChangedParametersToState,
    // tracktion_AutomatableEditItem.cpp:321), so flushing pollutes the undo
    // history with unnamed transactions and clears the redo stack after an undo.
    // Duet's save path must account for this; a state digest must not flush.
    // The vocabulary's own ops all write through to the tree immediately.
    auto tree = impl->edit->state.createCopy();
    // Strip what changes without an edit having happened.
    tree.removeChild (tree.getChildWithName (te::IDs::TRANSPORT), nullptr);
    tree.removeProperty ("lastSignificantChange", nullptr);
    tree.removeProperty ("modifiedBy", nullptr);

    std::string out;
    canonicalDump (tree, out, 0);
    return out;
}

std::string ProjectEditor::stateDigest() const
{
    auto x = stateXml();
    std::ostringstream os;
    os << std::hex << std::hash<std::string>{} (x) << "/" << std::dec << x.size() << "B";
    return os.str();
}

//==============================================================================
// Transport.

void ProjectEditor::play()
{
    auto& tc = impl->edit->getTransport();
    tc.setPosition (te::TimePosition());
    tc.play (false);
}

void ProjectEditor::stop()          { impl->edit->getTransport().stop (false, false); }
bool ProjectEditor::isPlaying() const { return impl->edit->getTransport().isPlaying(); }
double ProjectEditor::positionSec() const { return impl->edit->getTransport().getPosition().inSeconds(); }

int ProjectEditor::xrunCount() const
{
    if (auto* dev = impl->engine.getDeviceManager().deviceManager.getCurrentAudioDevice())
        return dev->getXRunCount();
    return -1;
}

std::string ProjectEditor::deviceDescription() const
{
    if (auto* d = impl->engine.getDeviceManager().deviceManager.getCurrentAudioDevice())
        return (d->getTypeName() + ": " + d->getName()
                + juce::String::formatted (" %.0f Hz, %d smp", d->getCurrentSampleRate(),
                                           d->getCurrentBufferSizeSamples())).toStdString();
    return "no audio device";
}

void ProjectEditor::pumpMessages (int ms) const
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
}

} // namespace duet
