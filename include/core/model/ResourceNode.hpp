#pragma once
#include "core/model/IGameElement.hpp"

namespace rts::core::model {

    class ResourceNode : public IGameElement {
    public:
        enum class ResourceType : int { Gold = 0, Wood = 1 };

        ResourceNode(Vector2D pos, ResourceType type, int totalAmount, int gatherAmount = 10);

        // ── 채집 ─────────────────────────────────────────────────
        bool tryGather(int& amountOut) override;
        int  resourceType() const override { return static_cast<int>(m_type); }
        int  remaining()    const noexcept { return m_remaining; }
        bool isDepleted()   const noexcept { return m_remaining <= 0; }
        ResourceType type() const noexcept { return m_type; }

        // ── IElement ─────────────────────────────────────────────
        void     update()                         override {}
        Vector2D getPosition()              const override { return m_position; }
        void     setPosition(const Vector2D& p)   override { m_position = p; }

        // ── IGameElement ─────────────────────────────────────────
        void tick(float)                          override {}
        const GameState& state()            const override { return m_state; }
        ActionType getAction()              const override {
            return m_remaining <= 0 ? ActionType::Dead : ActionType::Idle;
        }
        void setSelected(bool s)                  override { m_state.selected = s; }

        // no-ops
        void idle()                               override {}
        void stop()                               override {}
        void moveTo(const Vector2D&)              override {}
        void patrol(const Vector2D&, const Vector2D&) override {}
        void holdPosition()                       override {}
        void attack(IGameElement*)                override {}
        void attackMove(const Vector2D&)          override {}
        void takeDamage(float, IGameElement*)     override {}
        void gather(IGameElement*)                override {}
        void build(int, const Vector2D&)          override {}
        void cast(int, const Vector2D&)           override {}

        std::string displayName() const override;

    private:
        Vector2D     m_position;
        ResourceType m_type;
        int          m_remaining;
        int          m_gatherAmount;
        GameState    m_state{};
    };
}
