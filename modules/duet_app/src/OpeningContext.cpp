#include <duet/app/OpeningContext.h>

#include <duet/collab/ProjectTools.h>

#include <algorithm>
#include <cmath>

namespace duet::app
{
duet::collab::OpeningContext openingContextOf (const ProducerMoment& moment)
{
    duet::collab::OpeningContext context;

    switch (moment.asked.scope)
    {
        case duet::gui::AskScope::clips:
            context.selection = duet::collab::SelectionKind::clips;

            for (const auto clip : moment.asked.clips)
                context.selectionIds.push_back (duet::collab::toolId::forClip (clip));

            break;

        case duet::gui::AskScope::track:
            context.selection = duet::collab::SelectionKind::tracks;
            context.selectionIds.push_back (duet::collab::toolId::forTrack (moment.asked.track));

            break;

        case duet::gui::AskScope::nothing:
            break;
    }

    // Bars and beats as the producer reads them: both count from one.
    const auto perBar = std::max (1.0, moment.beatsPerBar);
    const auto beats = std::max (0.0, moment.playheadBeats);

    context.playheadBar = static_cast<int> (std::floor (beats / perBar)) + 1;
    context.playheadBeat = std::fmod (beats, perBar) + 1.0;
    context.transportPlaying = moment.transportPlaying;

    return context;
}
} // namespace duet::app
