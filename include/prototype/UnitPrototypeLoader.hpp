//
// Created by black on 25. 12. 23..
//

#pragma once

namespace rts::prototype {
    class UnitPrototypeLoader {
    public:
        explicit UnitPrototypeLoader(factory::UnitFactory& factory)
            : m_factory(factory) {}

        void loadAll() {
            auto marineStats = loadStats("marine.json");
            m_factory.registerPrototype(
                UnitType::Marine,
                std::make_unique<Mrine>(marineStats)
            );
        }

    private:
        factory::UnitFactory& m_factory;
    };
}