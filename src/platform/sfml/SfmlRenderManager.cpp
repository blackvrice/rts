//
// Created by black on 25. 12. 27..
//
#include <platform/sfml/SfmlRenderManager.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <unordered_map>

#include "core/font/FontManager.hpp"
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderContext.hpp"
#include "core/render/RenderQueue.hpp"
#include "platform/sfml/SfmlAssetPaths.hpp"
#include "platform/sfml/SfmlHudOverlay.hpp"
#include "platform/sfml/SfmlWindow.hpp"

namespace {
    constexpr int kWorldTileSize = 64;
    constexpr const char* kWorldTilesetPath = "Terrain/Tileset/Tilemap_color1.png";
    constexpr int kBlueWarriorIdleTextureId = 1;
    constexpr const char* kBlueWarriorIdlePath = "Units/Blue Units/Warrior/Warrior_Idle.png";

    const sf::Texture* tinySwordsWorldTileset() {
        static sf::Texture texture;
        static bool attemptedLoad = false;
        static bool loaded = false;

        if (!attemptedLoad) {
            attemptedLoad = true;
            texture.setSmooth(false);

            const std::filesystem::path path =
                std::filesystem::path(rts::platform::sfml::TinySwordsRoot) / kWorldTilesetPath;
            loaded = texture.loadFromFile(path.string());
        }

        return loaded ? &texture : nullptr;
    }

    const sf::Texture* tinySwordsSpriteTexture(const int textureId) {
        static std::unordered_map<int, std::unique_ptr<sf::Texture>> textures;

        if (const auto it = textures.find(textureId); it != textures.end()) {
            return it->second.get();
        }

        const char* relativePath = nullptr;
        switch (textureId) {
            case kBlueWarriorIdleTextureId:
                relativePath = kBlueWarriorIdlePath;
                break;
            default:
                return nullptr;
        }

        auto texture = std::make_unique<sf::Texture>();
        texture->setSmooth(false);

        const std::filesystem::path path =
            std::filesystem::path(rts::platform::sfml::TinySwordsRoot) / relativePath;
        if (!texture->loadFromFile(path.string())) {
            return nullptr;
        }

        const sf::Texture* result = texture.get();
        textures.emplace(textureId, std::move(texture));
        return result;
    }
    sf::IntRect tileSourceRect(const sf::Texture& texture, const int tileIndex) {
        const auto textureSize = texture.getSize();
        const int columns = static_cast<int>(textureSize.x) / kWorldTileSize;
        const int safeColumns = std::max(columns, 1);
        const int x = tileIndex % safeColumns;
        const int y = tileIndex / safeColumns;

        return sf::IntRect(
            {x * kWorldTileSize, y * kWorldTileSize},
            {kWorldTileSize, kWorldTileSize}
        );
    }

    void drawTinySwordsTileGrid(sf::RenderWindow& window) {
        const sf::Texture* tileset = tinySwordsWorldTileset();
        if (!tileset) {
            return;
        }

        const auto windowSize = window.getSize();
        const int columns = static_cast<int>((windowSize.x + kWorldTileSize - 1) / kWorldTileSize);
        const int rows = static_cast<int>((windowSize.y + kWorldTileSize - 1) / kWorldTileSize);

        // Tilemap_color1.png은 64px 타일 시트라서 잔디/흙 변형을 반복 배치한다.
        constexpr std::array<int, 8> tilePattern{0, 1, 2, 9, 10, 11, 18, 19};
        sf::Sprite tile(*tileset);

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < columns; ++x) {
                const int patternIndex = (x * 3 + y * 5 + (x + y) % 2) % static_cast<int>(tilePattern.size());
                tile.setTextureRect(tileSourceRect(*tileset, tilePattern[patternIndex]));
                tile.setPosition({
                    static_cast<float>(x * kWorldTileSize),
                    static_cast<float>(y * kWorldTileSize)
                });
                window.draw(tile);
            }
        }

        sf::RectangleShape line;
        line.setFillColor(sf::Color(28, 43, 36, 96));

        line.setSize({1.0f, static_cast<float>(windowSize.y)});
        for (int x = 0; x <= columns; ++x) {
            line.setPosition({static_cast<float>(x * kWorldTileSize), 0.0f});
            window.draw(line);
        }

        line.setSize({static_cast<float>(windowSize.x), 1.0f});
        for (int y = 0; y <= rows; ++y) {
            line.setPosition({0.0f, static_cast<float>(y * kWorldTileSize)});
            window.draw(line);
        }
    }
}

namespace rts::platform::sfml {
    SfmlRenderManager::SfmlRenderManager()
        : m_hud(std::make_unique<SfmlHudOverlay>()) {
    }

    SfmlRenderManager::~SfmlRenderManager() = default;

    void SfmlRenderManager::execute(
        const core::render::RenderQueue &queue,
        const core::render::RenderContext &ctx
    ) {
        auto& windowBase = ctx.window();

        auto* sfWindow = static_cast<sf::RenderWindow*>(windowBase.getNativeHandle());
        if (!sfWindow) return;

        drawTinySwordsTileGrid(*sfWindow);

        for (const auto &cmd: queue.commands()) {
            std::visit(
                [&](auto &&data) {
                    draw(*sfWindow, data);
                },
                cmd.data
            );
        }

        m_hud->render(*sfWindow);
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
        const sf::Texture* texture = tinySwordsSpriteTexture(r.textureId);
        if (!texture) {
            return;
        }

        sf::Sprite sprite(*texture);
        if (r.sourceW > 0 && r.sourceH > 0) {
            sprite.setTextureRect(sf::IntRect(
                {r.sourceX, r.sourceY},
                {r.sourceW, r.sourceH}
            ));
        }

        const sf::IntRect sourceRect = sprite.getTextureRect();
        if (sourceRect.size.x <= 0 || sourceRect.size.y <= 0) {
            return;
        }

        sprite.setPosition({r.x, r.y});
        sprite.setScale(
            {
                r.w / static_cast<float>(sourceRect.size.x),
                r.h / static_cast<float>(sourceRect.size.y)
            }
        );

        window.draw(sprite);
    }

    void SfmlRenderManager::draw(
        sf::RenderWindow& window,
        const core::render::DrawCircle& c
    ) {
        sf::CircleShape shape(c.radius);

        // 중심 기준으로 위치 잡기
        shape.setOrigin({c.radius, c.radius});
        shape.setPosition({c.cx, c.cy});

        // 내부 색
        shape.setFillColor(sf::Color(
            (c.color >> 16) & 0xFF,
            (c.color >> 8)  & 0xFF,
            (c.color)       & 0xFF,
            (c.color >> 24) & 0xFF
        ));

        // 테두리
        shape.setOutlineColor(sf::Color(
            (c.border_color >> 16) & 0xFF,
            (c.border_color >> 8)  & 0xFF,
            (c.border_color)       & 0xFF,
            (c.border_color >> 24) & 0xFF
        ));
        shape.setOutlineThickness(1.f);

        window.draw(shape);
    }
}
