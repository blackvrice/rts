//
// Created by black on 26. 1. 1..
//

#pragma once
#include "Key.hpp"

namespace rts::core::model {
    struct KeyEvent {
        Key key{};        // 논리 키 (의미 기반)
        Scan scancode{};    // 물리 키 (위치 기반)
        KeyModifier mods{KeyModifier::None};
    };
}
