#include <duet/collab/Estimate.h>

#include <utility>

namespace duet::collab
{
Json wrapped (const Estimate& estimate)
{
    Json out = Json::object();
    out["value"] = estimate.value;
    out["source"] = "estimated";
    out["method"] = estimate.method;
    out["confidence"] = estimate.confidence;

    return out;
}

void EstimateLedger::beginRun (const std::string& runId)
{
    const std::lock_guard lock (mutex);
    byRun[runId].clear();
}

Json EstimateLedger::record (const std::string& runId,
                             std::string tool,
                             std::string field,
                             const Estimate& estimate)
{
    {
        const std::lock_guard lock (mutex);
        byRun[runId].push_back (LedgerEntry { std::move (tool), std::move (field), estimate });
    }

    return wrapped (estimate);
}

bool EstimateLedger::basedOnEstimates (const std::string& runId) const
{
    const std::lock_guard lock (mutex);
    const auto found = byRun.find (runId);

    return found != byRun.end() && ! found->second.empty();
}

std::vector<LedgerEntry> EstimateLedger::entries (const std::string& runId) const
{
    const std::lock_guard lock (mutex);
    const auto found = byRun.find (runId);

    return found == byRun.end() ? std::vector<LedgerEntry> {} : found->second;
}
} // namespace duet::collab
