#include "core/map/FogOfWar.hpp"
#include <algorithm>

namespace rts::core::map {

    void FogOfWar::init(int w, int h) {
        m_width = w;
        m_height = h;
        m_states.assign(static_cast<std::size_t>(w * h), State::Unexplored);
    }

    void FogOfWar::resetVisible() {
        for (auto& s : m_states) {
            if (s == State::Visible) {
                s = State::Explored;
            }
        }
    }

    void FogOfWar::revealCircle(int cx, int cy, int radius) {
        if (m_width <= 0 || m_height <= 0) return;

        const int r2 = radius * radius;
        const int minY = std::max(0, cy - radius);
        const int maxY = std::min(m_height - 1, cy + radius);
        const int minX = std::max(0, cx - radius);
        const int maxX = std::min(m_width - 1, cx + radius);

        for (int y = minY; y <= maxY; ++y) {
            const int dy = y - cy;
            const int dy2 = dy * dy;
            for (int x = minX; x <= maxX; ++x) {
                const int dx = x - cx;
                if (dx * dx + dy2 <= r2) {
                    m_states[static_cast<std::size_t>(y * m_width + x)] = State::Visible;
                }
            }
        }
    }

    FogOfWar::State FogOfWar::get(int x, int y) const noexcept {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
            return State::Unexplored;
        }
        return m_states[static_cast<std::size_t>(y * m_width + x)];
    }

} // namespace rts::core::map
