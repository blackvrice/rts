#include "game/game/GameLogicManager.hpp"

#include <algorithm>
#include <core/model/Unit.hpp>
#include <core/model/IGameElement.hpp>

namespace rts::manager {

    GameLogicManager::GameLogicManager(
        command::LogicCommandBus& bus,
        command::LogicCommandRouter& router)
        : manager::ILogicManager(bus, router)
    {
        // ===== 테스트용 유닛 생성 =====
        auto unit = std::make_shared<core::model::Unit>(*this);
        unit->setPosition({ 100.f, 100.f });
        unit->moveTo({ 300.f, 200.f });

        addElement(unit);
    }

    void GameLogicManager::start() {
        m_router.on<command::SelectCommand>([this](const command::SelectCommand& cmd) {
            clearSelection();

            for (auto& element : m_elements) {
                if (cmd.area().contains(element->getPosition())) {
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

        m_router.on<command::AttackCommand>([this](const command::AttackCommand&) {
            // TODO
        });

        m_router.on<command::HoldPositionCommand>([this](const command::HoldPositionCommand&) {
            for (auto& weak : m_selectedElements) {
                if (auto element = weak.lock()) {
                    element->holdPosition();
                }
            }
        });

        m_router.on<command::PatrolCommand>([this](const command::PatrolCommand&) {
            // TODO
        });
    }

    void GameLogicManager::update() {
        // Logic-level 업데이트 (AI, 상태 전환 등)
    }

    void GameLogicManager::tick(float dt) const {
        for (auto& element : m_elements) {
            element->tick(dt);
        }
    }

    void GameLogicManager::selectElement(
        const std::shared_ptr<core::model::IGameElement>& element)
    {
        m_selectedElements.push_back(element);
    }

    const std::vector<std::shared_ptr<core::model::IElement>>&
    GameLogicManager::getElements() const {
        return m_elements;
    }


    void GameLogicManager::clearSelection() {
        m_selectedElements.clear();
    }

    // ===============================
    // 이동 가능 여부 체크
    // ===============================
    bool GameLogicManager::canMoveUnitTo(
        const core::model::Unit& unit,
        const core::model::Vector2D& pos) const
    {
        // ===== 1. 맵 경계 체크 =====
        constexpr float MAP_MIN_X = 0.f;
        constexpr float MAP_MIN_Y = 0.f;
        constexpr float MAP_MAX_X = 2000.f;
        constexpr float MAP_MAX_Y = 2000.f;

        if (pos.x < MAP_MIN_X || pos.y < MAP_MIN_Y ||
            pos.x > MAP_MAX_X || pos.y > MAP_MAX_Y) {
            return false;
        }

        // ===== 2. 다른 유닛과 충돌 체크 =====
        constexpr float UNIT_RADIUS = 12.f;
        constexpr float MIN_DIST_SQ = UNIT_RADIUS * UNIT_RADIUS * 4.f;

        for (auto& element : m_elements) {
            auto other = std::dynamic_pointer_cast<core::model::Unit>(element);
            if (!other || other.get() == &unit)
                continue;

            auto p = other->getPosition();
            float dx = p.x - pos.x;
            float dy = p.y - pos.y;

            if ((dx * dx + dy * dy) < MIN_DIST_SQ) {
                return false;
            }
        }

        return true;
    }

    void GameLogicManager::addElement(
        std::shared_ptr<core::model::IGameElement> element)
    {
        if (!element)
            return;

        m_elements.push_back(std::move(element));
    }

}
