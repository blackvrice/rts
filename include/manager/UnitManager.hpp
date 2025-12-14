//
// Created by black on 25. 12. 7..
//
#pragma once
#include <vector>
#include <cstdint>
#include <SFML/Graphics/Rect.hpp>

#include "ecs/Registry.hpp"
#include "component/Position.hpp"
#include "component/RenderInfo.hpp"
#include "component/Selection.hpp"
#include "ecs/Registry.hpp"

namespace rts::manager {

    class UnitManager {
    public:
        explicit UnitManager(rts::ecs::Registry& registry);

        ecs::Entity createUnit(sf::Vector2f pos);
        void removeUnit(ecs::Entity e);

        std::vector<ecs::Entity> findInRect(const sf::FloatRect& rect);

    private:
        rts::ecs::Registry& m_reg;
    };

} // namespace rts::manager
