//
// Created by black on 25. 12. 6..
//

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

#include "../command/CommandRouter.hpp"
#include "manager/IManager.hpp"
#include "scene/Scene.hpp"

namespace rts::manager {

    class SceneManager final : public IManager {
    public:
        explicit SceneManager(std::shared_ptr<core::CommandBus> bus, core::CommandRouter& router)
            : IManager(bus, router) {}

        void start() override;
        void update() override;

    private:
        void requestSceneChange(const std::string& name);
        void changeScene(const std::string& name);

        // Scene factory table
        using SceneFactory = std::function<std::unique_ptr<scene::Scene>()>;
        std::unordered_map<std::string, SceneFactory> m_sceneFactories;

        std::unique_ptr<scene::Scene> m_currentScene;

        bool m_sceneChangeRequested = false;
        std::string m_nextScene;

    };

} // namespace rts::manager
