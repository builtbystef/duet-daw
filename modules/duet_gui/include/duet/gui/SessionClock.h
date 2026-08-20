#pragma once

#include <duet/gui/TimelineClock.h>

namespace duet::model
{
class Session;
}

namespace duet::gui
{
/** The open project's clock: the model's transport and tempo, in the beats the
    timeline is drawn in.

    One of these lives as long as the project it reads. The conversion is the
    project's tempo, which is what makes a beat the same distance everywhere on
    screen and the playhead land where the grid says.
*/
class SessionClock final : public TimelineClock
{
public:
    explicit SessionClock (duet::model::Session& openProject);

    [[nodiscard]] double beatsPerBar() const override;
    [[nodiscard]] double playheadBeats() const override;
    void setPlayheadBeats (double beats) override;
    [[nodiscard]] bool isPlaying() const override;
    [[nodiscard]] double contentLengthBeats() const override;

private:
    [[nodiscard]] double beatsPerSecond() const;

    duet::model::Session& session;
};
} // namespace duet::gui
