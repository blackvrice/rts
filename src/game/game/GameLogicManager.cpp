#include "game/game/GameLogicManager.hpp"

#include <algorithm>
#include <iostream>
#include <core/model/Unit.hpp>
#include <core/model/IGameElement.hpp>
#include <core/world/GameWorld.hpp>

namespace rts::core::manager {
    GameLogicManager::GameLogicManager(
        command::LogicCommandBus &bus,
        command::LogicCommandRouter &router,
        core::world::GameWorld &world)
        : m_world(world), ILogicManager(bus, router) {
        // ===== 테스트용 유닛 생성 =====
        auto unit = std::make_shared<core::model::Unit>();
        unit->setPosition({300.f, 300.f});

        m_world.addElement(unit);

        auto unit2 = std::make_shared<core::model::Unit>();
        unit2->setPosition({500.f, 500.f});

        m_world.addElement(unit2);

        m_router.on<command::SelectCommand>([this](const command::SelectCommand &cmd) {
            clearSelection();

            auto elements = m_world.getElements();
            for (auto &element: elements) {
                if (!cmd.area().contains(element->getPosition()))
                    continue;

                if (auto *game = dynamic_cast<core::model::IGameElement *>(element.get())) {
                    selectElement(*game);
                }
            }
        });

        m_router.on<command::MoveCommand>([this](const command::MoveCommand &cmd) {
            handleMoveCommand(cmd);
        });

        m_router.on<command::AttackCommand>([this](const command::AttackCommand &) {
            // TODO
        });

        m_router.on<command::HoldPositionCommand>([this](const command::HoldPositionCommand &) {
            for (auto &weak: m_selectedElements) {
                if (auto element = weak.lock()) {
                    element->holdPosition();
                }
            }
        });

        m_router.on<command::PatrolCommand>([this](const command::PatrolCommand &) {
            // TODO
        });

        m_router.on<command::ControlGroupAddCommand>([this](const auto &cmd) {
            applySelectedToGroup(cmd.groupId(), /*assign=*/false);
        });

        m_router.on<command::ControlGroupAssignCommand>([this](const auto &cmd) {
            applySelectedToGroup(cmd.groupId(), /*assign=*/true);
        });

        m_router.on<command::ControlGroupSelectCommand>([this](const auto &cmd) {
            const uint16_t num = cmd.groupId();
            auto &group = m_groups[num];
            eraseExpired(group); // 여기서 한 번 정리
            m_selectedElements = group; // 대입
        });
    }

    void GameLogicManager::update() {
        // Logic-level 업데이트 (AI, 상태 전환 등)
    }

    void GameLogicManager::tick(float dt) {
        auto elements = m_world.getElements();
        for (auto &element: elements) {
            if (auto *game = dynamic_cast<core::model::IGameElement *>(element.get())) {
                game->tick(dt);
            }
        }
    }

    void GameLogicManager::selectElement(core::model::IGameElement &element) {
        m_selectedElements.push_back(element.weak_from_this());
    }

    void GameLogicManager::addSelectedElement(core::model::IGameElement &element) {
        m_selectedElements.push_back(element.weak_from_this());
    }


    void GameLogicManager::clearSelection() {
        m_selectedElements.clear();
    }

    // ===============================
    // 이동 가능 여부 체크
    // ===============================
    bool GameLogicManager::canMoveUnitTo(
        const core::model::Unit &unit,
        const core::model::Vector2D &pos) const {
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
        auto elements = m_world.getElements();
        for (auto &element: elements) {
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

    template<class T>
    void GameLogicManager::eraseExpired(std::vector<std::weak_ptr<T> > &v) {
        v.erase(std::remove_if(v.begin(), v.end(),
                               [](const std::weak_ptr<T> &w) { return w.expired(); }),
                v.end());
    }

    template<class T>
    bool GameLogicManager::containsPtr(const std::vector<std::weak_ptr<T> > &v, const std::shared_ptr<T> &p) {
        const T *raw = p.get();
        for (auto &w: v) {
            if (auto sp = w.lock(); sp && sp.get() == raw) return true;
        }
        return false;
    }


    void GameLogicManager::applySelectedToGroup(uint16_t num, bool assign) {
        auto &group = m_groups[num];

        // 선택 목록에서 expired 제거 (선택)
        eraseExpired(m_selectedElements);

        // 그룹에서도 expired 제거 (선택)
        eraseExpired(group);

        const auto selCount = m_selectedElements.size();

        if (assign) {
            group.clear();
            group.reserve(selCount);
        } else {
            group.reserve(group.size() + selCount); // 재할당 최소화
        }

        for (auto &w: m_selectedElements) {
            if (auto sp = w.lock()) {
                if (!assign) {
                    // 중복 방지 필요 없으면 이 if 블록을 통째로 제거
                    if (containsPtr(group, sp)) continue;
                }
                group.emplace_back(sp); // weak_ptr로 저장 (암시적 변환)
            }
        }
    }

    void GameLogicManager::handleMoveCommand(const command::MoveCommand& cmd)
    {
        const core::model::Vector2D target = cmd.target();
        for (auto &weak: m_selectedElements) {
            if (auto element = weak.lock()) {
                element->stop();
                element->moveTo(target);
            }
        }
    }

}
