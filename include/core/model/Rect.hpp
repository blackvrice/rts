//
// Created by black on 26. 1. 4..
//

#pragma once
#include "Vector2D.hpp"
#include <algorithm>

namespace rts::core::model {

    class Rect {
    public:
        Rect() = default;

        // 아무 두 점으로 생성 가능
        Rect(const Vector2D& a, const Vector2D& b) {
            m_min.x = std::min(a.x, b.x);
            m_min.y = std::min(a.y, b.y);
            m_max.x = std::max(a.x, b.x);
            m_max.y = std::max(a.y, b.y);
        }

        float left()   const { return m_min.x; }
        float top()    const { return m_min.y; }
        float right()  const { return m_max.x; }
        float bottom() const { return m_max.y; }

        float width()  const { return m_max.x - m_min.x; }
        float height() const { return m_max.y - m_min.y; }

        bool empty() const {
            return width() <= 0.f || height() <= 0.f;
        }

        bool contains(const Vector2D& p) const {
            return p.x >= left() && p.x <= right()
                && p.y >= top()  && p.y <= bottom();
        }

        bool intersects(const Rect& other) const {
            return !(other.left()   > right()  ||
                     other.right()  < left()   ||
                     other.top()    > bottom() ||
                     other.bottom() < top());
        }

        const Vector2D& min() const { return m_min; }
        const Vector2D& max() const { return m_max; }

    private:
        Vector2D m_min { 0.f, 0.f };
        Vector2D m_max { 0.f, 0.f };
    };

} // namespace rts::core::model
