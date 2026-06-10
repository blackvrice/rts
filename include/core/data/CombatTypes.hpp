#pragma once

namespace rts::core::data {
    // Damage class of an attack.
    enum class WeaponType { Normal, Pierce, Siege, Magic };
    // Defensive class of a target (units and buildings).
    enum class ArmorType { Unarmored, Light, Heavy, Fortified };

    // Area-of-effect zones (world units) around an impact: full damage within
    // inner, half within mid, quarter within outer. outer == 0 means no splash.
    struct SplashRadii {
        float inner { 0.0f };
        float mid { 0.0f };
        float outer { 0.0f };

        bool any() const noexcept { return outer > 0.0f; }
    };

    // Damage fraction at a given distance from the splash center.
    constexpr float splashFalloff(const float distance, const SplashRadii& s) noexcept {
        if (distance <= s.inner) return 1.00f;
        if (distance <= s.mid)   return 0.50f;
        if (distance <= s.outer) return 0.25f;
        return 0.0f;
    }

    // WeaponType x ArmorType effectiveness multiplier applied to attack damage.
    // Warcraft-style: pierce shreds light, siege batters fortified (buildings),
    // normal is weak into fortified, magic favors heavy.
    constexpr float damageMultiplier(const WeaponType weapon, const ArmorType armor) noexcept {
        switch (weapon) {
            case WeaponType::Normal:
                switch (armor) {
                    case ArmorType::Unarmored: return 1.00f;
                    case ArmorType::Light:     return 1.00f;
                    case ArmorType::Heavy:     return 1.00f;
                    case ArmorType::Fortified: return 0.70f;
                }
                break;
            case WeaponType::Pierce:
                switch (armor) {
                    case ArmorType::Unarmored: return 1.00f;
                    case ArmorType::Light:     return 1.50f;  // strong vs light
                    case ArmorType::Heavy:     return 0.75f;
                    case ArmorType::Fortified: return 0.35f;
                }
                break;
            case WeaponType::Siege:
                switch (armor) {
                    case ArmorType::Unarmored: return 1.00f;
                    case ArmorType::Light:     return 1.00f;
                    case ArmorType::Heavy:     return 1.00f;
                    case ArmorType::Fortified: return 1.50f;  // strong vs buildings
                }
                break;
            case WeaponType::Magic:
                switch (armor) {
                    case ArmorType::Unarmored: return 1.00f;
                    case ArmorType::Light:     return 1.25f;
                    case ArmorType::Heavy:     return 1.50f;  // strong vs heavy
                    case ArmorType::Fortified: return 0.50f;
                }
                break;
        }
        return 1.00f;
    }
}
