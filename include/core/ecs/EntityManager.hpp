#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/ecs/EntityId.hpp"

namespace rts::core::ecs {
    // Allocates and tracks EntityIds. Destroyed slots are recycled, with the
    // slot's generation bumped on each destroy so handles created before the
    // reuse no longer validate (isAlive == false).
    class EntityManager {
    public:
        EntityId create() {
            if (!m_freeIndices.empty()) {
                const std::uint32_t index = m_freeIndices.back();
                m_freeIndices.pop_back();
                m_alive[index] = true;
                ++m_aliveCount;
                return EntityId { index, m_generations[index] };
            }

            const auto index = static_cast<std::uint32_t>(m_generations.size());
            m_generations.push_back(0);
            m_alive.push_back(true);
            ++m_aliveCount;
            return EntityId { index, 0 };
        }

        void destroy(const EntityId id) {
            if (!isAlive(id)) {
                return;
            }
            m_alive[id.index] = false;
            ++m_generations[id.index];  // invalidate handles to the old occupant
            m_freeIndices.push_back(id.index);
            --m_aliveCount;
        }

        bool isAlive(const EntityId id) const noexcept {
            return id.index < m_generations.size()
                && m_alive[id.index]
                && m_generations[id.index] == id.generation;
        }

        std::uint32_t generation(const std::uint32_t index) const noexcept {
            return index < m_generations.size() ? m_generations[index] : 0u;
        }

        std::size_t aliveCount() const noexcept { return m_aliveCount; }

    private:
        std::vector<std::uint32_t> m_generations;  // per slot index
        std::vector<bool> m_alive;                 // per slot index
        std::vector<std::uint32_t> m_freeIndices;  // recyclable slots
        std::size_t m_aliveCount { 0 };
    };
}
