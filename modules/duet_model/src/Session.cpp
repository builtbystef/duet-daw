#include <duet/model/Session.h>

#include <tracktion_engine/tracktion_engine.h>

#include <array>
#include <sstream>
#include <utility>

namespace te = tracktion;

namespace duet::model
{
namespace
{
    constexpr double demoPhraseSeconds = 8.0;
    constexpr int demoNoteVelocity = 100;
} // namespace

/** Everything engine-shaped lives here, so that Session.h can name no engine or
    JUCE type. The initialiser is declared first so that it outlives the engine:
    the engine's managers start timers and background threads that need a
    message manager, which is also what makes a Session usable headlessly.
*/
struct Session::Impl
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    te::Engine engine { "Duet" };
    te::Edit edit { engine, te::Edit::forEditing };
};

Session::Session() : impl (std::make_unique<Impl>()) {}
Session::~Session() = default;

int Session::audioTrackCount() const { return te::getAudioTracks (impl->edit).size(); }

double Session::tempoBpm() const { return impl->edit.tempoSequence.getBpmAt (te::TimePosition()); }

double Session::editLengthSeconds() const { return impl->edit.getLength().inSeconds(); }

void Session::loadDemoContent()
{
    auto tracks = te::getAudioTracks (impl->edit);

    if (tracks.isEmpty())
        return;

    auto* track = tracks.getFirst();
    track->setName ("Demo");

    if (auto instrument =
            impl->edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}))
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

    auto& transport = impl->edit.getTransport();
    transport.setLoopRange (phrase);
    transport.looping = true;
}

void Session::startPlayback()
{
    auto& transport = impl->edit.getTransport();
    transport.ensureContextAllocated();
    transport.play (false);
}

void Session::stopPlayback() { impl->edit.getTransport().stop (false, false); }

bool Session::isPlaying() const { return impl->edit.getTransport().isPlaying(); }

double Session::playbackPositionSeconds() const
{
    return impl->edit.getTransport().getPosition().inSeconds();
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
