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
#include <SFML/Graphics/View.hpp>
#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>

#include <SFML/Graphics/Image.hpp>

#include "core/data/DataRegistry.hpp"
#include "core/font/FontManager.hpp"
#include "core/manager/CameraManager.hpp"
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderContext.hpp"
#include "core/render/RenderQueue.hpp"
#include "platform/sfml/SfmlAssetPaths.hpp"
#include "platform/sfml/SfmlHudOverlay.hpp"
#include "platform/sfml/SfmlWindow.hpp"

namespace {
    constexpr int kWorldTileSize = 64;
    // Fallback tileset path used only when data/animations.json lacks "world.tileset".
    constexpr const char* kWorldTilesetPath = "Terrain/Tileset/Tilemap_color1.png";

    struct SpriteTrimCacheKey {
        const sf::Texture* texture;
        int x;
        int y;
        int w;
        int h;

        bool operator==(const SpriteTrimCacheKey& other) const {
            return texture == other.texture &&
                   x == other.x &&
                   y == other.y &&
                   w == other.w &&
                   h == other.h;
        }
    };

    struct SpriteTrimCacheKeyHash {
        std::size_t operator()(const SpriteTrimCacheKey& key) const {
            std::size_t result = std::hash<const sf::Texture*>{}(key.texture);
            const auto combine = [&result](const std::size_t value) {
                result ^= value + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            };
            combine(std::hash<int>{}(key.x));
            combine(std::hash<int>{}(key.y));
            combine(std::hash<int>{}(key.w));
            combine(std::hash<int>{}(key.h));
            return result;
        }
    };

    std::unordered_map<const sf::Texture*, sf::Image> g_spriteTrimImages;
    std::unordered_map<SpriteTrimCacheKey, sf::IntRect, SpriteTrimCacheKeyHash> g_spriteTrimCache;

    const sf::Texture* tinySwordsWorldTileset() {
        static sf::Texture texture;
        static bool attemptedLoad = false;
        static bool loaded = false;

        if (!attemptedLoad) {
            attemptedLoad = true;
            texture.setSmooth(false);

            // Tileset path is data-driven via the "world.tileset" sprite entry;
            // fall back to the built-in path when it is absent.
            std::string relativePath = kWorldTilesetPath;
            if (const auto* clip = rts::core::data::DataRegistry::global().sprite("world.tileset");
                clip && !clip->texture.empty()) {
                relativePath = clip->texture;
            }

            const std::filesystem::path path =
                std::filesystem::path(rts::platform::sfml::TinySwordsRoot) / relativePath;
            loaded = texture.loadFromFile(path.string());
        }

        return loaded ? &texture : nullptr;
    }

    const sf::Image& spriteTrimImageFor(const sf::Texture& texture) {
        if (const auto it = g_spriteTrimImages.find(&texture); it != g_spriteTrimImages.end()) {
            return it->second;
        }

        auto [it, _] = g_spriteTrimImages.emplace(&texture, texture.copyToImage());
        return it->second;
    }

    sf::IntRect trimTransparentSourceRect(const sf::Texture& texture, const sf::IntRect& sourceRect) {
        const auto textureSize = texture.getSize();
        const int x0 = std::clamp(sourceRect.position.x, 0, static_cast<int>(textureSize.x));
        const int y0 = std::clamp(sourceRect.position.y, 0, static_cast<int>(textureSize.y));
        const int x1 = std::clamp(sourceRect.position.x + sourceRect.size.x, x0, static_cast<int>(textureSize.x));
        const int y1 = std::clamp(sourceRect.position.y + sourceRect.size.y, y0, static_cast<int>(textureSize.y));

        if (x0 >= x1 || y0 >= y1) {
            return sourceRect;
        }

        const SpriteTrimCacheKey key{&texture, x0, y0, x1 - x0, y1 - y0};
        if (const auto it = g_spriteTrimCache.find(key); it != g_spriteTrimCache.end()) {
            return it->second;
        }

        const sf::Image& image = spriteTrimImageFor(texture);
        const auto imageSize = image.getSize();
        const std::uint8_t* pixels = image.getPixelsPtr();

        int minX = x1;
        int minY = y1;
        int maxX = x0 - 1;
        int maxY = y0 - 1;

        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const std::size_t alphaIndex =
                    (static_cast<std::size_t>(y) * imageSize.x + static_cast<std::size_t>(x)) * 4U + 3U;
                if (pixels[alphaIndex] == 0U) {
                    continue;
                }

                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }

        const sf::IntRect trimmed = maxX < minX || maxY < minY
            ? sourceRect
            : sf::IntRect({minX, minY}, {maxX - minX + 1, maxY - minY + 1});
        g_spriteTrimCache.emplace(key, trimmed);
        return trimmed;
    }

    float animationSeconds() {
        static const auto start = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<float>(elapsed).count();
    }

    sf::IntRect animatedSourceRect(const sf::Texture& texture, const rts::core::render::DrawSprite& r) {
        if (r.sourceW <= 0 || r.sourceH <= 0) {
            return sf::IntRect({0, 0}, {
                static_cast<int>(texture.getSize().x),
                static_cast<int>(texture.getSize().y)
            });
        }

        const auto textureSize = texture.getSize();
        const int availableFrames = std::max(
            1,
            (static_cast<int>(textureSize.x) - r.sourceX) / r.sourceW
        );
        const int frameCount = std::clamp(r.frameCount, 1, availableFrames);
        const int frameIndex = r.framesPerSecond > 0.0f
            ? static_cast<int>(std::floor(animationSeconds() * r.framesPerSecond)) % frameCount
            : 0;

        // Tiny Swords unit sheets are laid out as a single horizontal row of 192px frames.
        return sf::IntRect(
            {r.sourceX + frameIndex * r.sourceW, r.sourceY},
            {r.sourceW, r.sourceH}
        );
    }

    std::optional<rts::core::render::DrawSprite> selectedHudSprite(
        const rts::core::render::RenderQueue& queue
    ) {
        for (const auto& command : queue.commands()) {
            if (command.layer != rts::core::render::RenderLayer::World) {
                continue;
            }

            const auto* sprite = std::get_if<rts::core::render::DrawSprite>(&command.data);
            if (sprite && sprite->showInHud) {
                return *sprite;
            }
        }

        return std::nullopt;
    }

    rts::core::model::PlayerResourceState hudResources(
        const rts::core::render::RenderQueue& queue
    ) {
        for (const auto& command : queue.commands()) {
            const auto* resources = std::get_if<rts::core::render::UpdateHudResources>(&command.data);
            if (resources) {
                return resources->resources;
            }
        }

        return {};
    }

    rts::core::render::UpdateHudSelection hudSelection(
        const rts::core::render::RenderQueue& queue
    ) {
        for (const auto& command : queue.commands()) {
            const auto* selection = std::get_if<rts::core::render::UpdateHudSelection>(&command.data);
            if (selection) {
                return *selection;
            }
        }

        return {};
    }

    std::string hudCursor(
        const rts::core::render::RenderQueue& queue
    ) {
        std::string cursorKey = "default";
        for (const auto& command : queue.commands()) {
            const auto* cursor = std::get_if<rts::core::render::UpdateHudCursor>(&command.data);
            if (cursor) {
                cursorKey = cursor->cursorKey;
            }
        }
        return cursorKey;
    }

    // Data-driven loader: resolves a texture by its asset-relative path, cached
    // by path. Every sprite/cursor/tileset texture is loaded through here.
    const sf::Texture* tinySwordsTextureByPath(const std::string& relativePath) {
        static std::unordered_map<std::string, std::unique_ptr<sf::Texture>> textures;

        if (const auto it = textures.find(relativePath); it != textures.end()) {
            return it->second.get();
        }

        auto texture = std::make_unique<sf::Texture>();
        texture->setSmooth(false);
        const std::filesystem::path path =
            std::filesystem::path(rts::platform::sfml::TinySwordsRoot) / relativePath;
        if (!texture->loadFromFile(path.string())) {
            textures.emplace(relativePath, nullptr);  // cache the miss
            return nullptr;
        }

        const sf::Texture* result = texture.get();
        textures.emplace(relativePath, std::move(texture));
        return result;
    }

    const sf::Texture* spriteTextureFor(const rts::core::render::DrawSprite& r) {
        return r.texturePath.empty() ? nullptr : tinySwordsTextureByPath(r.texturePath);
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

    void drawTinySwordsTileGrid(sf::RenderWindow& window)
    {
        const sf::Texture* tileset = tinySwordsWorldTileset();
        if (!tileset)
        {
            return;
        }

        constexpr int tileSize = 64;

        const sf::View view = window.getView();
        const sf::Vector2f center = view.getCenter();
        const sf::Vector2f size = view.getSize();
        const float left = center.x - size.x * 0.5f;
        const float top = center.y - size.y * 0.5f;
        const float right = center.x + size.x * 0.5f;
        const float bottom = center.y + size.y * 0.5f;

        const int startColumn = static_cast<int>(std::floor(left / tileSize)) - 1;
        const int endColumn = static_cast<int>(std::ceil(right / tileSize)) + 1;
        const int startRow = static_cast<int>(std::floor(top / tileSize)) - 1;
        const int endRow = static_cast<int>(std::ceil(bottom / tileSize)) + 1;

        sf::Sprite tile(*tileset);

        // Tilemap_color1.png 기준
        // 10번 타일 = x: 1칸, y: 1칸 위치의 64x64 잔디 내부 타일
        const sf::IntRect grassTileRect(
            {1 * tileSize, 1 * tileSize},
            {tileSize, tileSize}
        );

        tile.setTextureRect(grassTileRect);

        for (int y = startRow; y <= endRow; ++y)
        {
            for (int x = startColumn; x <= endColumn; ++x)
            {
                tile.setPosition({
                    static_cast<float>(x * tileSize),
                    static_cast<float>(y * tileSize)
                });

                window.draw(tile);
            }
        }

        // 디버그용 격자선
        sf::RectangleShape line;
        line.setFillColor(sf::Color(28, 43, 36, 96));

        line.setSize({1.0f, bottom - top});
        for (int x = startColumn; x <= endColumn; ++x)
        {
            line.setPosition({static_cast<float>(x * tileSize), top});
            window.draw(line);
        }

        line.setSize({right - left, 1.0f});
        for (int y = startRow; y <= endRow; ++y)
        {
            line.setPosition({left, static_cast<float>(y * tileSize)});
            window.draw(line);
        }
    }

    void drawMouseCursor(sf::RenderWindow& window, const std::string& cursorKey) {
        // Cursor texture resolved from data ("cursor.<key>" in animations.json).
        const auto* clip = rts::core::data::DataRegistry::global().sprite("cursor." + cursorKey);
        const sf::Texture* texture = (clip && !clip->texture.empty())
            ? tinySwordsTextureByPath(clip->texture)
            : nullptr;
        if (!texture) {
            return;
        }

        const sf::Vector2i mouse = sf::Mouse::getPosition(window);
        sf::Sprite cursor(*texture);
        cursor.setTextureRect(sf::IntRect({0, 0}, {64, 64}));
        cursor.setScale({0.5f, 0.5f});

        // Cursor_01's arrow tip sits inside transparent padding, so offset it to the mouse hotspot.
        cursor.setPosition({
            static_cast<float>(mouse.x) - 11.0f,
            static_cast<float>(mouse.y) - 9.0f
        });

        window.draw(cursor);
    }

    void drawSelectedHudSprite(
        sf::RenderWindow& window,
        const std::optional<rts::core::render::DrawSprite>& selected
    ) {
        if (!selected) {
            return;
        }

        const sf::Texture* texture = spriteTextureFor(*selected);
        if (!texture) {
            return;
        }

        const sf::Vector2u windowSize = window.getSize();
        const float width = std::max(static_cast<float>(windowSize.x), 1280.0f);
        const float height = std::max(static_cast<float>(windowSize.y), 720.0f);
        const float bottomHeight = std::clamp(height * 0.24f, 220.0f, 270.0f);
        const float margin = 16.0f;
        const float bottomY = height - bottomHeight;
        const float miniWidth = std::clamp(width * 0.18f, 285.0f, 340.0f);
        const float commandWidth = std::clamp(width * 0.22f, 360.0f, 420.0f);

        const sf::Vector2f miniMin{margin, bottomY + margin};
        const sf::Vector2f miniMax{miniMin.x + miniWidth, height - margin};
        const sf::Vector2f commandMin{width - commandWidth - margin, bottomY + margin};
        const sf::Vector2f statusMin{miniMax.x + margin, bottomY + margin};
        const sf::Vector2f statusMax{commandMin.x - margin, height - margin};
        const sf::Vector2f portraitMin{statusMin.x + 18.0f, statusMin.y + 44.0f};
        const sf::Vector2f portraitMax{portraitMin.x + 132.0f, statusMax.y - 18.0f};
        const sf::Vector2f innerMin{portraitMin.x + 14.0f, portraitMin.y + 12.0f};
        const sf::Vector2f innerMax{portraitMax.x - 14.0f, portraitMax.y - 10.0f};

        sf::IntRect sourceRect = animatedSourceRect(*texture, *selected);
        if (selected->trimTransparent) {
            sourceRect = trimTransparentSourceRect(*texture, sourceRect);
        }

        if (sourceRect.size.x <= 0 || sourceRect.size.y <= 0) {
            return;
        }

        const float innerW = innerMax.x - innerMin.x;
        const float innerH = innerMax.y - innerMin.y;
        const float scale = std::min(
            innerW / static_cast<float>(sourceRect.size.x),
            innerH / static_cast<float>(sourceRect.size.y)
        );
        const float drawW = static_cast<float>(sourceRect.size.x) * scale;
        const float drawH = static_cast<float>(sourceRect.size.y) * scale;

        sf::RectangleShape backing({innerW, innerH});
        backing.setPosition(innerMin);
        backing.setFillColor(sf::Color(5, 10, 15, 230));
        window.draw(backing);

        sf::Sprite sprite(*texture);
        sprite.setTextureRect(sourceRect);
        // HUD portrait reuses the selected unit's current frame so action state matches the world sprite.
        sprite.setScale({scale, scale});
        sprite.setPosition({
            innerMin.x + (innerW - drawW) * 0.5f,
            innerMax.y - drawH
        });
        window.draw(sprite);
    }
}

namespace rts::platform::sfml {
    SfmlRenderManager::SfmlRenderManager(core::command::UICommandBus& uiBus)
        : m_hud(std::make_unique<SfmlHudOverlay>(uiBus)) {
    }

    SfmlRenderManager::~SfmlRenderManager() = default;

    void SfmlRenderManager::execute(
        const core::render::RenderQueue &queue,
        const core::render::RenderContext &ctx
    ) {
        auto& windowBase = ctx.window();

        auto* sfWindow = static_cast<sf::RenderWindow*>(windowBase.getNativeHandle());
        if (!sfWindow) return;

        auto& camera = ctx.camera();
        const auto windowSize = sfWindow->getSize();
        camera.setViewportSize({
            static_cast<float>(windowSize.x),
            static_cast<float>(windowSize.y)
        });

        const sf::View defaultView = sfWindow->getView();
        const auto selectedHudUnit = selectedHudSprite(queue);
        const auto resources = hudResources(queue);
        const auto selection = hudSelection(queue);
        const auto cursorKey = hudCursor(queue);
        const auto cameraPosition = camera.position();
        sf::View worldView;
        worldView.setSize({
            static_cast<float>(windowSize.x),
            static_cast<float>(windowSize.y)
        });
        worldView.setCenter({
            cameraPosition.x + static_cast<float>(windowSize.x) * 0.5f,
            cameraPosition.y + static_cast<float>(windowSize.y) * 0.5f
        });

        sfWindow->setView(worldView);
        drawTinySwordsTileGrid(*sfWindow);

        for (const auto &cmd: queue.commands()) {
            if (cmd.layer != core::render::RenderLayer::World) {
                continue;
            }

            std::visit(
                [&](auto &&data) {
                    draw(*sfWindow, data);
                },
                cmd.data
            );
        }

        sfWindow->setView(defaultView);
        for (const auto &cmd: queue.commands()) {
            if (cmd.layer == core::render::RenderLayer::World) {
                continue;
            }

            std::visit(
                [&](auto &&data) {
                    draw(*sfWindow, data);
                },
                cmd.data
            );
        }

        m_hud->render(*sfWindow, resources, selection);
        drawSelectedHudSprite(*sfWindow, selectedHudUnit);
        drawMouseCursor(*sfWindow, cursorKey);
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
        const sf::Texture* texture = spriteTextureFor(r);
        if (!texture) {
            return;
        }

        if (r.w <= 0.0f || r.h <= 0.0f) {
            return;
        }

        sf::Sprite sprite(*texture);
        sf::IntRect sourceRect = animatedSourceRect(*texture, r);
        sprite.setTextureRect(sourceRect);
        if (r.trimTransparent) {
            sourceRect = trimTransparentSourceRect(*texture, sourceRect);
            sprite.setTextureRect(sourceRect);
        }

        if (sourceRect.size.x <= 0 || sourceRect.size.y <= 0) {
            return;
        }

        if (r.rotation != 0.f) {
            sprite.setOrigin({
                static_cast<float>(sourceRect.size.x) * 0.5f,
                static_cast<float>(sourceRect.size.y) * 0.5f
            });
            sprite.setPosition({
                r.x + r.w * 0.5f,
                r.y + r.h * 0.5f
            });
            sprite.setScale({
                r.w / static_cast<float>(sourceRect.size.x),
                r.h / static_cast<float>(sourceRect.size.y)
            });
            sprite.setRotation(sf::degrees(r.rotation));
        } else if (r.trimTransparent) {
            // Trimmed unit frames stay bottom-centered so the model position can remain at the feet.
            const float scale = std::min(
                r.w / static_cast<float>(sourceRect.size.x),
                r.h / static_cast<float>(sourceRect.size.y)
            );
            const float drawW = static_cast<float>(sourceRect.size.x) * scale;
            const float drawH = static_cast<float>(sourceRect.size.y) * scale;

            sprite.setPosition({
                r.x + (r.w - drawW) * 0.5f,
                r.y + (r.h - drawH)
            });
            sprite.setScale({scale, scale});
        } else {
            sprite.setPosition({r.x, r.y});
            sprite.setScale(
                {
                    r.w / static_cast<float>(sourceRect.size.x),
                    r.h / static_cast<float>(sourceRect.size.y)
                }
            );
        }

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

    void SfmlRenderManager::draw(
        sf::RenderWindow&,
        const core::render::UpdateHudResources&
    ) {
    }

    void SfmlRenderManager::draw(
        sf::RenderWindow&,
        const core::render::UpdateHudSelection&
    ) {
    }

    void SfmlRenderManager::draw(
        sf::RenderWindow&,
        const core::render::UpdateHudCursor&
    ) {
    }
}
