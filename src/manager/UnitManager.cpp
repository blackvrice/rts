//
// Created by black on 25. 12. 7..
//
#include "manager/UnitManager.hpp"

namespace rts::manager {

    UnitManager::UnitManager(rts::ecs::Registry& reg)
        : m_reg(reg)
    {}

    ecs::Entity UnitManager::createUnit(sf::Vector2f pos)
    {
        auto e = m_reg.createEntity();
        m_reg.add<rts::component::Position>(e, { pos });
        m_reg.add<rts::component::RenderInfo>(e, { sf::Color::White, 8 });
        m_reg.add<rts::component::Selection>(e, { false });
        return e;
    }

    void UnitManager::removeUnit(ecs::Entity e)
    {
        m_reg.destroyEntity(e);
    }

    std::vector<ecs::Entity> UnitManager::findInRect(const sf::FloatRect& rect)
    {
        std::vector<ecs::Entity> result;

        for (auto e : m_reg.entities())
        {
            if (!m_reg.has<component::Position>(e))
                continue;

            auto& pos = m_reg.get<component::Position>(e)->value;
            if (rect.contains(pos))
                result.push_back(e);
        }

        return result;
    }

} // namespace rts::manager
