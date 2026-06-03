//
// Created by black on 25. 12. 25..
//

#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/manager/ILogicManager.hpp"

namespace rts::core::model {
    class IGameElement;
    class Unit;
    struct Vector2D;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class GameLogicManager final : public ILogicManager {
    public:
        GameLogicManager(command::LogicCommandBus &bus,
                         command::LogicCommandRouter &router,
                        core::world::GameWorld &world
                         );

        void update() override;

        void tick(float dt) override;

        bool canMoveUnitTo(const model::Unit &unit, const model::Vector2D &pos) const;


        void addSelectedElement(model::IGameElement &element);
        // Logic 전용 API
        void clearSelection();

        void selectElement(model::IGameElement &element);
        void handleMoveCommand(const command::MoveCommand& cmd);

    private:
        static constexpr int GROUPS = 10;

        template<class T>
        void eraseExpired(std::vector<std::weak_ptr<T> > &v);

        template<class T>
        bool containsPtr(const std::vector<std::weak_ptr<T> > &v, const std::shared_ptr<T> &p);

        void applySelectedToGroup(uint16_t num, bool assign);

        core::world::GameWorld& m_world;
        std::vector<std::weak_ptr<model::IGameElement> > m_selectedElements;
        std::array<std::vector<std::weak_ptr<model::IGameElement> >, GROUPS> m_groups;
    };
} // namespace rts::manager
