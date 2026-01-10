// Created by black on 26. 1. 1..
//
//

#include "game/game/GameLogicManager.hpp"

namespace rts::manager {
    GameLogicManager::GameLogicManager(command::LogicCommandBus &bus,
                                         command::LogicCommandRouter &router) : manager::ILogicManager(bus, router) {

    }

    void GameLogicManager::start() {
        m_router.on<command::SelectCommand>([this](const command::SelectCommand& cmd) {
            clearSelection();
            for (auto element : m_elements) {
                if (cmd.area().contains(element->position())) {
                    selectElement(element);
                }
            }
        });

        m_router.on<command::MoveCommand>([this](const command::MoveCommand& cmd) {
            core::model::Vector2D base = cmd.target();
            int index = 0;

            for (auto& weak : m_selectedElements) {
                if (auto element = weak.lock()) {
                    core::model::Vector2D offset{
                        (index % 4) * 20.f,
                        (index / 4) * 20.f
                    };

                    element->stop();
                    element->moveTo(base + offset);
                    ++index;
                }
            }
        });

        m_router.on<command::AttackCommand>([this](const command::AttackCommand& cmd) {
            for (auto element : m_selectedElements) {

            }
        });

        m_router.on<command::HoldPositionCommand>([this](const command::HoldPositionCommand& cmd) {
            for (auto element : m_selectedElements) {

            }
        });

        m_router.on<command::PatrolCommand>([this](const command::PatrolCommand& cmd) {
            for (auto element : m_selectedElements) {

            }
        });
    }


    void GameLogicManager::update() {

    }

    void GameLogicManager::tick(const float dt) const {
        for (auto element : m_elements) {
            element->tick(dt);
        }
    }

    void GameLogicManager::selectElement(const std::shared_ptr<core::model::IGameElement>& element) {
        m_selectedElements.push_back(element);
    }

    void GameLogicManager::clearSelection() {
        m_selectedElements.clear();
    }


}
