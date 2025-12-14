//
// Created by black on 25. 12. 7..
//
#pragma once
#include <SFML/Graphics.hpp>

#include "ecs/Registry.hpp"
#include "component/TransformComponent.hpp"
#include "component/SpriteComponent.hpp"
#include "component/AnimationComponent.hpp"

namespace rts::system {
    class RendererSystem {
    public:
        explicit RendererSystem(ecs::Registry& registry);

        // TileMap 같은 Drawable 레이어 추가
        void addLayer(sf::Drawable* drawable);

        // 최종 렌더링
        void render(sf::RenderWindow& window, float dt);

    private:
        ecs::Registry& m_reg;
        std::vector<sf::Drawable*> m_layers;

    private:
        void updateAnimation(ecs::Entity e, float dt);
    };
}