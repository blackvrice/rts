//
// Created by black on 25. 12. 6..
//
// include/rts/presenter/SelectCommand.hpp
#pragma once

#include "core/Command.hpp"
#include <SFML/Graphics/Rect.hpp>

namespace rts::presenter {

    class SelectCommand : public rts::core::Command {
    public:
        explicit SelectCommand(const sf::FloatRect& area)
            : m_area(area) {}

        void execute(rts::manager::GameManager& game) override;

    private:
        sf::FloatRect m_area;
    };

} // namespace rts::presenter
