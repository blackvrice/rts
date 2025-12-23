//
// Created by black on 25. 12. 6..
//

// include/rts/view/RenderLoop.hpp
#pragma once
#include <memory>
#include <SFML/Graphics.hpp>
#include "../manager/SceneManager.hpp"

namespace rts::view {

    class RenderLoop {
    public:
        RenderLoop(std::shared_ptr<scene::SceneManager> mgr);

        void run();

    private:
        std::shared_ptr<scene::SceneManager> m_mgr;
        sf::RenderWindow m_window;
    };

} // namespace rts::view
