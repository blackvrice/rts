#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>

namespace rts::core::map {

class FogOfWar {
public:
    enum class State : uint8_t { Unexplored = 0, Explored = 1, Visible = 2 };

    void init(int w, int h);
    void resetVisible();                           // Visible → Explored (call at tick start)
    void revealCircle(int cx, int cy, int radius); // Mark cells in radius as Visible
    State get(int x, int y) const noexcept;
    int   width()  const noexcept { return m_width;  }
    int   height() const noexcept { return m_height; }
    const std::vector<State>& states() const noexcept { return m_states; }

private:
    int              m_width  = 0;
    int              m_height = 0;
    std::vector<State> m_states;
};

} // namespace rts::core::map
