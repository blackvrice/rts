//
// Created by black on 25. 12. 23..
//

#include <factory/SceneFactory.hpp>

#include "scene/InGameScene.hpp"
#include "scene/LobbyScene.hpp"
#include "scene/LoginScene.hpp"

namespace rts::factory {
    SceneFactory::SceneFactory(core::DIContainer &di) : m_di(di)  {

        m_creators[type::SceneType::Login] = [](core::DIContainer& di) {
            return di.resolve<scene::LoginScene>();
        };

        m_creators[type::SceneType::Lobby] = [](core::DIContainer& di) {
            return di.resolve<scene::LobbyScene>();
        };

        m_creators[type::SceneType::Game] = [](core::DIContainer& di) {
            return di.resolve<scene::InGameScene>();
        };
    }

    std::shared_ptr<scene::Scene> SceneFactory::CreateScene(type::SceneType type)  {
        auto sceneDI = m_di.createScope(); // ⭐ Scene 전용 Scope

        auto it = m_creators.find(type);
        if (it == m_creators.end())
            throw std::runtime_error("Unknown SceneType");

        return it->second(*sceneDI);
    }
}
