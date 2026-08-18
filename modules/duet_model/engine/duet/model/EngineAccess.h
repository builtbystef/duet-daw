#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace duet::model
{
class Session;

/** The way through the engine seam, for the one module that has to cross it.

    Everything that edits a project goes through the vocabulary, and the
    vocabulary names no engine type. Persistence is different: what it writes is
    the project's whole state, and the engine's state tree is that state, so it
    reaches the Edit itself.

    This header is not on duet_model's public include path — only a target that
    links duet_model_engine_access sees it, and duet_persistence is the only one.
*/
struct EngineAccess
{
    /** The Edit a session is editing. */
    [[nodiscard]] static tracktion::engine::Edit& editOf (Session& session);
};
} // namespace duet::model
