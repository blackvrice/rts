//
// Created by black on 25. 12. 6..
//
// src/presenter/SelectCommand.cpp
#include "presenter/SelectCommand.hpp"
#include "manager/GameManager.hpp"
#include "component/Position.hpp"
#include "component/Selection.hpp"

namespace rts::presenter {

    void SelectCommand::execute(rts::manager::GameManager& game)
    {
        auto& reg = game.registry();

        auto& posStorage = reg.storage<rts::component::Position>();
        auto& selStorage = reg.storage<rts::component::Selection>();

        // 모든 선택 해제
        for (auto& [entity, sel] : selStorage.data()) {
            sel.selected = false;
        }

        // 박스 영역에 포함된 유닛 선택
        for (auto& [entity, pos] : posStorage.data()) {
            if (m_area.contains(pos.value)) {
                if (auto* sel = selStorage.get(entity))
                    sel->selected = true;
                else
                    selStorage.add(entity, { true });
            }
        }
    }

} // namespace rts::presenter
