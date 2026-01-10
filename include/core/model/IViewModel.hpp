//
// Created by black on 26. 1. 6..
//

#pragma once

#include <string>

namespace rts::core::model {

    class IViewModel {
    public:
        virtual ~IViewModel() = default;

        // 🔄 프레임 갱신
        virtual void update() = 0;

        // 👁 표시
        virtual bool visible() const = 0;
        virtual void setVisible(bool v) = 0;

        // 🆔 디버그 / 식별
        virtual const char* name() const = 0;

        // 🧹 정리 가능 여부
        virtual bool expired() const = 0;
    };

}