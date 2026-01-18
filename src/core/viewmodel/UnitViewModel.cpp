#include "core/viewmodel/UnitViewModel.hpp"
#include "core/model/Unit.hpp"

namespace rts::core::viewmodel {

    UnitViewModel::UnitViewModel(std::shared_ptr<model::Unit> unit)
        : m_unit(unit)
    {
        if (auto u = m_unit.lock()) {
            m_position = u->getPosition();
            m_action   = u->getAction();
            m_hpRatio  = u->getHp() / u->getMaxHp();
        }
    }

    void UnitViewModel::update() {
        auto unit = m_unit.lock();
        if (!unit)
            return;

        m_position = unit->getPosition();
        m_action   = unit->getAction();
        m_hpRatio  = unit->getHp() / unit->getMaxHp();
    }

    // ===== IViewModel =====

    bool UnitViewModel::visible() const {
        return m_visible;
    }

    void UnitViewModel::setVisible(bool v) {
        m_visible = v;
    }

    const char* UnitViewModel::name() const {
        return "Unit";
    }

    bool UnitViewModel::expired() const {
        return m_unit.expired();
    }

    const void* UnitViewModel::modelPtr() const {
        if (auto u = m_unit.lock()) {
            return u.get();
        }
        return nullptr;
    }
}
