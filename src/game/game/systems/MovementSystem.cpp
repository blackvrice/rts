#include "game/game/systems/MovementSystem.hpp"

#include <core/manager/PathManager.hpp>
#include <core/model/IGameElement.hpp>
#include <core/model/Unit.hpp>
#include <core/world/GameWorld.hpp>

#include "game/game/systems/CollisionSystem.hpp"

namespace rts::core::manager {
    void MovementSystem::update(
        world::GameWorld& world,
        float dt,
        const CollisionSystem& collision) const {
        for (const auto& element : world.getElements()) {
            if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                const auto previousPosition = unit->getPosition();
                if (unit->getAction() == model::ActionType::Move) {
                    unit->updateMove(dt, world.gridTransform());
                } else {
                    unit->tick(dt);
                }

                // Movement is speculative until the collision system accepts the next position.
                if (unit->getAction() != model::ActionType::Dead &&
                    !collision.canMoveUnitTo(world, *unit, unit->getPosition())) {
                    unit->setPosition(previousPosition);
                    unit->stop();
                }

                const auto currentPosition = unit->getPosition();
                if (currentPosition.x != previousPosition.x ||
                    currentPosition.y != previousPosition.y) {
                    world.onCollisionChanged();
                }
                continue;
            }

            if (auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element)) {
                gameElement->tick(dt);
            }
        }
    }

    void MovementSystem::issueMove(
        world::GameWorld& world,
        const SelectionSystem::SelectedList& selected,
        const model::Vector2D& target) const {
        const auto& transform = world.gridTransform();
        const auto goal = transform.worldToGrid(target);

        for (const auto& weak : selected) {
            if (auto element = weak.lock()) {
                auto unit = std::dynamic_pointer_cast<model::Unit>(element);
                if (!unit) {
                    element->stop();
                    element->moveTo(target);
                    continue;
                }

                path::PathOptions options;
                options.allowDiagonal = false;
                options.useDynamicBlocking = true;

                const auto start = transform.worldToGrid(unit->getPosition());
                const auto path = world.path().findPath(
                    world.gridQuery(),
                    world.collisionVersion(),
                    start,
                    goal,
                    options
                );

                unit->stop();
                if (path && !path->empty()) {
                    unit->setMoveTargetWithPath(*path, target);
                }
            }
        }
    }
}
