//
// Created by black on 25. 12. 27..
//
#include <platform/sfml/SfmlRenderManager.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
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
#include <numbers>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <SFML/Graphics/Image.hpp>

#include "core/data/DataRegistry.hpp"
#include "core/font/FontManager.hpp"
#include "core/manager/CameraManager.hpp"
#include "core/command/AudioCommand.hpp"
#include "core/command/AudioCommandBus.hpp"
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

    rts::core::render::UpdateMinimap hudMinimap(
        const rts::core::render::RenderQueue& queue
    ) {
        for (const auto& command : queue.commands()) {
            const auto* minimap = std::get_if<rts::core::render::UpdateMinimap>(&command.data);
            if (minimap) {
                return *minimap;
            }
        }

        return {};
    }

    // True when the frame carries gameplay HUD data; non-game scenes (e.g. lobby)
    // push no HUD commands, so the bottom console must not be drawn over them.
    bool queueHasHud(const rts::core::render::RenderQueue& queue) {
        for (const auto& command : queue.commands()) {
            if (std::get_if<rts::core::render::UpdateHudSelection>(&command.data) ||
                std::get_if<rts::core::render::UpdateHudResources>(&command.data)) {
                return true;
            }
        }
        return false;
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
    SfmlRenderManager::SfmlRenderManager(core::command::UICommandBus& uiBus,
                                         core::command::AudioCommandBus& audioBus)
        : m_hud(std::make_unique<SfmlHudOverlay>(uiBus)), m_audioBus(audioBus) {
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
        const auto minimap = hudMinimap(queue);
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

        // Only the gameplay scene draws the bottom command HUD; the lobby/menu pushes
        // no HUD data and renders its own full-screen UI instead.
        if (queueHasHud(queue)) {
            m_hud->render(*sfWindow, resources, selection, minimap);
            if (selection.selectedCount == 1) {
                drawSelectedHudSprite(*sfWindow, selectedHudUnit);
            }
        }
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
            const float baseX = r.x + (r.w - drawW) * 0.5f;
            const float baseY = r.y + (r.h - drawH);

            if (r.flipX) {
                // Negative X scale mirrors around the origin; shift right by drawW to
                // keep the sprite in the same on-screen box (face left).
                sprite.setScale({-scale, scale});
                sprite.setPosition({baseX + drawW, baseY});
            } else {
                sprite.setScale({scale, scale});
                sprite.setPosition({baseX, baseY});
            }
        } else {
            const float sx = r.w / static_cast<float>(sourceRect.size.x);
            const float sy = r.h / static_cast<float>(sourceRect.size.y);
            if (r.flipX) {
                sprite.setScale({-sx, sy});
                sprite.setPosition({r.x + r.w, r.y});
            } else {
                sprite.setScale({sx, sy});
                sprite.setPosition({r.x, r.y});
            }
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
        sf::RenderWindow& window,
        const core::render::DrawFog& fog
    ) {
        if (fog.tileSize <= 0.0f || fog.fogW <= 0 || fog.fogH <= 0) {
            return;
        }
        const std::size_t expected =
            static_cast<std::size_t>(fog.fogW) * static_cast<std::size_t>(fog.fogH);
        if (fog.states.size() < expected) {
            return;
        }

        // The window view is the world view at this point in the World-layer pass, so
        // the offscreen mask shares world coordinates and ends up screen-aligned.
        const sf::View worldView = window.getView();
        const sf::Vector2u winSize = window.getSize();
        if (winSize.x == 0 || winSize.y == 0) {
            return;
        }
        if (!m_fogTexture || m_fogTexture->getSize() != winSize) {
            m_fogTexture = std::make_unique<sf::RenderTexture>();
            if (!m_fogTexture->resize(winSize)) {
                m_fogTexture.reset();
                return;
            }
            m_fogTexture->setSmooth(true);
        }
        m_fogTexture->setView(worldView);
        m_fogTexture->clear(sf::Color::Transparent);

        // Shroud memory layer (tile resolution): unexplored = dark, everything seen
        // before (Explored/Visible) = dim. Only the camera-visible tiles are emitted.
        const float tile = fog.tileSize;
        const sf::Vector2f vc = worldView.getCenter();
        const sf::Vector2f vs = worldView.getSize();
        const int minX = std::max(0, static_cast<int>(std::floor((vc.x - vs.x * 0.5f) / tile)));
        const int minY = std::max(0, static_cast<int>(std::floor((vc.y - vs.y * 0.5f) / tile)));
        const int maxX = std::min(fog.fogW - 1, static_cast<int>(std::ceil((vc.x + vs.x * 0.5f) / tile)));
        const int maxY = std::min(fog.fogH - 1, static_cast<int>(std::ceil((vc.y + vs.y * 0.5f) / tile)));

        const sf::Color exploredColor(fog.exploredColor);
        const sf::Color unexploredColor(fog.unexploredColor);
        // FogOfWar::State: Unexplored = 0, Explored = 1, Visible = 2.
        sf::VertexArray shroud(sf::PrimitiveType::Triangles);
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const std::uint8_t state = fog.states[static_cast<std::size_t>(y) * fog.fogW + x];
                const sf::Color c = (state == 0) ? unexploredColor : exploredColor;
                const float x0 = x * tile, y0 = y * tile;
                const float x1 = x0 + tile, y1 = y0 + tile;
                shroud.append(sf::Vertex{ { x0, y0 }, c });
                shroud.append(sf::Vertex{ { x1, y0 }, c });
                shroud.append(sf::Vertex{ { x1, y1 }, c });
                shroud.append(sf::Vertex{ { x0, y0 }, c });
                shroud.append(sf::Vertex{ { x1, y1 }, c });
                shroud.append(sf::Vertex{ { x0, y1 }, c });
            }
        }
        if (shroud.getVertexCount() > 0) {
            m_fogTexture->draw(shroud);
        }

        // Punch the currently-visible circles out of the shroud. The reveal blend
        // multiplies the destination by (1 - src.alpha), so an opaque-cored circle
        // that feathers to zero at its rim clears the shroud with a soft, pixel-smooth
        // edge instead of the old tile staircase.
        const sf::RenderStates reveal{ sf::BlendMode{
            sf::BlendMode::Factor::Zero,
            sf::BlendMode::Factor::OneMinusSrcAlpha,
            sf::BlendMode::Equation::Add } };
        constexpr int kSegments = 48;
        constexpr float kCoreFraction = 0.78f;  // fully-clear inside, feather beyond
        const sf::Color opaque(255, 255, 255, 255);
        const sf::Color clear(255, 255, 255, 0);
        for (const auto& s : fog.sources) {
            if (s.radius <= 0.0f) {
                continue;
            }
            const float innerR = s.radius * kCoreFraction;

            // Solid core (triangle fan) clears the interior completely.
            sf::VertexArray core(sf::PrimitiveType::TriangleFan, kSegments + 2);
            core[0] = sf::Vertex{ { s.cx, s.cy }, opaque };
            for (int i = 0; i <= kSegments; ++i) {
                const float a = (2.0f * std::numbers::pi_v<float> * i) / kSegments;
                core[i + 1] = sf::Vertex{
                    { s.cx + innerR * std::cos(a), s.cy + innerR * std::sin(a) }, opaque };
            }
            m_fogTexture->draw(core, reveal);

            // Feather ring: opaque at innerR fading to clear at the full radius.
            sf::VertexArray ring(sf::PrimitiveType::TriangleStrip, (kSegments + 1) * 2);
            for (int i = 0; i <= kSegments; ++i) {
                const float a = (2.0f * std::numbers::pi_v<float> * i) / kSegments;
                const float cosA = std::cos(a), sinA = std::sin(a);
                ring[i * 2] = sf::Vertex{
                    { s.cx + innerR * cosA, s.cy + innerR * sinA }, opaque };
                ring[i * 2 + 1] = sf::Vertex{
                    { s.cx + s.radius * cosA, s.cy + s.radius * sinA }, clear };
            }
            m_fogTexture->draw(ring, reveal);
        }

        m_fogTexture->display();

        // Blit the mask over the world in screen space (it already holds the world
        // view's pixels), then restore the world view for any later world commands.
        window.setView(window.getDefaultView());
        window.draw(sf::Sprite(m_fogTexture->getTexture()));
        window.setView(worldView);
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

    void SfmlRenderManager::draw(
        sf::RenderWindow&,
        const core::render::PlaySound& sound
    ) {
        // Hand the cue to the audio thread instead of synthesizing/playing inline on
        // the main render thread; SfmlAudioManager owns the tone synthesis now.
        m_audioBus.push(std::make_unique<core::command::PlayCueCommand>(sound.cue, sound.volume));
    }

    void SfmlRenderManager::draw(
        sf::RenderWindow&,
        const core::render::UpdateMinimap&
    ) {
        // Minimap is drawn by the ImGui HUD overlay, not the world pass.
    }
}
