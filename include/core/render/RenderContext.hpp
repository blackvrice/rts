// Created by black on 25. 12. 27..
//
// core/render/RenderContext.hpp
#pragma once

#include "core/manager/CameraManager.hpp"
#include "core/model/Vector2D.hpp"
#include "platform/IWindow.hpp"

namespace rts::core::render {

    class RenderContext {
    public:
        RenderContext(platform::IWindow& window, model::Vector2D size, manager::CameraManager& camera)
            : m_window(&window), m_camera(&camera), m_size(size) {
            m_camera->setViewportSize(size);
        }

        platform::IWindow& window() const { return *m_window; }
        manager::CameraManager& camera() const { return *m_camera; }
    private:
        platform::IWindow* m_window;   // ❗ non-owning
        manager::CameraManager* m_camera; // non-owning
        model::Vector2D m_size;       // 해상도
        float dpiScale = 1.0f;     // DPI 스케일
    };

}
