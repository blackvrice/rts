#include "game/game/systems/FeedbackSystem.hpp"

#include <memory>

#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"

namespace rts::core::manager {
    void FeedbackSystem::capture(world::GameWorld& world) {
        std::unordered_map<std::uint64_t, ElementSnapshot> next;
        for (const auto& element : world.getElements()) {
            auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!gameElement) {
                continue;
            }
            float hp = 0.0f;
            bool complete = false;
            if (const auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                hp = unit->getHp();
            } else if (const auto building = std::dynamic_pointer_cast<model::Building>(element)) {
                hp = building->getHp();
                complete = building->isComplete();
            } else if (const auto resource = std::dynamic_pointer_cast<model::ResourceNode>(element)) {
                hp = static_cast<float>(resource->remaining());
            }

            const auto eid = gameElement->entityId();
            const auto id = (static_cast<std::uint64_t>(eid.index) << 32) | eid.generation;
            const auto action = gameElement->getAction();
            if (const auto it = m_snapshots.find(id); it != m_snapshots.end()) {
                const auto& prev = it->second;
                if (hp < prev.hp && action != model::ActionType::Dead) {
                    world::emitSound(world, world::SoundCue::Hit,
                                     gameElement->getPosition(), 44.0f);
                    world::emitEffect(world, world::EffectType::HitSpark,
                                      gameElement->getPosition(), 34.0f, 0.28f);
                }
                if (prev.action != model::ActionType::Dead &&
                    action == model::ActionType::Dead) {
                    const auto pos = gameElement->getPosition();
                    world::emitSound(world, world::SoundCue::Death, pos, 58.0f);
                    // Brief death puff only; no lingering blood/scorch decal at the spot.
                    world::emitEffect(world, world::EffectType::DeathBurst, pos, 52.0f, 0.6f);
                }
                if (!prev.complete && complete) {
                    world::emitSound(world, world::SoundCue::ConstructionComplete,
                                     gameElement->getPosition(), 62.0f);
                    world::emitEffect(world, world::EffectType::ConstructionDust,
                                      gameElement->getPosition(), 82.0f, 0.75f);
                }
            }
            // Dead elements linger in the world; don't keep tracking them (the
            // death transition already fired) so they can't seed phantom feedback.
            if (action != model::ActionType::Dead) {
                next[id] = ElementSnapshot {
                    .hp = hp,
                    .complete = complete,
                    .action = action
                };
            }
        }
        m_snapshots.swap(next);
    }
}
