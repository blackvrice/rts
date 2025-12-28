//
// Created by black on 25. 12. 25..
//

#pragma once
#include <core/model/Vector2D.hpp>

namespace rts::command {
    struct UIRenderCommand {
        enum class Type {
            Rect,
            Text,
            Sprite
        };

        Type type;

        core::Vector2D pos;
        core::Vector2D size;
    };
}
