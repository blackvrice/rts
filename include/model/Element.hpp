//
// Created by black on 25. 12. 7..
//
#pragma once
#include <memory>

namespace rts::model {
    class Element {
    public:
        int id;
        std::vector<short> bitmap; // 32*32 크기
    };
}
