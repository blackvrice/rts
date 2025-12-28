//
// Created by black on 25. 12. 27..
//
#include <platform/sfml/SfmlRenderManager.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>

namespace rts::platform::sfml {
    void SfmlRenderManager::execute(
        const core::render::RenderQueue &queue,
        const core::render::RenderContext &ctx
    ) {
        auto *window = static_cast<sf::RenderWindow *>(ctx.nativeHandle);
        if (!window) return;

        for (const auto &cmd: queue.commands()) {
            std::visit(
                [&](auto &&data) {
                    draw(window, data);
                },
                cmd.data
            );
        }
    }

    void SfmlRenderManager::draw(sf::RenderWindow *window, const core::render::DrawRect &r) {
        sf::RectangleShape rect({r.w, r.h});
        rect.setPosition({r.x, r.y});
        rect.setFillColor(sf::Color(r.color));
        window->draw(rect);
    }

    void SfmlRenderManager::draw(
        sf::RenderWindow *window,
        const core::render::DrawText &r
    ) {
        if (!window) return;

        // TODO: ResourceManager로 교체 예정
        static sf::Font defaultFont("assets/fonts/default.ttf");
        static bool fontLoaded = false;

        if (!fontLoaded) {
            fontLoaded = true;
        }

        sf::Text text(defaultFont);
        text.setString(r.text);
        text.setCharacterSize(r.size);
        text.setPosition({r.x, r.y});

        // ARGB → SFML Color
        sf::Color color(
            (r.color >> 16) & 0xFF, // R
            (r.color >> 8) & 0xFF, // G
            (r.color) & 0xFF, // B
            (r.color >> 24) & 0xFF // A
        );
        text.setFillColor(color);

        window->draw(text);
    }

    void SfmlRenderManager::draw(
        sf::RenderWindow *window,
        const core::render::DrawSprite &r
    ) {
        if (!window) return;

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

        window->draw(sprite);
    }
}
