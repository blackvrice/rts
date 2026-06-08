#pragma once

#include <cstddef>
#include <cstdint>

namespace rts::core::ecs {
    // Stable handle to a game entity. `index` locates a slot; `generation`
    // distinguishes the current occupant from any previous one that reused the
    // same slot, so a stale handle never resolves to a different live entity.
    struct EntityId {
        std::uint32_t index { 0 };
        std::uint32_t generation { 0 };

        bool operator==(const EntityId&) const noexcept = default;
    };

    // Sentinel for "no entity". index is out of any real range so it can never
    // be alive in an EntityManager.
    inline constexpr EntityId InvalidEntityId { 0xFFFFFFFFu, 0u };

    constexpr bool isValid(const EntityId id) noexcept {
        return id != InvalidEntityId;
    }
}

template <>
struct std::hash<rts::core::ecs::EntityId> {
    std::size_t operator()(const rts::core::ecs::EntityId& id) const noexcept {
        return (static_cast<std::size_t>(id.index) << 32) ^ id.generation;
    }
};
