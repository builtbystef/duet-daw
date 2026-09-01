#pragma once

#include <duet/gui/Mixer.h>

#include <duet/model/Session.h>

#include <vector>

namespace duet::gui
{
/** One row in a track's input picker. */
struct InputChoice
{
    duet::model::InputRef input = duet::model::noInput;
    std::string label;
    bool enabled = true;
    bool selected = false;
};

/** The monitoring control for the selected input. */
struct MonitorState
{
    duet::model::InputMonitoring mode = duet::model::InputMonitoring::whileArmed;
    bool enabled = false;
};

/** One track's recording and routing surface, with no painting of its own. */
struct TrackIoSnapshot
{
    duet::model::TrackRef track = duet::model::noTrack;
    duet::model::TrackKind kind = duet::model::TrackKind::audio;
    std::vector<InputChoice> inputs;
    MonitorState monitoring;
    bool armAvailable = false;
    bool armed = false;
    bool recording = false;
    std::vector<RoutingDestination> outputs;
    duet::model::TrackRef output = duet::model::noTrack;
};

/** The paintless Track I/O seam both the arrangement and Mixer read and write.

    Owns no JUCE component. Input, monitoring, and arm are configuration with no
    Action; an output change is the existing Set Track Output Action.
*/
class TrackIo
{
public:
    void setSession (duet::model::Session* openProject);

    [[nodiscard]] TrackIoSnapshot snapshot (duet::model::TrackRef track) const;

    void setInput (duet::model::TrackRef track, duet::model::InputRef input);
    void setMonitoring (duet::model::TrackRef track, duet::model::InputMonitoring mode);
    void setArmed (duet::model::TrackRef track, bool armed);
    void setOutput (duet::model::TrackRef track, duet::model::TrackRef destination);

private:
    duet::model::Session* session = nullptr;
    Mixer mixer;
};
} // namespace duet::gui
