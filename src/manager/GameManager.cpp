#include "manager/GameManager.hpp"
#include "component/RenderInfo.hpp"
#include "model/UnitSnapshot.hpp"

namespace rts::manager {

    GameManager::GameManager()
        : m_renderer(m_registry)     // ★ RendererSystem 초기화
    {
        m_bus = std::make_shared<rts::core::CommandBus>();
    }

    void GameManager::update(float dt)
    {
        // 1) 명령 처리
        m_bus->process(*this);

        // 2) ECS 시스템 업데이트
        m_movement.update(m_registry, dt);
        m_selection.update(m_registry, dt);
    }

    // ★ GameManager → Renderer 호출 (Scene에서 사용)
    void GameManager::render(sf::RenderWindow& win, float dt)
    {
        m_renderer.render(win, dt);
    }

    std::shared_ptr<core::CommandBus> GameManager::commands() {
        return m_bus;
    }

    ecs::Registry& GameManager::registry() {
        return m_registry;
    }

    // ★ Snapshot 생성 부분은 REST/Replay용
    //   RenderInfo → UnitSnapshot 구조는 그대로 둬도 됨
    model::Snapshot GameManager::buildSnapshot() const
    {
        model::Snapshot ss;

        auto& posStorage = m_registry.storage<rts::component::Position>();
        auto& renStorage = m_registry.storage<rts::component::RenderInfo>();

        for (auto const& [e, ren] : renStorage.data()) {
            if (auto* pos = posStorage.get(e)) {
                rts::model::UnitSnapshot u;
                u.position = pos->value;
                u.radius   = ren.radius;  // UI / Debug 용
                u.color    = ren.color;   // UI / Debug 용
                ss.units.push_back(u);
            }
        }

        return ss;
    }

} // namespace rts::manager
