#pragma once

#include <duet/collab/JsonRpc.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace duet::collab
{
/** One estimated value: what it is, what routine says so, and how much that
    routine trusts itself, from 0 to 1.

    The shape of a guess, and the whole of the provenance contract's asymmetry
    (ADR 0002): a value read from the project model or measured by a documented
    routine crosses the seam as a bare scalar, and every guess crosses as one of
    these. So a bare value is by construction a fact, and the Collaborator can
    tell the two apart without being told which is which.
*/
struct Estimate
{
    Json value;
    std::string method;
    double confidence = 0.0;
};

/** That estimate as the seam writes it: `{ value, source, method, confidence }`,
    with `source` always "estimated".
*/
[[nodiscard]] Json wrapped (const Estimate& estimate);

/** One line of a run's ledger: the tool that handed an estimate over, what that
    tool called it, and the estimate itself.
*/
struct LedgerEntry
{
    std::string tool;
    std::string field;
    Estimate estimate;
};

/** Every estimated value the Collaborator was handed, run by run.

    The mechanism behind the estimate mark, and the reason the mark is
    mechanical rather than the model's own account of itself (spec js437t): a
    run whose ledger holds anything has been handed a guess, so everything it
    says from then on is marked as based on estimates, whether or not the guess
    was used and whether or not the model mentions it. Over-marking is the
    accepted cost of never under-marking.

    Wrapping and recording are one act — `record` is what answers with the
    wrapped value — so a tool cannot hand an estimate over without the ledger
    knowing about it.

    A run's lines survive the run, because the mark on what it said is
    inspectable for as long as the conversation holds it. What clears them is
    that run's own name coming round again, which is what `beginRun` is for: no
    ledger is inherited, whatever the run before it did or however it ended.

    Written from the Collaborator service's thread as tools answer, and read
    from wherever a surface shows the mark, so every member takes this object's
    own lock. It is never held while anything else is called.
*/
class EstimateLedger
{
public:
    EstimateLedger() = default;

    /** Empties this run's ledger, so that a run begins having been handed
        nothing.
    */
    void beginRun (const std::string& runId);

    /** Records one estimated value against a run, and answers it wrapped, ready
        to cross the seam.
    */
    Json record (const std::string& runId,
                 std::string tool,
                 std::string field,
                 const Estimate& estimate);

    /** Whether this run has been handed an estimate, which is the mark itself. */
    [[nodiscard]] bool basedOnEstimates (const std::string& runId) const;

    /** What this run was handed, in the order it was handed over. */
    [[nodiscard]] std::vector<LedgerEntry> entries (const std::string& runId) const;

private:
    mutable std::mutex mutex;
    std::map<std::string, std::vector<LedgerEntry>> byRun;
};
} // namespace duet::collab
