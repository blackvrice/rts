//
// Created by black on 26. 1. 2..
//

#include <SFML/Graphics/Font.hpp>

#include "core/font/FontMetrics.hpp"

namespace rts::platform::sfml {
    class SfmlFontMetrics final
        : public core::font::FontMetrics {
    public:
        float advance(char32_t cp, core::font::FontId id, int size) const override
        {
            const sf::Font* f =
                core::font::FontManager::instance().getNative<sf::Font>(id);

            if (!f)
                return 0.f; // 🔥 절대 크래시 나면 안 됨

            return f->getGlyph(cp, size, false).advance;
        }

        float kerning(char32_t a, char32_t b,
                      core::font::FontId id, int size) const override
        {
            const sf::Font* f =
                core::font::FontManager::instance().getNative<sf::Font>(id);

            if (!f)
                return 0.f;   // 🔥 크래시 방지

            return f->getKerning(a, b, size);
        }

        float lineHeight(core::font::FontId id, int size) const override {
            const sf::Font* f =
                core::font::FontManager::instance().getNative<sf::Font>(id);
            return f->getLineSpacing(size);
        }
    };
}
