//
// Created by black on 25. 12. 6..
//
// include/rts/model/GameModel.hpp
#pragma once
#include <ecs/Registry.hpp>
#include <model/Snapshot.hpp>
#include <ecs/Registry.hpp>
namespace rts::model {

    class GameModel {
    public:
        GameModel() = default;

        rts::ecs::Registry& registry() { return m_registry; }
        const rts::ecs::Registry& registry() const { return m_registry; }

        Snapshot buildSnapshot() const;

    private:
        // 데모용: 멀티스레드 mutex는 생략
        rts::ecs::Registry m_registry;
    };

} // namespace rts::model
