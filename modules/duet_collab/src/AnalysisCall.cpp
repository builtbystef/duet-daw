#include "AnalysisCall.h"

#include <algorithm>
#include <cmath>

namespace duet::collab::analysisCall
{
namespace
{
    /** How far a bar boundary may be out before it counts as the next bar.

        A bar position is arithmetic over the tempo map, so a clip that starts
        exactly on bar 97 can come back as 97.000000001, and the project would
        be one bar longer than it is.
    */
    constexpr double barTolerance = 1.0e-6;
} // namespace

RpcOutcome abandoned()
{
    return RpcOutcome::failure (rpcError::runAbandoned, "the run was canceled");
}

std::string argument (const ToolCall& call, std::string_view name)
{
    const auto found = call.arguments.find (name);

    if (found == call.arguments.end() || ! found->is_string())
        return {};

    return found->get<std::string>();
}

std::optional<BarRange> barRangeOf (const ToolCall& call)
{
    const auto found = call.arguments.find ("barRange");

    if (found == call.arguments.end() || found->is_null())
        return BarRange {};

    if (! found->is_array() || found->size() != 2 || ! found->at (0).is_number()
        || ! found->at (1).is_number())
        return {};

    const auto first = found->at (0).get<double>();
    const auto last = found->at (1).get<double>();

    if (first < 1.0 || last < first)
        return {};

    return BarRange { true, first, last };
}

int barCountOf (const model::Session& session)
{
    const auto bars = session.barAtSeconds (session.editLengthSeconds()) - 1.0;

    return std::max (0, static_cast<int> (std::ceil (bars - barTolerance)));
}

std::optional<Plan> readPlan (const model::Session& session,
                              const ProjectReadMarshal& marshal,
                              const std::string& id,
                              const BarRange& bars)
{
    std::optional<Plan> plan;

    marshal (
        [&]
        {
            const auto ref = toolId::toTrack (id);

            if (! ref.has_value())
                return;

            const auto digest = session.trackStateDigest (*ref);

            if (digest.empty())
                return;

            Plan made { *ref,
                        digest,
                        bars.stated ? session.secondsAtBar (bars.firstBar) : 0.0,
                        bars.stated ? session.secondsAtBar (bars.lastBar + 1.0) : 0.0,
                        ! bars.stated };

            // The bars a call is answered bar by bar over: the ones it named, or
            // every bar the project holds. A whole bar and not a fraction of
            // one, because a bar is what a chord is named at.
            made.firstBar = static_cast<int> (std::floor (bars.stated ? bars.firstBar : 1.0));
            made.lastBar =
                bars.stated ? static_cast<int> (std::floor (bars.lastBar)) : barCountOf (session);

            for (auto bar = made.firstBar; bar <= made.lastBar + 1; ++bar)
                made.barStarts.push_back (session.secondsAtBar (bar));

            plan = std::move (made);
        });

    return plan;
}
} // namespace duet::collab::analysisCall
