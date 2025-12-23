//
// Created by black on 25. 12. 23..
//

#pragma once
#include <memory>

#include "scene/Scene.hpp"
#include "type/SceneType.hpp"
#include <core/DIContainer.hpp>


namespace rts::factory {
    class SceneFactory {
        public:
        explicit SceneFactory(core::DIContainer& di);
        std::shared_ptr<scene::Scene> CreateScene(type::SceneType type);
    private:
        core::DIContainer& m_di;
        using Creator = std::function<std::shared_ptr<scene::Scene>(core::DIContainer&)>;
        std::unordered_map<type::SceneType, Creator> m_creators;
    };
}
