#pragma once

#include <memory>
#include <core/model/IViewModel.hpp>
#include <core/model/Vector2D.hpp>

#include "core/model/IGameElement.hpp"

namespace rts::core::model {
    class Unit;
}

namespace rts::core::viewmodel {

    class UnitViewModel : public IViewModel {
    public:
        explicit UnitViewModel(std::shared_ptr<model::Unit> unit);

        // ===== IViewModel 구현 =====
        bool visible() const override;
        void setVisible(bool v) override;
        const char* name() const override;
        bool expired() const override;

        // ===== 업데이트 =====
        void update();

        // ===== View 전용 데이터 =====
        const model::Vector2D& position() const { return m_position; }
        float hpRatio() const { return m_hpRatio; }

        const void* modelPtr() const override;

    private:
        std::weak_ptr<model::Unit> m_unit;

        model::Vector2D m_position {};
        float m_hpRatio = 1.f;
        model::ActionType m_action = model::ActionType::Idle;

        bool m_visible = true;
    };

}
