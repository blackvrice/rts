// core/font/FontMetrics.hpp
#pragma once
#include <cstdint>

#include "FontTypes.hpp"

namespace rts::core::font {

    class FontMetrics {
    public:
        virtual ~FontMetrics() = default;

        virtual float advance(
            char32_t codepoint,
            FontId fontId,
            int fontSize
        ) const = 0;

        virtual float kerning(
            char32_t prev,
            char32_t curr,
            FontId fontId,
            int fontSize
        ) const {
            return 0.f;
        }

        virtual float lineHeight(
            FontId fontId,
            int fontSize
        ) const = 0;
    };

}
