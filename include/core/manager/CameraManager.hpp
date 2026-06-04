#pragma once

#include "core/model/Vector2D.hpp"

namespace rts::core::manager {
    class CameraManager {
    public:
        CameraManager();

        const model::Vector2D& position() const noexcept;
        const model::Vector2D& viewportSize() const noexcept;
        const model::Vector2D& worldSize() const noexcept;

        void setPosition(model::Vector2D position);
        void moveBy(model::Vector2D delta);
        void setViewportSize(model::Vector2D size);
        void setWorldSize(model::Vector2D size);

        model::Vector2D screenToWorld(const model::Vector2D& screen) const noexcept;
        model::Vector2D worldToScreen(const model::Vector2D& world) const noexcept;

    private:
        void clampPosition();

        model::Vector2D m_position {};
        model::Vector2D m_viewportSize { 1920.0f, 1080.0f };
        model::Vector2D m_worldSize { 2048.0f, 2048.0f };
    };
}
