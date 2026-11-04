#include "game/game/systems/MatchRules.hpp"

#include "core/model/Building.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"
#include "game/game/systems/GameplayQueries.hpp"

namespace rts::core::manager {
    bool MatchRules::checkVictoryDefeat(world::GameWorld& world) {
        if (world.gameResult() != world::GameResult::InProgress) {
            return false;
        }

        // Elimination is permanent: a rebuilt Town Hall does not bring a player back.
        for (const auto& slot : world.players()) {
            if (!slot.eliminated &&
                queries::countBuildings(world, slot.id, model::BuildingType::TownHall) == 0) {
                world.player(slot.id)->eliminated = true;
            }
        }

        const int local = world.localPlayerId();
        bool localAllianceAlive = false;
        bool enemyAlive = false;
        for (const auto& slot : world.players()) {
            if (slot.eliminated) continue;
            if (world.areAllies(local, slot.id)) localAllianceAlive = true;
            if (world.areEnemies(local, slot.id)) enemyAlive = true;
        }

        // Defeat wins ties (both sides losing their last hall on the same tick),
        // matching the original 1v1 rule.
        if (!localAllianceAlive) {
            world.setGameResult(world::GameResult::Defeat);
            world::emitSound(world, world::SoundCue::Defeat, {}, 80.0f);
            return true;
        }
        if (!enemyAlive) {
            world.setGameResult(world::GameResult::Victory);
            world::emitSound(world, world::SoundCue::Victory, {}, 80.0f);
            return true;
        }
        return false;
    }
}
