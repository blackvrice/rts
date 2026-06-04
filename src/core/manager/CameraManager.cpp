#include "core/manager/CameraManager.hpp"

#include <algorithm>

namespace rts::core::manager {
    CameraManager::CameraManager() = default;

    const model::Vector2D& CameraManager::position() const noexcept {
        return m_position;
    }

    const model::Vector2D& CameraManager::viewportSize() const noexcept {
        return m_viewportSize;
    }

    const model::Vector2D& CameraManager::worldSize() const noexcept {
        return m_worldSize;
    }

    void CameraManager::setPosition(model::Vector2D position) {
        m_position = position;
        clampPosition();
    }

    void CameraManager::moveBy(model::Vector2D delta) {
        setPosition(m_position + delta);
    }

    void CameraManager::setViewportSize(model::Vector2D size) {
        m_viewportSize = {
            std::max(size.x, 1.0f),
            std::max(size.y, 1.0f)
        };
        clampPosition();
    }

    void CameraManager::setWorldSize(model::Vector2D size) {
        m_worldSize = {
            std::max(size.x, 1.0f),
            std::max(size.y, 1.0f)
        };
        clampPosition();
    }

    model::Vector2D CameraManager::screenToWorld(const model::Vector2D& screen) const noexcept {
        // Camera position is the world-space top-left of the viewport.
        return screen + m_position;
    }

    model::Vector2D CameraManager::worldToScreen(const model::Vector2D& world) const noexcept {
        return world - m_position;
    }

    void CameraManager::clampPosition() {
        const float maxX = std::max(0.0f, m_worldSize.x - m_viewportSize.x);
        const float maxY = std::max(0.0f, m_worldSize.y - m_viewportSize.y);

        m_position.x = std::clamp(m_position.x, 0.0f, maxX);
        m_position.y = std::clamp(m_position.y, 0.0f, maxY);
    }
}
