//
// Created by black on 25. 12. 7..
//

#include <system/RendererSystem.hpp>
#include <algorithm>

namespace rts::system {
    RendererSystem::RendererSystem(ecs::Registry &registry)
        : m_reg(registry) {
    }

    void RendererSystem::addLayer(sf::Drawable *drawable) {
        m_layers.push_back(drawable);
    }

    void RendererSystem::updateAnimation(ecs::Entity e, float dt) {
        using namespace ecs;

        auto *anim = m_reg.get<component::AnimationComponent>(e);
        if (!anim) return;

        anim->timer += dt;

        if (anim->timer >= anim->frameTime) {
            anim->timer = 0.f;
            anim->frame = (anim->frame + 1) % anim->frameCount;

            auto *sprite = m_reg.get<component::SpriteComponent>(e);
            sprite->sprite.setTextureRect(sf::IntRect({anim->frame * anim->frameWidth, 0},
                                                      {
                                                          anim->frameWidth,
                                                          anim->frameHeight
                                                      }
            ));
        }
    }

    void RendererSystem::render(sf::RenderWindow &window, float dt) {
        using namespace ecs;

        // ① TileMap / Background 레이어 먼저 렌더
        for (auto *layer: m_layers)
            window.draw(*layer);

        // ② 엔티티 Sprite 수집
        struct DrawItem {
            float depth;
            sf::Sprite *sprite;
        };
        std::vector<DrawItem> queue;

        const auto &allEntities = m_reg.entities();

        for (Entity e: allEntities) {
            auto *transform = m_reg.get<component::TransformComponent>(e);
            auto *spriteComp = m_reg.get<component::SpriteComponent>(e);
            if (!transform || !spriteComp)
                continue;

            // 애니메이션 적용
            updateAnimation(e, dt);

            // 스프라이트 위치 세팅
            spriteComp->sprite.setPosition(transform->position);
            spriteComp->sprite.setRotation(sf::radians(transform->rotation));
            spriteComp->sprite.setScale(transform->scale);

            // RTS Depth Sorting (layer * 10000 + y)
            float depth = spriteComp->layer * 10000 + transform->position.y;

            queue.push_back({depth, &spriteComp->sprite});
        }

        // ③ Depth 정렬
        std::sort(queue.begin(), queue.end(),
                  [](auto &a, auto &b) {
                      return a.depth < b.depth;
                  });

        // ④ 렌더링
        for (auto &item: queue)
            window.draw(*item.sprite);
    }
}
