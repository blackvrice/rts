// Created by black on 25. 12. 27..
//
// core/render/RenderContext.hpp
#pragma once

#include "core/model/Vector2D.hpp"

namespace rts::core::render {

    struct RenderContext {
        void* nativeHandle;        // 플랫폼별 렌더 타겟
        Vector2D screenSize;       // 해상도
        float dpiScale = 1.0f;     // DPI 스케일
    };

}
