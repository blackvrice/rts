//
// Created by black on 26. 1. 6..
//

#pragma once
#include "Unit.hpp"
#include "IViewModel.hpp"

namespace rts::core::model {
    class UnitViewModel : public IViewModel {
    public:
        explicit UnitViewModel(std::shared_ptr<Unit> unit);

    private:
        std::weak_ptr<Unit> m_unit;
    };
}
