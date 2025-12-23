//
// Created by black on 25. 12. 22..
//

#pragma once
#include <ui/UIElement.hpp>
#include <vector>

namespace rts::manager {
    class IUIManager {
    public:
        virtual ~IUIManager() = default;

        // UICommand 처리 (입력, 상태 변경 등)
        virtual void update() = 0;

        // 화면에 그리기만 담당 (side-effect 없음)
        virtual void render() = 0;
    };

}
