#pragma once

#include <duet/model/Session.h>

#include <duet/model/EngineAccess.h>

#include <string>
#include <unordered_map>

/** The engine side of the model, shared by the two files that make it up.

    Session.h names no engine type, so everything engine-shaped is here, off the
    module's public include path. Session.cpp holds the Session, EditOps.cpp the
    vocabulary; both need the same handle onto the Edit.
*/
namespace duet::model
{
namespace te = tracktion;

inline juce::File toJuceFile (const std::filesystem::path& path)
{
    return juce::File { juce::String { path.string() } };
}

inline std::filesystem::path toPath (const juce::File& file)
{
    return std::filesystem::path { file.getFullPathName().toStdString() };
}

inline juce::String toJuceString (std::string_view text)
{
    return juce::String { std::string { text } };
}

/** The aux return a bus track carries, if any: the engine routes sends by bus
    number, and this is what ties one of those numbers to a Duet bus.
*/
inline te::AuxReturnPlugin* returnOn (te::AudioTrack& bus)
{
    for (auto* plugin : bus.pluginList.getPlugins())
        if (auto* auxReturn = dynamic_cast<te::AuxReturnPlugin*> (plugin))
            return auxReturn;

    return nullptr;
}

inline te::EditItemID toItemID (std::uint64_t ref)
{
    return te::EditItemID::fromRawID (static_cast<juce::uint64> (ref));
}

template <typename Ref>
Ref toRef (te::EditItemID id)
{
    return static_cast<Ref> (id.getRawID());
}

/** How the project refers to a file: relative to the project folder when the
    file is inside it, absolute when it is not.

    The engine's own relative paths are written against the edit file and read
    against the folder that holds it, one level apart, which is how a clip ends
    up pointing at a file that does not exist and playing silence (hazard 5).
    Duet writes the reference the project reads.
*/
std::string projectReferenceTo (const std::filesystem::path& projectFolder,
                                const std::filesystem::path& sourceFile);

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

    //==============================================================================
    // Keeping an asked-for playback rolling (hazard 6).

    /** How many times running the transport has been asked to play without it
        rolling. Reset by every tick that finds it rolling, so the rebuild —
        which arrives after playback has started — gets the whole window again.
    */
    int askedWithoutRolling = 0;

    /** Asks the transport to play, allocating the playback context first: after
        the device rebuild there is no context to play through.
    */
    void askTransportToPlay() const;

    /** One tick of the retry: gives up, does nothing, or asks again. */
    void keepPlaybackRolling();

    /** Runs for exactly as long as playback is wanted — its running is that
        memory, and there is no second copy of it to keep in step — and drives
        keepPlaybackRolling.
    */
    struct PlaybackKeeper final : juce::Timer
    {
        explicit PlaybackKeeper (Impl& owner) : impl (&owner) {}
        void timerCallback() override { impl->keepPlaybackRolling(); }

        Impl* impl = nullptr;
    };

    /** The stretch the transport loops over, in beats.

        Kept in musical time and not in seconds, because the tempo map moves
        under it: a tempo change rescales every clip, and a loop that stayed
        where it was in seconds would stop being the loop over the phrase the
        producer set it over. The engine rescales its own loop range when the
        tempo is set and does not when that change is undone, so the range it
        holds is only ever a cached answer — this is the question.
    */
    std::optional<std::pair<double, double>> loopBeats;

    /** Puts the engine's loop range back in step with the musical one. */
    void applyLoopRange() const;

    void announceChange() const
    {
        if (projectChanged)
            projectChanged();
    }

    //==============================================================================
    // Finding the things a ref names.

    te::AudioTrack* trackFor (TrackRef ref) const
    {
        return dynamic_cast<te::AudioTrack*> (te::findTrackForID (*edit, toItemID (ref)));
    }

    te::Clip* clipFor (ClipRef ref) const { return te::findClipForID (*edit, toItemID (ref)); }

    te::MidiClip* midiClipFor (ClipRef ref) const
    {
        return dynamic_cast<te::MidiClip*> (clipFor (ref));
    }

    te::Plugin* pluginFor (PluginRef ref) const
    {
        return te::findPluginForID (*edit, toItemID (ref)).get();
    }

    te::VolumeAndPanPlugin* faderFor (TrackRef ref) const
    {
        if (auto* track = trackFor (ref))
            return track->getVolumePlugin();

        return nullptr;
    }

    /** The parameter an automation target names, or null when the thing that
        would own the curve is not there.
    */
    te::AutomatableParameter* parameterFor (const AutomationTarget& target) const;

    /** Puts every plugin parameter back in step with the state it is stored in.

        A plugin keeps each parameter's value in the plugin as well as in the
        state, and the engine deliberately does not follow a state change back
        into the plugin: a change there may be automation or a modifier
        speaking, and neither is the value the producer set. An undo is neither
        of those — it is the producer's own value returning — so the model says
        so. Without this a read after an undo answers with the value the undo
        took away, and the fader does not move.
    */
    void refreshParametersFromState() const;

    //==============================================================================
    // Notes. The engine gives a note no durable identity of its own, so the
    // model keeps one: a note is its ValueTree, and JUCE's undo of a removed
    // child puts back the very object it took away, so a handle stays pointed at
    // the same note across undo and redo.

    struct NoteHandle
    {
        ClipRef clip = noClip;
        juce::ValueTree state;
    };

    mutable std::unordered_map<NoteRef, NoteHandle> notesByRef;
    mutable NoteRef nextNoteRef = 1;

    /** The handle for a note, made on first sight of it. */
    NoteRef refForNote (ClipRef clip, const juce::ValueTree& noteState) const;

    /** The note a handle names, or null once it is gone for good. */
    te::MidiNote* noteFor (NoteRef ref) const;

    //==============================================================================
    // Declared last, so that it stops before anything it touches goes away.
    PlaybackKeeper playbackKeeper { *this };

    static std::vector<std::string> toStrings (const juce::StringArray& strings)
    {
        std::vector<std::string> out;
        out.reserve (static_cast<std::size_t> (strings.size()));

        for (const auto& string : strings)
            out.push_back (string.toStdString());

        return out;
    }
};

/** What a track is for.

    Two of the three kinds are legible from the track itself, and the third is
    not: a Duet group bus is an ordinary engine track that the producer
    designated as a bus, and nothing in the engine's own state says so. The
    designation is stored on the track's own tree rather than in the DUET tree,
    so that it travels with the track — deleting the track takes it away, and
    undoing the deletion brings it back, with no code of ours involved.
*/
TrackKind trackKindOf (te::AudioTrack& track);

/** The engine plugin a Duet built-in is, as the plugin cache names it. */
const char* engineTypeOf (BuiltinPlugin plugin);

/** Which built-in a plugin is, or nothing for one Duet does not ship. */
std::optional<BuiltinPlugin> builtinOf (te::Plugin& plugin);

/** Writes every parameter of a plugin into its state, at the value it already
    has.

    A parameter sitting at its default has no property in the state at all, and
    the engine writes one the first time the value changes — which means the
    undo of that first change takes the property away again, and the next thing
    to put the parameter back in step with the state has to create it, an edit
    of its own that would clear the redo stack. A plugin that states all of its
    parameters from the moment it is made never gets into that position: an undo
    only ever puts an existing property back to an earlier value.
*/
void stateParametersExplicitly (te::Plugin& plugin);
} // namespace duet::model
