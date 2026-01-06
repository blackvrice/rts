//
// Created by black on 25. 12. 27..
//
#include <platform/sfml/SfmlRenderManager.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "core/font/FontManager.hpp"
#include "platform/sfml/SfmlWindow.hpp"

namespace rts::platform::sfml {
    void SfmlRenderManager::execute(
        const core::render::RenderQueue &queue,
        const core::render::RenderContext &ctx
    ) {
        auto& windowBase = ctx.window();

        auto* sfWindow = static_cast<sf::RenderWindow*>(windowBase.getNativeHandle());
        if (!sfWindow) return;

        for (const auto &cmd: queue.commands()) {
            std::visit(
                [&](auto &&data) {
                    draw(*sfWindow, data);
                },
                cmd.data
            );
        }
    }

    void SfmlRenderManager::draw(sf::RenderWindow &window, const core::render::DrawRect &r) {
        sf::RectangleShape rect({r.rect.width(), r.rect.height()});
        rect.setPosition({r.rect.left(), r.rect.top()});
        rect.setOutlineColor(sf::Color(r.border_color));
        rect.setOutlineThickness(1);
        rect.setFillColor(sf::Color(r.color));
        window.draw(rect);
    }

    void SfmlRenderManager::draw(
        sf::RenderWindow& window,
        const core::render::DrawText& r
    ) {
        using core::font::FontManager;

        const sf::Font* font =
            FontManager::instance().getNative<sf::Font>(r.fontId);

        // 🔥 폰트 없으면 fallback (절대 크래시 X)
        // if (!font)
        //     font = FontManager::instance().getNative<sf::Font>(core::font::FontId::Default);

        // 그래도 없으면 그냥 그리지 않음
        if (!font)
            return;

        sf::Text text(*font);
        text.setString(r.text);
        text.setCharacterSize(r.size);
        text.setPosition({r.pos.x, r.pos.y});

        // ARGB → SFML Color
        sf::Color color(r.color);
        text.setFillColor(color);

        window.draw(text);
    }

    void SfmlRenderManager::draw(
        sf::RenderWindow &window,
        const core::render::DrawSprite &r
    ) {
        // TODO: textureId → ResourceManager에서 가져오기
        static sf::Texture dummyTexture;
        static bool textureLoaded = false;

        if (!textureLoaded) {
            dummyTexture.loadFromFile("assets/textures/dummy.png");
            textureLoaded = true;
        }

        sf::Sprite sprite(dummyTexture);
        sprite.setTexture(dummyTexture);

        // 원본 텍스처 크기
        auto texSize = dummyTexture.getSize();

        sprite.setPosition({r.x, r.y});

        // 크기 스케일링
        sprite.setScale(
            {
                r.w / static_cast<float>(texSize.x),
                r.h / static_cast<float>(texSize.y)
            }
        );

        window.draw(sprite);
    }
}
