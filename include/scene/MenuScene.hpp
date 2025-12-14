//
// Created by black on 25. 12. 6..
//
#pragma once
#include <manager/GameManager.hpp>
#include <scene/Scene.hpp>
#include <SFML/Graphics.hpp>

namespace rts::scene {

    class MenuScene : public Scene {
    public:
        MenuScene(const std::shared_ptr<SceneManager> &mgr,
                  const std::shared_ptr<manager::GameManager> &game);

        void handleEvent(const sf::Event& ev) override;
        void update(float dt) override {}
        void render(sf::RenderTarget& target) override;

    private:
        sf::Font m_font;
        sf::Text m_title;
    };

} // namespace rts::scene
