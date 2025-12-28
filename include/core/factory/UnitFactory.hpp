//
// Created by black on 25. 12. 23..
//

#pragma once
#include <memory>

namespace rts::factory {
    class UnitFactory {
    public:
        void registerPrototype(UnitType type, std::unique_ptr<unit::Unit> prototype) {
            m_prototypes[type] = std::move(prototype);
        }

        std::unique_ptr<unit::Unit> create(UnitType type) {
            auto it = m_prototypes.find(type);
            if (it == m_prototypes.end())
                throw std::runtime_error("Unknown UnitType");

            return it->second->clone(); // ⭐ Prototype
        }

    private:
        std::unordered_map<UnitType, std::unique_ptr<unit::Unit> > m_prototypes;
    };
}
