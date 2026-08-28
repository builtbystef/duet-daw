#include <duet/collab/TrackRenders.h>

#include <string>
#include <system_error>
#include <utility>

namespace duet::collab
{
TrackRenderer offlineTrackRenderer (model::Session& session)
{
    return [&session] (model::TrackRef track,
                       const std::filesystem::path& destination,
                       const std::function<bool()>& keepGoing)
    {
        // Off a detached copy of the project, so that the producer goes on
        // playing and recording through an analysis that costs seconds: an
        // Edit that is rendering is an Edit that is not playing, and this
        // render is one nobody asked to wait for.
        //
        // The master is the whole project through the master chain, which is
        // exactly what a whole-project render is.
        return track == model::masterChannel
                   ? session.renderDetachedToFile (destination, keepGoing)
                   : session.renderDetachedTrackToFile (track, destination, keepGoing);
    };
}

//==============================================================================
TrackRenders::TrackRenders (TrackRenderer renderTrack,
                            std::filesystem::path renderFolder,
                            RunStillWanted runStillWanted)
    : render (std::move (renderTrack)), folder (std::move (renderFolder)),
      stillWanted (std::move (runStillWanted))
{
}

TrackRenders::~TrackRenders()
{
    for (const auto& [track, rendered] : cache)
    {
        std::error_code ignored;
        std::filesystem::remove (rendered.file, ignored);
    }
}

bool TrackRenders::wanted (const std::string& runId) const
{
    return ! stillWanted || stillWanted (runId);
}

std::optional<std::filesystem::path> TrackRenders::fileFor (model::TrackRef track,
                                                            const std::string& digest,
                                                            const std::string& runId)
{
    const auto held = cache.find (track);

    if (held != cache.end() && held->second.digest == digest)
        return held->second.file;

    auto destination =
        folder / ("analysis-" + std::to_string (track) + "-" + std::to_string (renders) + ".wav");
    ++renders;

    if (! render (track, destination, [this, &runId] { return wanted (runId); }))
    {
        std::error_code ignored;
        std::filesystem::remove (destination, ignored);

        return {};
    }

    if (held != cache.end())
    {
        std::error_code ignored;
        std::filesystem::remove (held->second.file, ignored);
        held->second = { digest, destination };
    }
    else
    {
        cache.emplace (track, Rendered { digest, destination });
    }

    return destination;
}
} // namespace duet::collab
