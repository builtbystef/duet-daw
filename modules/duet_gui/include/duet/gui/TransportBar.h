#pragma once

#include <duet/gui/ViewState.h>

#include <string>

namespace duet::model
{
class Session;
}

namespace duet::gui
{
enum class CpuHealth : std::uint8_t
{
    healthy,
    overloaded
};

/** The transport bar without painting: display values and producer gestures at
    the model and view-state seams. */
class TransportBar
{
public:
    explicit TransportBar (ViewState& projectView) : view (&projectView) {}

    void setSession (duet::model::Session* openProject) { session = openProject; }
    void setProjectStatus (std::string name, bool dirty);

    [[nodiscard]] std::string musicalPosition() const;
    [[nodiscard]] std::string wallTime() const;
    [[nodiscard]] double tempo() const;
    [[nodiscard]] std::string timeSignature() const;

    void setTempo (double bpm);
    void setTimeSignature (int numerator, int denominator);
    void setGridSize (GridSize size) { view->setGridSize (size); }
    [[nodiscard]] GridSize gridSize() const { return view->gridSize(); }

    void togglePlayback();
    void toggleRecording();
    void toggleLoop();
    void toggleMetronome();
    void toggleFollowPlayhead();
    void goToStart();
    void goToEnd();

    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] std::string undoLabel() const;
    [[nodiscard]] std::string redoLabel() const;

    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] bool isRecording() const;
    [[nodiscard]] bool isLooping() const;
    [[nodiscard]] bool metronomeEnabled() const;
    [[nodiscard]] bool followsPlayhead() const { return view->followPlayhead(); }

    void observeCpuLoad (double proportion);
    void sampleCpuLoad();
    [[nodiscard]] int cpuPercent() const { return displayedCpuPercent; }
    [[nodiscard]] CpuHealth cpuHealth() const { return health; }
    [[nodiscard]] std::string projectLabel() const;

private:
    [[nodiscard]] double positionSeconds() const;

    ViewState* view = nullptr;
    duet::model::Session* session = nullptr;
    std::string projectName;
    bool projectDirty = false;
    int overloadSamples = 0;
    int displayedCpuPercent = 0;
    CpuHealth health = CpuHealth::healthy;
};
} // namespace duet::gui
