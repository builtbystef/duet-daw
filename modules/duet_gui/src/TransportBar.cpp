#include <duet/gui/TransportBar.h>

#include <duet/model/Session.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace duet::gui
{
namespace
{
    std::string padded (long long value, int width)
    {
        std::ostringstream text;
        text << std::setfill ('0') << std::setw (width) << value;
        return text.str();
    }
} // namespace

void TransportBar::setProjectStatus (std::string name, bool dirty)
{
    projectName = std::move (name);
    projectDirty = dirty;
}

double TransportBar::positionSeconds() const
{
    return session != nullptr ? std::max (0.0, session->playbackPositionSeconds()) : 0.0;
}

std::string TransportBar::musicalPosition() const
{
    const auto bpm = tempo();
    const auto signature =
        session != nullptr ? session->timeSignature() : duet::model::TimeSignature { 4, 4 };
    const auto beats = positionSeconds() * bpm / 60.0;
    const auto beatsPerBar = static_cast<double> (signature.numerator) * 4.0
                             / static_cast<double> (std::max (1, signature.denominator));
    const auto bar = static_cast<int> (std::floor (beats / beatsPerBar)) + 1;
    const auto intoBar = beats - std::floor (beats / beatsPerBar) * beatsPerBar;
    const auto beat = static_cast<int> (std::floor (intoBar)) + 1;
    const auto ticks = static_cast<int> (std::floor ((intoBar - std::floor (intoBar)) * 960.0));
    return padded (bar, 3) + "." + padded (beat, 2) + "." + padded (ticks, 3);
}

std::string TransportBar::wallTime() const
{
    const auto milliseconds = static_cast<long long> (std::floor (positionSeconds() * 1000.0));
    const auto hours = static_cast<int> (milliseconds / 3'600'000);
    const auto minutes = static_cast<int> ((milliseconds / 60'000) % 60);
    const auto seconds = static_cast<int> ((milliseconds / 1'000) % 60);
    const auto millis = static_cast<int> (milliseconds % 1'000);
    return padded (hours, 2) + ":" + padded (minutes, 2) + ":" + padded (seconds, 2) + "."
           + padded (millis, 3);
}

double TransportBar::tempo() const { return session != nullptr ? session->tempoBpm() : 120.0; }

std::string TransportBar::timeSignature() const
{
    const auto signature =
        session != nullptr ? session->timeSignature() : duet::model::TimeSignature { 4, 4 };
    return std::to_string (signature.numerator) + "/" + std::to_string (signature.denominator);
}

void TransportBar::setTempo (double bpm)
{
    if (session != nullptr && bpm > 0.0 && bpm != session->tempoBpm())
        session->performAction ("Set Tempo", [bpm] (auto& ops) { ops.setTempo (bpm); });
}

void TransportBar::setTimeSignature (int numerator, int denominator)
{
    if (session == nullptr || numerator <= 0 || denominator <= 0)
        return;

    const auto existing = session->timeSignature();
    if (existing.numerator != numerator || existing.denominator != denominator)
        session->performAction ("Set Time Signature",
                                [=] (auto& ops) { ops.setTimeSignature (numerator, denominator); });
}

void TransportBar::togglePlayback()
{
    if (session == nullptr)
        return;
    if (session->isPlaying())
        session->stopPlayback();
    else
        session->startPlayback();
}

void TransportBar::toggleRecording()
{
    if (session == nullptr)
        return;
    if (session->isRecording())
        session->stopRecording();
    else
        session->startRecording();
}

void TransportBar::toggleLoop()
{
    if (session != nullptr)
        session->setLooping (! session->isLooping());
}

void TransportBar::toggleMetronome()
{
    if (session != nullptr)
        session->setMetronomeEnabled (! session->metronomeEnabled());
}

void TransportBar::toggleFollowPlayhead() { view->setFollowPlayhead (! view->followPlayhead()); }

void TransportBar::goToStart()
{
    if (session != nullptr)
        session->setPlaybackPositionSeconds (0.0);
}

void TransportBar::goToEnd()
{
    if (session != nullptr)
        session->setPlaybackPositionSeconds (session->editLengthSeconds());
}

bool TransportBar::undo() { return session != nullptr && session->undo(); }
bool TransportBar::redo() { return session != nullptr && session->redo(); }
bool TransportBar::canUndo() const { return session != nullptr && ! session->undoNames().empty(); }
bool TransportBar::canRedo() const { return session != nullptr && ! session->redoNames().empty(); }

std::string TransportBar::undoLabel() const
{
    return canUndo() ? "Undo " + session->undoNames().front() : "Nothing to undo";
}

std::string TransportBar::redoLabel() const
{
    return canRedo() ? "Redo " + session->redoNames().front() : "Nothing to redo";
}

bool TransportBar::isPlaying() const { return session != nullptr && session->isPlaying(); }
bool TransportBar::isRecording() const { return session != nullptr && session->isRecording(); }
bool TransportBar::isLooping() const { return session != nullptr && session->isLooping(); }
bool TransportBar::metronomeEnabled() const
{
    return session != nullptr && session->metronomeEnabled();
}

void TransportBar::observeCpuLoad (double proportion)
{
    displayedCpuPercent =
        static_cast<int> (std::lround (std::clamp (proportion, 0.0, 1.0) * 100.0));
    overloadSamples = proportion >= 0.9 ? overloadSamples + 1 : 0;
    constexpr int sustainedSamples = 30; // one second at the bar's 30 Hz refresh
    health = overloadSamples >= sustainedSamples ? CpuHealth::overloaded : CpuHealth::healthy;
}

void TransportBar::sampleCpuLoad()
{
    observeCpuLoad (session != nullptr && session->isPlaying() ? session->cpuLoad() : 0.0);
}

std::string TransportBar::projectLabel() const { return projectName + (projectDirty ? " *" : ""); }
} // namespace duet::gui
