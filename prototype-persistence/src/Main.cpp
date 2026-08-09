// PROTOTYPE — persistence spike for roadmap node rquzdc. Disposable; never ship.
//
// Question: where does Duet's own project data live relative to Tracktion's
// Edit file, and how does it version?
//
// Experiments (each prints full state; PASS/FAIL where a fact is asserted):
//   E1  EditItemID stability across Duet's save path (flushState + direct
//       XML write) and a full engine reload — and what the engine does to a
//       file with a duplicated itemID (the reassignment hazard, forced).
//   E2  Layering in practice: a DUET child tree under edit.state vs a
//       sidecar file. Undo interplay: Duet writes with nullptr UM survive
//       engine undo; Duet writes through the UM revert with it.
//   E3  The flushState undo-pollution hazard from skb4tp, reproduced, then
//       candidate save strategies measured against it.
//   E4  Versioning: what the engine stamps on the EDIT node, a Duet schema
//       stamp, a v1→v2 migration, and the too-new refusal.

#include <tracktion_engine/tracktion_engine.h>

#include <iostream>
#include <map>
#include <string>

namespace te = tracktion;

static int passed = 0, failed = 0;

static void check (bool ok, const std::string& what)
{
    std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << what << std::endl;
    (ok ? passed : failed)++;
}

static void banner (const std::string& s)
{
    std::cout << "\n=== " << s << " ===" << std::endl;
}

static void dumpUndoStacks (juce::UndoManager& um, const std::string& tag)
{
    std::cout << "  " << tag << " undo:";
    for (auto& d : um.getUndoDescriptions()) std::cout << " [" << (d.isEmpty() ? "<unnamed>" : d) << "]";
    if (um.getUndoDescriptions().isEmpty()) std::cout << " (empty)";
    std::cout << "  redo:";
    for (auto& d : um.getRedoDescriptions()) std::cout << " [" << (d.isEmpty() ? "<unnamed>" : d) << "]";
    if (um.getRedoDescriptions().isEmpty()) std::cout << " (empty)";
    std::cout << std::endl;
}

//==============================================================================
struct Fixture
{
    juce::File workDir;
    te::Engine& engine;
    std::unique_ptr<te::Edit> edit;
    juce::File editFile, waveFile;

    Fixture (te::Engine& e, juce::File dir, const juce::String& editName)
        : workDir (dir), engine (e)
    {
        workDir.createDirectory();
        generateWave();
        editFile = workDir.getChildFile (editName);
        edit = te::createEmptyEdit (engine, editFile);
        buildDemoProject();
    }

    void generateWave()
    {
        waveFile = workDir.getChildFile ("tone.wav");
        if (waveFile.existsAsFile())
            return;
        juce::WavAudioFormat wav;
        const double sr = 44100.0;
        const int len = (int) sr * 2;
        juce::AudioBuffer<float> buf (2, len);
        for (int i = 0; i < len; ++i)
        {
            auto v = (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / sr) * 0.3f;
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (new juce::FileOutputStream (waveFile), sr, 2, 16, {}, 0));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (buf, 0, len);
    }

    void buildDemoProject()
    {
        edit->ensureNumberOfAudioTracks (3);
        auto tracks = te::getAudioTracks (*edit);

        tracks[0]->setName ("bass");
        tracks[1]->setName ("keys");
        tracks[2]->setName ("arp");

        for (int i = 0; i < 2; ++i)
        {
            te::ClipPosition pos { { te::TimePosition(), te::TimePosition::fromSeconds (2.0) } };
            if (auto clip = tracks[i]->insertWaveClip (tracks[i]->getName() + "-clip", waveFile, pos, false))
                // ddp1qt finding: default relative source path resolves against the edit's
                // temp subdir, yielding "../x.wav" → silence. Pin absolute.
                clip->getSourceFileReference().setToFile (waveFile, te::SourceFileReference::PathStyle::alwaysAbsolute, false);
        }

        if (auto midiClip = tracks[2]->insertMIDIClip ({ te::TimePosition(), te::TimePosition::fromSeconds (2.0) }, nullptr))
        {
            auto& seq = midiClip->getSequence();
            const int scale[] = { 57, 60, 64, 67 };
            for (int n = 0; n < 8; ++n)
                seq.addNote (scale[n % 4], te::BeatPosition::fromBeats (n * 0.5),
                             te::BeatDuration::fromBeats (0.5), 96, 0, nullptr);
        }
    }

    // Duet's save path (per ddp1qt: EditFileOperations::save crashes for
    // project-less edits; flushState + direct XML write is the proven path).
    bool duetSave (const juce::File& to)
    {
        edit->flushState();
        if (auto xml = edit->state.createXml())
            return xml->writeTo (to);
        return false;
    }

    // Name → itemID for every track and clip, for stability comparison.
    std::map<std::string, juce::String> idMap() const
    {
        std::map<std::string, juce::String> m;
        for (auto* t : te::getAudioTracks (*edit))
        {
            m["track:" + t->getName().toStdString()] = t->itemID.toString();
            for (auto* c : t->getClips())
                m["clip:" + c->getName().toStdString()] = c->itemID.toString();
        }
        return m;
    }
};

static std::map<std::string, juce::String> idMapOf (te::Edit& edit)
{
    std::map<std::string, juce::String> m;
    for (auto* t : te::getAudioTracks (edit))
    {
        m["track:" + t->getName().toStdString()] = t->itemID.toString();
        for (auto* c : t->getClips())
            m["clip:" + c->getName().toStdString()] = c->itemID.toString();
    }
    return m;
}

static void printIdMap (const std::map<std::string, juce::String>& m, const std::string& tag)
{
    std::cout << "  " << tag << ":" << std::endl;
    for (auto& [k, v] : m)
        std::cout << "    " << k << " = " << v << std::endl;
}

//==============================================================================
static void e1_itemIdStability (te::Engine& engine, const juce::File& base)
{
    banner ("E1: EditItemID stability across duetSave + engine reload");

    Fixture fx (engine, base.getChildFile ("e1"), "e1.tracktionedit");
    auto before = fx.idMap();
    printIdMap (before, "IDs before save");

    check (fx.duetSave (fx.editFile), "duetSave wrote the edit file");

    auto reloaded = te::loadEditFromFile (engine, fx.editFile);
    auto after = idMapOf (*reloaded);
    printIdMap (after, "IDs after full engine reload");
    check (before == after, "every track and clip itemID survived save + reload unchanged");

    // Second reload of the same file — IDs must still match (no per-load reassignment).
    auto reloaded2 = te::loadEditFromFile (engine, fx.editFile);
    check (after == idMapOf (*reloaded2), "second reload of the same file: IDs identical again");

    banner ("E1b: forced duplicate itemID — when DOES the engine reassign?");
    // Duplicate the 'bass' TRACK node verbatim (same itemID) and load that file.
    auto xml = juce::parseXML (fx.editFile);
    auto tree = juce::ValueTree::fromXml (*xml);
    juce::ValueTree dup;
    for (int i = 0; i < tree.getNumChildren(); ++i)
        if (tree.getChild (i).hasType (te::IDs::TRACK)
              && tree.getChild (i)[te::IDs::name].toString() == "bass")
            dup = tree.getChild (i).createCopy();
    jassert (dup.isValid());
    dup.setProperty (te::IDs::name, "bass-dup", nullptr);   // same itemID, different name
    tree.addChild (dup, -1, nullptr);

    auto dupFile = fx.workDir.getChildFile ("e1-dup.tracktionedit");
    tree.createXml()->writeTo (dupFile);

    auto dupEdit = te::loadEditFromFile (engine, dupFile);
    auto dupIds = idMapOf (*dupEdit);
    printIdMap (dupIds, "IDs after loading the file with a duplicated itemID");
    const auto bassId = before["track:bass"];
    const bool origKept = dupIds["track:bass"] == bassId;
    const bool dupKept  = dupIds["track:bass-dup"] == bassId;
    std::cout << "  original 'bass' kept its ID: " << (origKept ? "yes" : "NO — reassigned") << std::endl;
    std::cout << "  duplicate 'bass-dup' kept the ID: " << (dupKept ? "yes" : "NO — reassigned") << std::endl;
    check (! (origKept && dupKept), "the engine did not tolerate two items with one ID");
}

//==============================================================================
static void e2_layering (te::Engine& engine, const juce::File& base)
{
    banner ("E2: DUET child tree under edit.state — survives save + reload, keys resolve");

    Fixture fx (engine, base.getChildFile ("e2"), "e2.tracktionedit");
    auto& um = fx.edit->getUndoManager();

    const juce::Identifier DUET ("DUET");
    const juce::Identifier ANNOTATION ("ANNOTATION");

    // Duet's own facts, written with nullptr UM (not part of producer undo).
    auto duetTree = fx.edit->state.getOrCreateChildWithName (DUET, nullptr);
    duetTree.setProperty ("duetSchemaVersion", 1, nullptr);
    duetTree.setProperty ("conversationSummary", "producer asked for a darker bass", nullptr);

    auto bass = te::getAudioTracks (*fx.edit)[0];
    juce::ValueTree ann (ANNOTATION);
    ann.setProperty ("target", bass->itemID.toString(), nullptr);
    ann.setProperty ("text", "Collaborator: this is the anchor of the low end", nullptr);
    duetTree.addChild (ann, -1, nullptr);

    check (fx.duetSave (fx.editFile), "saved with DUET child tree in place");

    auto reloaded = te::loadEditFromFile (engine, fx.editFile);
    auto reDuet = reloaded->state.getChildWithName (DUET);
    check (reDuet.isValid(), "DUET child tree survived the engine reload");
    check (reDuet["conversationSummary"].toString() == "producer asked for a darker bass",
           "DUET property survived verbatim");

    auto reAnn = reDuet.getChildWithName (ANNOTATION);
    auto target = te::EditItemID::fromString (reAnn["target"].toString());
    auto* resolved = te::findTrackForID (*reloaded, target);
    check (resolved != nullptr && resolved->getName() == "bass",
           "annotation's itemID key resolves to the same track after reload");

    banner ("E2b: undo interplay — nullptr writes survive engine undo, UM writes revert");

    // A producer action through the UM...
    um.beginNewTransaction ("Rename track");
    bass->setName ("bass (renamed)");
    // ...and a Duet fact written outside the UM in the same breath.
    duetTree.setProperty ("conversationSummary", "renamed during review", nullptr);

    check (um.undo(), "engine undo succeeded");
    check (bass->getName() == "bass", "producer rename reverted by undo");
    check (duetTree["conversationSummary"].toString() == "renamed during review",
           "nullptr-written Duet fact UNTOUCHED by engine undo");

    // The alternative: a Duet fact written THROUGH the UM rides the undo stack.
    um.beginNewTransaction ("Set annotation");
    duetTree.setProperty ("mood", "brooding", &um);
    check (um.undo(), "undo of the UM-written Duet fact succeeded");
    check (! duetTree.hasProperty ("mood"), "UM-written Duet fact reverted with undo");

    banner ("E2c: sidecar file variant — clean separation, manual sync");

    juce::ValueTree sidecar ("DUET_SIDECAR");
    sidecar.setProperty ("duetSchemaVersion", 1, nullptr);
    juce::ValueTree sAnn (ANNOTATION);
    sAnn.setProperty ("target", bass->itemID.toString(), nullptr);
    sAnn.setProperty ("text", "sidecar annotation", nullptr);
    sidecar.addChild (sAnn, -1, nullptr);

    auto sidecarFile = fx.workDir.getChildFile ("e2.duet");
    check (sidecar.createXml()->writeTo (sidecarFile), "sidecar file written");

    auto reloadedSidecar = juce::ValueTree::fromXml (*juce::parseXML (sidecarFile));
    auto sTarget = te::EditItemID::fromString (reloadedSidecar.getChild (0)["target"].toString());
    check (te::findTrackForID (*reloaded, sTarget) != nullptr,
           "sidecar's itemID key resolves against the separately-reloaded edit");
    std::cout << "  (sidecar works mechanically; the cost is two files to keep in sync,\n"
                 "   no free undo coupling, and a torn state if one write fails)" << std::endl;
}

//==============================================================================
static void e3_savePollution (te::Engine& engine, const juce::File& base)
{
    banner ("E3: the flushState undo-pollution hazard (skb4tp), reproduced");

    Fixture fx (engine, base.getChildFile ("e3"), "e3.tracktionedit");
    auto& um = fx.edit->getUndoManager();
    um.clearUndoHistory();

    auto bass = te::getAudioTracks (*fx.edit)[0];
    auto vol = bass->getVolumePlugin();

    // A parameter change → a deferred, undo-tracked blob write at flush time.
    vol->setVolumeDb (-6.0f);

    // A producer action, then undo → redo stack is populated.
    um.beginNewTransaction ("Rename keys");
    te::getAudioTracks (*fx.edit)[1]->setName ("keys (renamed)");
    check (um.undo(), "undo done — redo stack now holds [Rename keys]");
    dumpUndoStacks (um, "before flushState");
    const bool redoBefore = um.canRedo();

    fx.edit->flushState();
    dumpUndoStacks (um, "after flushState ");
    const bool redoAfter = um.canRedo();
    std::cout << "  redo available: before=" << redoBefore << " after=" << redoAfter << std::endl;
    check (redoBefore, "redo existed before the flush");
    if (redoBefore && ! redoAfter)
        std::cout << "  [REPRODUCED] flushState after a param change KILLED the redo stack" << std::endl;
    else
        std::cout << "  [NOT REPRODUCED] redo survived flushState this time" << std::endl;

    banner ("E3b: candidate — save WITHOUT flushState after a param change");

    Fixture fx2 (engine, base.getChildFile ("e3b"), "e3b.tracktionedit");
    auto vol2 = te::getAudioTracks (*fx2.edit)[0]->getVolumePlugin();
    vol2->setVolumeDb (-3.0f);

    // Save with no flush: is the -3 dB in the file?
    auto noFlushFile = fx2.workDir.getChildFile ("no-flush.tracktionedit");
    fx2.edit->state.createXml()->writeTo (noFlushFile);
    auto reloadedNoFlush = te::loadEditFromFile (engine, noFlushFile);
    auto reVol = te::getAudioTracks (*reloadedNoFlush)[0]->getVolumePlugin();
    const bool kept = std::abs (reVol->getVolumeDb() - (-3.0f)) < 0.01f;
    std::cout << "  volume after reload of a NO-FLUSH save: " << reVol->getVolumeDb()
              << " dB (wrote -3.0) → the no-flush save "
              << (kept ? "KEPT the parameter change — flush not needed for this param"
                       : "LOST the parameter change — flush is mandatory before saving params")
              << std::endl;

    banner ("E3c: candidate — flush at the transaction boundary, before any undo");

    Fixture fx3 (engine, base.getChildFile ("e3c"), "e3c.tracktionedit");
    auto& um3 = fx3.edit->getUndoManager();
    um3.clearUndoHistory();
    auto vol3 = te::getAudioTracks (*fx3.edit)[0]->getVolumePlugin();

    um3.beginNewTransaction ("Set bass volume");
    vol3->setVolumeDb (-6.0f);
    fx3.edit->flushState();               // flush INSIDE/at the end of the action, redo stack empty
    dumpUndoStacks (um3, "after boundary flush");

    um3.beginNewTransaction ("Rename keys");
    te::getAudioTracks (*fx3.edit)[1]->setName ("keys (renamed)");
    check (um3.undo(), "undo the rename");
    const bool redo3a = um3.canRedo();

    // Save again with NO param change since the last flush — is redo preserved?
    fx3.edit->flushState();
    dumpUndoStacks (um3, "after clean-flush   ");
    const bool redo3b = um3.canRedo();
    std::cout << "  redo before second flush=" << redo3a << " after=" << redo3b << std::endl;
    check (redo3a && redo3b,
           "a flush with NO pending param changes leaves the redo stack alone");

    banner ("E3d: the REAL trigger — automation drives a param away from its explicit value");
    // Edit::flushState() flushes EVERY plugin (tracktion_Edit.cpp:1176), and
    // AutomatableEditItem::saveChangedParametersToState (line 307) writes the
    // IDs::parameters blob WITH the UM whenever currentValue != explicitValue —
    // which is exactly the automation-following state.

    Fixture fx4 (engine, base.getChildFile ("e3d"), "e3d.tracktionedit");
    auto& um4 = fx4.edit->getUndoManager();
    um4.clearUndoHistory();
    auto vol4 = te::getAudioTracks (*fx4.edit)[0]->getVolumePlugin();
    auto volParam = vol4->getAutomatableParameterByID ("volume");

    volParam->getCurve().addPoint (te::TimePosition(), 0.2f, 0.0f, nullptr);
    volParam->getCurve().addPoint (te::TimePosition::fromSeconds (2.0), 0.9f, 0.0f, nullptr);
    volParam->updateToFollowCurve (te::TimePosition::fromSeconds (1.0));
    std::cout << "  param current=" << volParam->getCurrentValue()
              << " explicit=" << volParam->getCurrentExplicitValue()
              << (volParam->getCurrentValue() != volParam->getCurrentExplicitValue()
                      ? "  → DIVERGED (automation-following)" : "  → equal") << std::endl;

    um4.beginNewTransaction ("Rename keys");
    te::getAudioTracks (*fx4.edit)[1]->setName ("keys (renamed)");
    check (um4.undo(), "undo the rename — redo populated");
    dumpUndoStacks (um4, "before flushState");
    const bool redo4a = um4.canRedo();

    fx4.edit->flushState();
    dumpUndoStacks (um4, "after flushState ");
    const bool redo4b = um4.canRedo();
    std::cout << "  redo available: before=" << redo4a << " after=" << redo4b << std::endl;
    if (redo4a && ! redo4b)
        std::cout << "  [REPRODUCED] flushState during automation-following KILLED the redo stack" << std::endl;
    check (redo4a, "redo existed before the flush");

    banner ("E3e: candidate — SNAPSHOT save: copy the tree, apply param blobs to the copy, no UM");

    Fixture fx5 (engine, base.getChildFile ("e3e"), "e3e.tracktionedit");
    auto& um5 = fx5.edit->getUndoManager();
    um5.clearUndoHistory();
    auto vol5 = te::getAudioTracks (*fx5.edit)[0]->getVolumePlugin();
    auto volParam5 = vol5->getAutomatableParameterByID ("volume");

    volParam5->getCurve().addPoint (te::TimePosition(), 0.2f, 0.0f, nullptr);
    volParam5->getCurve().addPoint (te::TimePosition::fromSeconds (2.0), 0.9f, 0.0f, nullptr);
    volParam5->updateToFollowCurve (te::TimePosition::fromSeconds (1.0));
    const float explicitBefore = volParam5->getCurrentExplicitValue();

    um5.beginNewTransaction ("Rename keys");
    te::getAudioTracks (*fx5.edit)[1]->setName ("keys (renamed)");
    check (um5.undo(), "undo the rename — redo populated");

    // The snapshot save: no flushState, no UM. Copy the tree, then write the
    // IDs::parameters blob (same format as saveChangedParametersToState,
    // tracktion_AutomatableEditItem.cpp:307) onto the COPY, located by itemID.
    auto snapshot = fx5.edit->state.createCopy();

    for (auto p : te::getAllPlugins (*fx5.edit, true))
    {
        juce::MemoryOutputStream stream;
        for (auto ap : p->getAutomatableParameters())
            if (ap->getCurrentValue() != ap->getCurrentExplicitValue())
            {
                stream.writeString (ap->paramID);
                stream.writeFloat (ap->getCurrentExplicitValue());
            }
        stream.flush();
        if (stream.getDataSize() > 0)
        {
            // Locate the plugin's node in the copy by its itemID.
            std::function<juce::ValueTree (juce::ValueTree)> findById =
                [&] (juce::ValueTree t) -> juce::ValueTree
                {
                    if (t[te::IDs::id].toString() == p->itemID.toString() && t.hasType (te::IDs::PLUGIN))
                        return t;
                    for (auto child : t)
                        if (auto r = findById (child); r.isValid())
                            return r;
                    return {};
                };
            if (auto node = findById (snapshot); node.isValid())
                node.setProperty (te::IDs::parameters, stream.getMemoryBlock(), nullptr);
        }
    }

    auto snapFile = fx5.workDir.getChildFile ("snapshot.tracktionedit");
    check (snapshot.createXml()->writeTo (snapFile), "snapshot save written");

    check (um5.canRedo(), "redo stack SURVIVED the snapshot save");
    dumpUndoStacks (um5, "after snapshot save");

    auto reSnap = te::loadEditFromFile (engine, snapFile);
    auto reParam = te::getAudioTracks (*reSnap)[0]->getVolumePlugin()->getAutomatableParameterByID ("volume");
    std::cout << "  explicit value: saved-from=" << explicitBefore
              << " reloaded=" << reParam->getCurrentExplicitValue() << std::endl;
    check (std::abs (reParam->getCurrentExplicitValue() - explicitBefore) < 0.0001f,
           "reloaded edit restored the explicit param value from the snapshot blob");
}

//==============================================================================
static void e4_versioning (te::Engine& engine, const juce::File& base)
{
    banner ("E4: what the engine stamps on the EDIT node");

    Fixture fx (engine, base.getChildFile ("e4"), "e4.tracktionedit");
    fx.duetSave (fx.editFile);

    auto xml = juce::parseXML (fx.editFile);
    std::cout << "  EDIT node attributes:" << std::endl;
    for (int i = 0; i < xml->getNumAttributes(); ++i)
        std::cout << "    " << xml->getAttributeName (i) << " = " << xml->getAttributeValue (i) << std::endl;

    banner ("E4b: Duet schema stamp + v1→v2 migration + too-new refusal");

    const juce::Identifier DUET ("DUET");
    constexpr int duetCurrentSchema = 2;

    // Simulate a v1 file: v1 used 'conversationSummary'; v2 renames it to 'sessionNotes'.
    auto duetTree = fx.edit->state.getOrCreateChildWithName (DUET, nullptr);
    duetTree.setProperty ("duetSchemaVersion", 1, nullptr);
    duetTree.setProperty ("conversationSummary", "old-style summary", nullptr);
    fx.duetSave (fx.editFile);

    auto reloaded = te::loadEditFromFile (engine, fx.editFile);
    auto tree = reloaded->state.getChildWithName (DUET);
    int fileSchema = tree["duetSchemaVersion"];
    std::cout << "  file schema=" << fileSchema << " app schema=" << duetCurrentSchema << std::endl;

    if (fileSchema < duetCurrentSchema)
    {
        // Migrations run oldest-first, one step per version, before anything reads the tree.
        if (fileSchema < 2)
        {
            tree.setProperty ("sessionNotes", tree["conversationSummary"], nullptr);
            tree.removeProperty ("conversationSummary", nullptr);
        }
        tree.setProperty ("duetSchemaVersion", duetCurrentSchema, nullptr);
    }
    check (tree["sessionNotes"].toString() == "old-style summary"
             && ! tree.hasProperty ("conversationSummary")
             && (int) tree["duetSchemaVersion"] == 2,
           "v1 file migrated to v2 in memory (renamed property, stamped)");

    // Too new: a v99 file in a v2 app.
    tree.setProperty ("duetSchemaVersion", 99, nullptr);
    const bool tooNew = (int) tree["duetSchemaVersion"] > duetCurrentSchema;
    check (tooNew, "a newer-than-app schema is DETECTABLE (policy decision: refuse vs read-only)");
    std::cout << "  (policy is the user's call: refuse to open, open read-only, or open\n"
                 "   and preserve unknown properties — ValueTree keeps unknown props/children\n"
                 "   verbatim, so 'open and preserve' is mechanically free)" << std::endl;
}

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    te::Engine engine { "DuetPersistencePrototype" };

    auto base = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("duet-persist-spike");
    base.deleteRecursively();
    base.createDirectory();
    std::cout << "work dir: " << base.getFullPathName() << std::endl;

    e1_itemIdStability (engine, base);
    e2_layering (engine, base);
    e3_savePollution (engine, base);
    e4_versioning (engine, base);

    banner ("SUMMARY");
    std::cout << "  passed=" << passed << " failed=" << failed << std::endl;
    return failed == 0 ? 0 : 1;
}
