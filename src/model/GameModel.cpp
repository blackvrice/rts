//
// Created by black on 25. 12. 6..
//
// src/model/GameModel.cpp
#include "model/GameModel.hpp"
#include "component/Position.hpp"
#include "component/RenderInfo.hpp"
#include "component/Selection.hpp"

namespace rts::model {

    Snapshot GameModel::buildSnapshot() const
    {
        Snapshot snap;
        auto& reg = m_registry;
        auto& posStorage  = const_cast<rts::ecs::Registry &>(reg).storage<rts::component::Position>();
        auto& rendStorage = const_cast<rts::ecs::Registry &>(reg).storage<rts::component::RenderInfo>();
        auto& selStorage  = const_cast<rts::ecs::Registry &>(reg).storage<rts::component::Selection>();

        for (auto& [entity, pos] : posStorage.data()) {
            UnitSnapshot view{};
            view.position = pos.value;

            if (auto* info = rendStorage.get(entity)) {
                view.color  = info->color;
                view.radius = info->radius;
            }
            if (auto* sel = selStorage.get(entity)) {
                view.selected = sel->selected;
                if (sel->selected)
                    view.color = sf::Color::Green;
            }

            snap.units.push_back(view);
        }

        return snap;
    }

} // namespace rts::model
