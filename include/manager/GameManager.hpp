//
// Created by black on 25. 12. 7..
//
// include/rts/manager/GameManager.hpp
#pragma once

#include <memory>
#include <ecs/Registry.hpp>
#include <../command/CommandBus.hpp>
#include <model/Snapshot.hpp>
#include <system/MovementSystem.hpp>
#include <system/SelectionSystem.hpp>

#include "IManager.hpp"
#include "system/RendererSystem.hpp"
// 필요하면 SelectionSystem 도 추가

namespace rts::manager {

    class GameManager : public IManager{
    public:
        GameManager();

        void update(float dt);
        void render(sf::RenderWindow& win, float dt);   // ★ 추가
        std::shared_ptr<command::CommandBus> commands();
        ecs::Registry& registry();
        model::Snapshot buildSnapshot() const;
    private:
        ecs::Registry m_registry;
        system::MovementSystem m_movement;
        system::SelectionSystem m_selection;
        system::RendererSystem m_renderer;         // ★ 추가
        std::shared_ptr<command::CommandBus> m_bus;
    };

} // namespace manager
