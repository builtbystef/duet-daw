#include <duet/model/Session.h>

#include <tracktion_engine/tracktion_engine.h>

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

    /** The project's edit file. Its name never reaches stored data — source
        references are relative to the folder that holds it — so the persistence
        facade owns the real project layout.
    */
    constexpr const char* editFileName = "project.tracktionedit";

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
    explicit Impl (std::filesystem::path folder) : projectFolder (std::move (folder)) {}

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    te::Engine engine { "Duet" };
    std::filesystem::path projectFolder;
    std::unique_ptr<te::Edit> edit;

    juce::UndoManager& undoManager() const { return edit->getUndoManager(); }

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
Session::Session (std::filesystem::path projectFolder)
    : impl (std::make_unique<Impl> (std::move (projectFolder)))
{
    impl->edit =
        te::createEmptyEdit (impl->engine, toJuceFile (impl->projectFolder / editFileName));

    // JUCE's UndoManager drops the oldest transactions once the stored units
    // pass their budget, but never below the minimum number of transactions. A
    // budget of one unit is what turns "at least 200 Actions" into "the newest
    // 200 Actions", which is the depth ADR 0004 asks for.
    impl->undoManager().setMaxNumberOfStoredUnits (1, undoDepth);

    // The engine creates its scene list lazily, the first time a track is added,
    // and that node outlives the undo that removes the track again. Creating it
    // up front is what makes two states of the same project comparable.
    impl->edit->getSceneList();

    // Building the empty project is not an Action; the history starts clean.
    impl->undoManager().clearUndoHistory();
}

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
}

bool Session::undo()
{
    if (! impl->undoManager().canUndo())
        return false;

    impl->edit->undo();
    return true;
}

bool Session::redo()
{
    if (! impl->undoManager().canRedo())
        return false;

    impl->edit->redo();
    return true;
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

//==============================================================================
namespace
{
    /** Writes a tree with its properties in name order.

        Undo and redo preserve what the state means while permuting the order in
        which its properties are stored, so a comparison that reads the state as
        it happens to be laid out reports differences that are not there.
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

            for (int i = node.getNumChildren(); --i >= 0;)
                remaining.push_back ({ node.getChild (i), depth + 1 });
        }
    }
} // namespace

std::string Session::stateDigest() const
{
    // Not a flush: Edit::flushState() writes the plugins' parameter blobs
    // through the undo history, so asking what the state is would change it
    // (hazard 3). Every vocabulary operation writes through to the tree anyway.
    auto state = impl->edit->state.createCopy();

    // What moves without an edit having happened.
    state.removeChild (state.getChildWithName (te::IDs::TRANSPORT), nullptr);
    state.removeProperty ("lastSignificantChange", nullptr);
    state.removeProperty ("modifiedBy", nullptr);

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
