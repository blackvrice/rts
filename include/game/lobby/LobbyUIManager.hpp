//
// Created by black on 25. 12. 25..
//
#pragma once

#include <string>
#include <vector>

#include <core/Manager/IUIManager.hpp>

#include "core/model/Rect.hpp"
#include "core/model/Vector2D.hpp"

namespace rts::core::manager {
    class CameraManager;

    // Main-menu / lobby screen: a title and clickable buttons rendered straight to
    // the RenderQueue. A button click pushes a SceneChangeCommand on the logic bus,
    // which the SceneManager applies on the main thread. A second view lists the
    // replays saved under AppData; clicking one hands it to the next Game scene.
    class LobbyUIManager : public IUIManager {
    public:
        LobbyUIManager(command::UICommandRouter&, command::LogicCommandBus&,
                       core::render::RenderQueue&, CameraManager& camera);

        void update() override;
        void render() override;
        void syncWithWorld() override;

    private:
        enum class View { Menu, Replays };

        struct MenuButton {
            core::model::Rect rect;
            std::string label;
            std::string scene;       // SceneChangeCommand target ("game"), empty = none
            std::string replayPath;  // full path when this row plays a replay
        };

        MenuButton startButton() const;    // laid out from the current viewport size
        MenuButton replaysButton() const;  // opens the replay list
        MenuButton backButton() const;     // returns to the menu from the list
        // Buttons for each saved replay (one row per file, most recent first).
        std::vector<MenuButton> replayListButtons() const;
        // Rescans the AppData replay folder into m_replayFiles.
        void refreshReplayList();

        void renderMenu();
        void renderReplays();

        CameraManager& m_camera;
        core::model::Vector2D m_mouse {};
        View m_view { View::Menu };
        std::vector<std::string> m_replayFiles;  // full paths, sorted newest first
    };
}
