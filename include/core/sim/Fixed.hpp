#pragma once

#include <cstdint>

#include "core/path/GridTypes.hpp"

namespace rts::core::sim {
    // 16.16 fixed-point scalar for deterministic simulation math: identical
    // integer results on every platform/run, unlike float. Range ~[-32768,
    // 32767] with a 1/65536 step — ample for a few-thousand-unit world.
    //
    // This is the determinism foundation for Epic 1.4.3. It is intentionally not
    // yet wired into the live simulation; movement, attack ranges, and
    // projectiles migrate onto it incrementally (per the plan) so each step can
    // be verified in isolation.
    class Fixed {
    public:
        static constexpr int kFractionBits = 16;
        static constexpr std::int32_t kOne = 1 << kFractionBits;

        constexpr Fixed() noexcept = default;

        static constexpr Fixed fromRaw(std::int32_t raw) noexcept {
            Fixed f;
            f.m_raw = raw;
            return f;
        }
        static constexpr Fixed fromInt(std::int32_t v) noexcept {
            return fromRaw(v << kFractionBits);
        }
        static constexpr Fixed fromFloat(float v) noexcept {
            // Round to nearest; constant-only path used at boundaries (e.g. loading
            // float data), never inside the deterministic update.
            return fromRaw(static_cast<std::int32_t>(
                v * static_cast<float>(kOne) + (v >= 0.0f ? 0.5f : -0.5f)));
        }

        constexpr std::int32_t raw() const noexcept { return m_raw; }
        constexpr float toFloat() const noexcept {
            return static_cast<float>(m_raw) / static_cast<float>(kOne);
        }
        // Floor toward negative infinity (arithmetic shift), so grid math is
        // correct for negative coordinates.
        constexpr std::int32_t toInt() const noexcept { return m_raw >> kFractionBits; }

        constexpr Fixed operator+(Fixed o) const noexcept { return fromRaw(m_raw + o.m_raw); }
        constexpr Fixed operator-(Fixed o) const noexcept { return fromRaw(m_raw - o.m_raw); }
        constexpr Fixed operator-() const noexcept { return fromRaw(-m_raw); }
        constexpr Fixed operator*(Fixed o) const noexcept {
            return fromRaw(static_cast<std::int32_t>(
                (static_cast<std::int64_t>(m_raw) * o.m_raw) >> kFractionBits));
        }
        constexpr Fixed operator/(Fixed o) const noexcept {
            return fromRaw(static_cast<std::int32_t>(
                (static_cast<std::int64_t>(m_raw) << kFractionBits) / o.m_raw));
        }
        constexpr Fixed& operator+=(Fixed o) noexcept { m_raw += o.m_raw; return *this; }
        constexpr Fixed& operator-=(Fixed o) noexcept { m_raw -= o.m_raw; return *this; }

        constexpr auto operator<=>(const Fixed&) const noexcept = default;
        constexpr bool operator==(const Fixed&) const noexcept = default;

    private:
        std::int32_t m_raw { 0 };
    };

    struct FixedVec2 {
        Fixed x;
        Fixed y;

        constexpr FixedVec2 operator+(FixedVec2 o) const noexcept { return { x + o.x, y + o.y }; }
        constexpr FixedVec2 operator-(FixedVec2 o) const noexcept { return { x - o.x, y - o.y }; }
        constexpr FixedVec2 operator*(Fixed s) const noexcept { return { x * s, y * s }; }
        constexpr Fixed dot(FixedVec2 o) const noexcept { return x * o.x + y * o.y; }
        constexpr Fixed lengthSq() const noexcept { return x * x + y * y; }

        constexpr bool operator==(const FixedVec2&) const noexcept = default;
    };

    // tileSize is world units per grid tile; pass GridTransform's tile size at the
    // call site so this header stays free of world configuration.
    constexpr path::GridPos worldToGrid(FixedVec2 world, int tileSize) noexcept {
        const Fixed size = Fixed::fromInt(tileSize);
        return path::GridPos {
            (world.x / size).toInt(),
            (world.y / size).toInt()
        };
    }

    constexpr FixedVec2 gridToWorldCenter(path::GridPos cell, int tileSize) noexcept {
        const Fixed half = Fixed::fromInt(tileSize) / Fixed::fromInt(2);
        return FixedVec2 {
            Fixed::fromInt(cell.x * tileSize) + half,
            Fixed::fromInt(cell.y * tileSize) + half
        };
    }

    // Compile-time self-checks: these double as the unit tests for the foundation.
    static_assert(Fixed::fromInt(3).toInt() == 3);
    static_assert((Fixed::fromInt(6) / Fixed::fromInt(2)).toInt() == 3);
    static_assert((Fixed::fromInt(3) * Fixed::fromInt(4)).toInt() == 12);
    static_assert((Fixed::fromInt(7) - Fixed::fromInt(10)).toInt() == -3);
    static_assert(worldToGrid(FixedVec2 { Fixed::fromInt(130), Fixed::fromInt(64) }, 64)
                  == path::GridPos { 2, 1 });
    static_assert(gridToWorldCenter(path::GridPos { 2, 1 }, 64)
                  == FixedVec2 { Fixed::fromInt(160), Fixed::fromInt(96) });
}
