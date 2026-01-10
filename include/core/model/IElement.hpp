//
// Created by black on 26. 1. 4..
//

#pragma once
#include "core/render/RenderQueue.hpp"

namespace rts::core::model {
    class IElement {
    public:
        virtual ~IElement() = default;

        // 수명 주기
        virtual void build() {}      // 초기화 (리소스 연결)
        virtual void update() = 0;   // 상태 갱신
        virtual void destroy() {}    // 해제

        // 렌더 명령 생성
        virtual void buildRenderCommands(render::RenderQueue& out) const = 0;

        // 상태
        virtual bool visible() const { return true; }
    };
}
