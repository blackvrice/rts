#pragma once

#include <map>

#include "core/model/PlayerResourceState.hpp"
#include "game/game/systems/SelectionSystem.hpp"

namespace rts::core::model {
    class ResourceNode;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    // Resource economy: worker gather orders, delivered resources, and the
    // per-tick population (supply) recount. Holds only the last logged
    // resource snapshot used for change logging.
    class EconomySystem {
    public:
        // Sends every selected live worker to gather the node, pairing it with
        // its closest drop-off (workers with no drop-off are stopped).
        void issueGather(world::GameWorld& world,
                         const SelectionSystem::SelectedList& selected,
                         model::ResourceNode& resource) const;

        // Credits resources that workers finished dropping off this tick.
        void applyReadyResourceDeliveries(world::GameWorld& world) const;

        // Re-targets workers whose drop-off died or whose node ran dry.
        void handleGatherRedirects(world::GameWorld& world) const;

        // Recomputes every player's food capacity/usage and army size from live
        // units, buildings and training queues, then logs any change.
        void recomputeSupply(world::GameWorld& world);

    private:
        // Logs gold/wood/food/army deltas for a team when they change (debug
        // output for the resource-change event).
        void logResourceChange(int teamId, const model::PlayerResourceState& current);

        // Last logged resource snapshot per player id.
        std::map<int, model::PlayerResourceState> m_lastResourceLog;
        bool m_resourceLogReady { false };
    };
}
