//
// Created by black on 25. 12. 7..
//
#include "system/SelectionSystem.hpp"

namespace rts::system {

    void SelectionSystem::update(rts::ecs::Registry& reg, float dt)
    {
        auto& selStorage = reg.storage<rts::component::Selection>();
        auto& posStorage = reg.storage<rts::component::Position>();

        // 예: 이동 중 selected=true 를 유지하는 등의 후처리도 가능
        // 여기서는 SelectionSystem 이 따로 할 일 없을 수도 있음.
        // 하지만 구조를 위해 기본 형태를 유지한다.

        for (auto& [entity, sel] : selStorage.data()) {
            // selection 관련 후처리 로직 (필요하면 작성)
            // ex) Blink 효과, 색상 변경 등
        }
    }

}
