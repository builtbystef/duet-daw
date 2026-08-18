#include <duet/model/Session.h>

#include <duet/model/EngineAccess.h>

#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace te = tracktion;

namespace duet::model
{
namespace
{
    constexpr double demoPhraseSeconds = 8.0;
    constexpr int demoNoteVelocity = 100;

    /** ADR 0004: undo goes 200 Actions deep, in memory, for the session. */
    constexpr int undoDepth = 200;

    /** The engine names its fader parameter this, and Duet's fader is it. */
    constexpr const char* volumeParameterID = "volume";

    juce::File toJuceFile (const std::filesystem::path& path)
    {
        return juce::File { juce::String { path.string() } };
    }

    std::filesystem::path toPath (const juce::File& file)
    {
        return std::filesystem::path { file.getFullPathName().toStdString() };
    }

    /** How the project refers to a file: relative to the project folder when the
        file is inside it, absolute when it is not.

        The engine's own relative paths are written against the edit file and read
        against the folder that holds it, one level apart, which is how a clip ends
        up pointing at a file that does not exist and playing silence (hazard 5).
        Duet writes the reference the project reads.
    */
    std::string projectReferenceTo (const std::filesystem::path& projectFolder,
                                    const std::filesystem::path& sourceFile)
    {
        const auto relative =
            sourceFile.lexically_normal().lexically_relative (projectFolder.lexically_normal());
        const bool insideProject = ! relative.empty() && *relative.begin() != "..";

        return insideProject ? relative.generic_string() : sourceFile.generic_string();
    }
} // namespace

/** Everything engine-shaped lives here, so that Session.h can name no engine or
    JUCE type. The initialiser is declared first so that it outlives the engine:
    the engine's managers start timers and background threads that need a
    message manager, which is also what makes a Session usable headlessly.
*/
struct Session::Impl
{
    explicit Impl (std::filesystem::path file)
        : editFile (std::move (file)), projectFolder (editFile.parent_path())
    {
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    te::Engine engine { "Duet" };
    std::filesystem::path editFile;
    std::filesystem::path projectFolder;
    std::unique_ptr<te::Edit> edit;
    std::function<void()> projectChanged;

    juce::UndoManager& undoManager() const { return edit->getUndoManager(); }

    void announceChange() const
    {
        if (projectChanged)
            projectChanged();
    }

    te::AutomatableParameter* volumeParameterFor (TrackRef ref) const
    {
        if (auto* audioTrack = trackFor (ref))
            if (auto* volume = audioTrack->getVolumePlugin())
                return volume->getAutomatableParameterByID (volumeParameterID).get();

        return nullptr;
    }

    te::AudioTrack* trackFor (TrackRef ref) const
    {
        return dynamic_cast<te::AudioTrack*> (te::findTrackForID (
            *edit, te::EditItemID::fromRawID (static_cast<juce::uint64> (ref))));
    }

    te::Clip* clipFor (ClipRef ref) const
    {
        return te::findClipForID (*edit,
                                  te::EditItemID::fromRawID (static_cast<juce::uint64> (ref)));
    }

    static std::vector<std::string> toStrings (const juce::StringArray& strings)
    {
        std::vector<std::string> out;
        out.reserve (static_cast<std::size_t> (strings.size()));

        for (const auto& string : strings)
            out.push_back (string.toStdString());

        return out;
    }
};

//==============================================================================
Session::Session (std::filesystem::path editFile)
    : impl (std::make_unique<Impl> (std::move (editFile)))
{
    impl->edit = te::createEmptyEdit (impl->engine, toJuceFile (impl->editFile));
    startUndoHistory();
}

Session::Session (std::filesystem::path editFile, FromFile /*readIt*/)
    : impl (std::make_unique<Impl> (std::move (editFile)))
{
    impl->edit = te::loadEditFromFile (impl->engine, toJuceFile (impl->editFile));

    if (impl->edit != nullptr)
        startUndoHistory();
}

std::unique_ptr<Session> Session::openExisting (std::filesystem::path editFile)
{
    if (! std::filesystem::exists (editFile))
        return nullptr;

    std::unique_ptr<Session> session { new Session { std::move (editFile), FromFile {} } };

    return session->impl->edit != nullptr ? std::move (session) : nullptr;
}

void Session::startUndoHistory()
{
    // JUCE's UndoManager drops the oldest transactions once the stored units
    // pass their budget, but never below the minimum number of transactions. A
    // budget of one unit is what turns "at least 200 Actions" into "the newest
    // 200 Actions", which is the depth ADR 0004 asks for.
    impl->undoManager().setMaxNumberOfStoredUnits (1, undoDepth);

    // The engine creates its scene list lazily, the first time a track is added,
    // and that node outlives the undo that removes the track again. Creating it
    // up front is what makes two states of the same project comparable.
    impl->edit->getSceneList();

    // Opening a project is not an Action; the history starts clean.
    impl->undoManager().clearUndoHistory();
}

tracktion::engine::Edit& EngineAccess::editOf (Session& session) { return *session.impl->edit; }

Session::~Session()
{
    if (impl->edit != nullptr)
        impl->edit->getTransport().stop (false, true);
}

//==============================================================================
EditOps::EditOps (Session& owner) noexcept : session (owner) {}

TrackRef EditOps::addTrack (std::string_view name)
{
    auto& edit = *session.impl->edit;

    if (auto track =
            edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr))
    {
        track->setName (juce::String { std::string { name } });
        return static_cast<TrackRef> (track->itemID.getRawID());
    }

    return noTrack;
}

void EditOps::removeTrack (TrackRef track)
{
    if (auto* audioTrack = session.impl->trackFor (track))
        session.impl->edit->deleteTrack (audioTrack);
}

void EditOps::renameTrack (TrackRef track, std::string_view newName)
{
    if (auto* audioTrack = session.impl->trackFor (track))
        audioTrack->setName (juce::String { std::string { newName } });
}

void EditOps::moveTrack (TrackRef track, int newIndex)
{
    auto* audioTrack = session.impl->trackFor (track);

    if (audioTrack == nullptr)
        return;

    auto& edit = *session.impl->edit;
    auto order = te::getAudioTracks (edit);
    order.removeAllInstancesOf (audioTrack);

    // The engine places a track after the one it is told to follow, so the track
    // that ends up before this one is what an index means here.
    const auto placeAfter = juce::jlimit (0, order.size(), newIndex) - 1;
    const auto preceding = placeAfter >= 0 ? order[placeAfter]->itemID : te::EditItemID();

    edit.moveTrack (audioTrack, te::TrackInsertPoint { te::EditItemID(), preceding });
}

ClipRef EditOps::insertAudioClip (TrackRef track,
                                  std::string_view name,
                                  const std::filesystem::path& sourceFile,
                                  double startSeconds,
                                  double lengthSeconds)
{
    auto* audioTrack = session.impl->trackFor (track);

    if (audioTrack == nullptr)
        return noClip;

    const auto file = toJuceFile (sourceFile);
    const te::ClipPosition position { { te::TimePosition::fromSeconds (startSeconds),
                                        te::TimePosition::fromSeconds (startSeconds
                                                                       + lengthSeconds) } };

    if (auto clip = audioTrack->insertWaveClip (
            juce::String { std::string { name } }, file, position, false))
    {
        // Hazard 5: the reference the engine stores by default resolves against
        // a temporary directory, and the clip plays silence. Pin it here, to the
        // path the project keeps.
        clip->getSourceFileReference().source =
            juce::String { projectReferenceTo (session.impl->projectFolder, sourceFile) };
        return static_cast<ClipRef> (clip->itemID.getRawID());
    }

    return noClip;
}

void EditOps::moveClip (ClipRef clip, double newStartSeconds)
{
    if (auto* c = session.impl->clipFor (clip))
        c->setStart (te::TimePosition::fromSeconds (newStartSeconds), false, true);
}

void EditOps::trimClip (ClipRef clip, double newLengthSeconds)
{
    if (auto* c = session.impl->clipFor (clip))
        c->setLength (te::TimeDuration::fromSeconds (newLengthSeconds), true);
}

void EditOps::deleteClip (ClipRef clip)
{
    if (auto* c = session.impl->clipFor (clip))
        c->removeFromParent();
}

void EditOps::setTrackVolumeDb (TrackRef track, double db)
{
    if (auto* audioTrack = session.impl->trackFor (track))
        if (auto* volume = audioTrack->getVolumePlugin())
            volume->setVolumeDb (static_cast<float> (db));
}

void EditOps::addVolumeAutomationPoint (TrackRef track, double timeSeconds, double db)
{
    if (auto* parameter = session.impl->volumeParameterFor (track))
        parameter->getCurve().addPoint (
            te::EditPosition { te::TimePosition::fromSeconds (timeSeconds) },
            te::decibelsToVolumeFaderPosition (static_cast<float> (db)),
            0.0F,
            &session.impl->undoManager());
}

//==============================================================================
void Session::performAction (std::string_view name, const std::function<void (EditOps&)>& ops)
{
    if (! juce::MessageManager::existsAndIsCurrentThread())
        throw std::logic_error { "duet::model::Session: the message thread is the sole writer of "
                                 "the project model, and performAction ran on another thread" };

    // An Action that runs long enough for the engine's transaction timer to see
    // a quiet message loop would otherwise be split in two.
    const te::Edit::UndoTransactionInhibitor keepTheActionWhole { *impl->edit };

    impl->undoManager().beginNewTransaction (juce::String { std::string { name } });
    EditOps editOps { *this };
    ops (editOps);

    // No sealing call, deliberately: the engine's deferred undo-tracked writes
    // land in the transaction that is still open, which is the Action that
    // caused them. The next Action opens its own.

    impl->announceChange();
}

bool Session::undo()
{
    if (! impl->undoManager().canUndo())
        return false;

    impl->edit->undo();
    impl->announceChange();
    return true;
}

bool Session::redo()
{
    if (! impl->undoManager().canRedo())
        return false;

    impl->edit->redo();
    impl->announceChange();
    return true;
}

void Session::onProjectChanged (std::function<void()> callback)
{
    impl->projectChanged = std::move (callback);
}

std::vector<std::string> Session::undoNames() const
{
    return Impl::toStrings (impl->undoManager().getUndoDescriptions());
}

std::vector<std::string> Session::redoNames() const
{
    return Impl::toStrings (impl->undoManager().getRedoDescriptions());
}

//==============================================================================
std::vector<TrackInfo> Session::tracks() const
{
    std::vector<TrackInfo> out;

    for (auto* track : te::getAudioTracks (*impl->edit))
    {
        TrackInfo trackInfo;
        trackInfo.track = static_cast<TrackRef> (track->itemID.getRawID());
        trackInfo.name = track->getName().toStdString();

        for (auto* clip : track->getClips())
        {
            ClipInfo clipInfo;
            clipInfo.clip = static_cast<ClipRef> (clip->itemID.getRawID());
            clipInfo.name = clip->getName().toStdString();
            clipInfo.startSeconds = clip->getPosition().getStart().inSeconds();
            clipInfo.lengthSeconds = clip->getPosition().getLength().inSeconds();

            if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip))
            {
                clipInfo.sourceReference =
                    audioClip->getSourceFileReference().source.get().toStdString();
                clipInfo.sourceFile = toPath (audioClip->getSourceFileReference().getFile());
            }

            trackInfo.clips.push_back (std::move (clipInfo));
        }

        out.push_back (std::move (trackInfo));
    }

    return out;
}

int Session::audioTrackCount() const { return te::getAudioTracks (*impl->edit).size(); }

double Session::tempoBpm() const { return impl->edit->tempoSequence.getBpmAt (te::TimePosition()); }

double Session::editLengthSeconds() const { return impl->edit->getLength().inSeconds(); }

double Session::trackVolumeDb (TrackRef track) const
{
    if (auto* parameter = impl->volumeParameterFor (track))
        return te::volumeFaderPositionToDB (parameter->getCurrentExplicitValue());

    return 0.0;
}

double Session::liveTrackVolumeDb (TrackRef track) const
{
    if (auto* audioTrack = impl->trackFor (track))
        if (auto* volume = audioTrack->getVolumePlugin())
            return volume->getVolumeDb();

    return 0.0;
}

//==============================================================================
namespace
{
    /** True when a node says something about the project: it carries a property,
        or a descendant does.

        The engine builds an edit out of scaffolding nodes that are empty until
        something needs them, and fills in more of them when it reads the edit
        back off disk. An empty node states nothing, so counting one is how a
        comparison reports a difference between a project and itself.
    */
    bool saysSomething (const juce::ValueTree& node)
    {
        std::vector<juce::ValueTree> remaining { node };

        while (! remaining.empty())
        {
            const auto next = remaining.back();
            remaining.pop_back();

            if (next.getNumProperties() > 0)
                return true;

            for (const auto& child : next)
                remaining.push_back (child);
        }

        return false;
    }

    /** Writes a tree as what it states, rather than as it happens to be stored.

        Properties go in name order, empty nodes go away, and children go in type
        order — stably, so that the run of children of one type keeps the order it
        is in. A track list and a plugin chain both mean their order; which of a
        track's differently-shaped children comes first means nothing, and the
        engine writes those in one order and reads them back in another.
    */
    void appendCanonicalised (const juce::ValueTree& tree, std::string& out)
    {
        struct Pending
        {
            juce::ValueTree node;
            int depth;
        };

        std::vector<Pending> remaining { { tree, 0 } };

        while (! remaining.empty())
        {
            const auto [node, depth] = remaining.back();
            remaining.pop_back();

            const auto indent = static_cast<std::size_t> (depth) * 2;

            out.append (indent, ' ');
            out += node.getType().toString().toStdString();
            out += '\n';

            std::vector<std::string> properties;
            properties.reserve (static_cast<std::size_t> (node.getNumProperties()));

            for (int i = 0; i < node.getNumProperties(); ++i)
            {
                const auto name = node.getPropertyName (i);
                properties.push_back (name.toString().toStdString() + "="
                                      + node.getProperty (name).toString().toStdString());
            }

            std::sort (properties.begin(), properties.end());

            for (const auto& property : properties)
            {
                out.append (indent + 2, ' ');
                out += property;
                out += '\n';
            }

            std::vector<juce::ValueTree> children;

            for (const auto& child : node)
                if (saysSomething (child))
                    children.push_back (child);

            std::stable_sort (children.begin(),
                              children.end(),
                              [] (const juce::ValueTree& first, const juce::ValueTree& second)
                              { return first.getType().toString() < second.getType().toString(); });

            // This stack runs backwards, so the children go on to it that way round.
            std::reverse (children.begin(), children.end());

            for (const auto& child : children)
                remaining.push_back ({ child, depth + 1 });
        }
    }
} // namespace

std::string Session::stateDigest() const
{
    // Not a flush: Edit::flushState() writes the plugins' parameter blobs
    // through the undo history, so asking what the state is would change it
    // (hazard 3). Every vocabulary operation writes through to the tree anyway.
    auto state = impl->edit->state.createCopy();

    // What moves without an edit having happened. The project ID is the engine's
    // note of which ProjectManager item an edit belongs to: it stamps a fresh
    // one on an edit it creates and drops it again when it reads one back, and
    // Duet keeps no ProjectManager for it to mean anything to.
    state.removeChild (state.getChildWithName (te::IDs::TRANSPORT), nullptr);
    state.removeProperty ("lastSignificantChange", nullptr);
    state.removeProperty ("modifiedBy", nullptr);
    state.removeProperty (te::IDs::projectID, nullptr);

    std::string canonical;
    appendCanonicalised (state, canonical);

    std::ostringstream digest;
    digest << std::hex << std::hash<std::string> {}(canonical) << "/" << std::dec
           << canonical.size() << "B";

    return std::move (digest).str();
}

bool Session::renderToFile (const std::filesystem::path& destination)
{
    const auto file = toJuceFile (destination);
    file.deleteFile();

    // Rendered on this thread: a headless test has no message loop to wait on,
    // and the engine's threaded path reports progress through a UI it has none of.
    return te::Renderer::renderToFile (*impl->edit, file, false);
}

//==============================================================================
void Session::loadDemoContent()
{
    auto tracks = te::getAudioTracks (*impl->edit);

    if (tracks.isEmpty())
        return;

    auto* track = tracks.getFirst();
    track->setName ("Demo");

    if (auto instrument =
            impl->edit->getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}))
        track->pluginList.insertPlugin (instrument, 0, nullptr);

    const te::TimeRange phrase { te::TimePosition(),
                                 te::TimePosition::fromSeconds (demoPhraseSeconds) };

    if (auto clip = track->insertMIDIClip (phrase, nullptr))
    {
        // An A-minor arpeggio, two notes per beat, so that what comes out of the
        // speakers is unmistakably the app's own audio and not a click.
        static constexpr std::array<int, 8> pitches { 57, 60, 64, 69, 72, 69, 64, 60 };
        auto& sequence = clip->getSequence();

        for (int note = 0; note < 32; ++note)
            sequence.addNote (pitches.at (static_cast<std::size_t> (note % 8)),
                              te::BeatPosition::fromBeats (note * 0.5),
                              te::BeatDuration::fromBeats (0.45),
                              demoNoteVelocity,
                              0,
                              nullptr);
    }

    auto& transport = impl->edit->getTransport();
    transport.setLoopRange (phrase);
    transport.looping = true;
}

void Session::startPlayback()
{
    auto& transport = impl->edit->getTransport();
    transport.ensureContextAllocated();
    transport.play (false);
}

void Session::stopPlayback() { impl->edit->getTransport().stop (false, false); }

bool Session::isPlaying() const { return impl->edit->getTransport().isPlaying(); }

double Session::playbackPositionSeconds() const
{
    return impl->edit->getTransport().getPosition().inSeconds();
}

std::string Session::audioDeviceDescription() const
{
    auto* device = impl->engine.getDeviceManager().deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return {};

    const double sampleRate = device->getCurrentSampleRate();
    const int blockSize = device->getCurrentBufferSizeSamples();
    const double latencyMs =
        sampleRate > 0.0 ? 1000.0 * device->getOutputLatencyInSamples() / sampleRate : 0.0;

    std::ostringstream description;
    description.precision (1);
    description << device->getTypeName() << ": " << device->getName() << ", " << std::fixed
                << sampleRate << " Hz, " << blockSize << " samples, " << latencyMs << " ms out";

    return std::move (description).str();
}
} // namespace duet::model
