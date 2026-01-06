//
// Created by black on 25. 12. 22..
//
#pragma once
#include <cmath>

namespace rts::core::model {
    struct Vector2D {
        float x = 0.f;
        float y = 0.f;

        Vector2D() = default;
        Vector2D(float x_, float y_) : x(x_), y(y_) {}


        Vector2D operator+(const Vector2D& o) const {
            return { x + o.x, y + o.y };
        }

        Vector2D operator-(const Vector2D& o) const {
            return { x - o.x, y - o.y };
        }

        Vector2D operator*(float s) const {
            return { x * s, y * s };
        }

        Vector2D operator/(float s) const {
            return { x / s, y / s };
        }

        bool operator==(const Vector2D& o) const {
            constexpr float EPS = 1e-5f;
            return std::fabs(x - o.x) < EPS &&
                   std::fabs(y - o.y) < EPS;
        }
    };
}