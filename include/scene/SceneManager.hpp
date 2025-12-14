//
// Created by black on 25. 12. 6..
//

#pragma once
#include <vector>
#include <memory>
#include "Scene.hpp"

namespace rts::scene {

    class SceneManager {
    public:
        void push(std::shared_ptr<Scene> scene);

        void pop();

        void replace(std::shared_ptr<Scene> scene);

        void handleEvent(const sf::Event& ev);

        void update(float dt);

        void render(sf::RenderTarget& target);

    private:
        std::vector<std::shared_ptr<Scene>> m_scenes;
    };

} // namespace rts::scene
