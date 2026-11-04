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

        const int playerHalls = queries::countBuildings(
            world, model::TeamId::Player, model::BuildingType::TownHall);
        const int enemyHalls = queries::countBuildings(
            world, model::TeamId::Enemy, model::BuildingType::TownHall);

        if (playerHalls == 0) {
            world.setGameResult(world::GameResult::Defeat);
            world::emitSound(world, world::SoundCue::Defeat, {}, 80.0f);
            return true;
        }
        if (enemyHalls == 0) {
            world.setGameResult(world::GameResult::Victory);
            world::emitSound(world, world::SoundCue::Victory, {}, 80.0f);
            return true;
        }
        return false;
    }
}
