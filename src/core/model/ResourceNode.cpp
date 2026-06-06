#include "core/model/ResourceNode.hpp"
#include <algorithm>
#include <string>

namespace rts::core::model {
    ResourceNode::ResourceNode(
        Vector2D pos,
        ResourceType type,
        int totalAmount,
        int gatherAmount,
        float gatherDurationSeconds,
        int maxGatherers)
        : m_position(pos)
        , m_type(type)
        , m_remaining(totalAmount)
        , m_gatherAmount(gatherAmount)
        , m_gatherDurationSeconds(gatherDurationSeconds)
        , m_maxGatherers(maxGatherers) {}

    bool ResourceNode::tryGather(int& amountOut) {
        if (m_remaining <= 0) {
            m_reservedGatherers.clear();
            amountOut = 0;
            return false;
        }
        amountOut = std::min(m_remaining, m_gatherAmount);
        m_remaining -= amountOut;
        if (m_remaining <= 0) {
            m_reservedGatherers.clear();
        }
        return true;
    }

    int ResourceNode::reservedGathererCount() const noexcept {
        return static_cast<int>(m_reservedGatherers.size());
    }

    bool ResourceNode::reserveGatherSlot(const IGameElement& worker) {
        if (isDepleted()) {
            return false;
        }

        if (hasGatherReservation(worker)) {
            return true;
        }

        if (reservedGathererCount() >= m_maxGatherers) {
            return false;
        }

        m_reservedGatherers.push_back(&worker);
        return true;
    }

    void ResourceNode::releaseGatherSlot(const IGameElement& worker) {
        const auto it = std::remove(
            m_reservedGatherers.begin(),
            m_reservedGatherers.end(),
            &worker
        );
        m_reservedGatherers.erase(it, m_reservedGatherers.end());
    }

    bool ResourceNode::hasGatherReservation(const IGameElement& worker) const {
        return std::find(
            m_reservedGatherers.begin(),
            m_reservedGatherers.end(),
            &worker
        ) != m_reservedGatherers.end();
    }

    std::string ResourceNode::displayName() const {
        return m_type == ResourceType::Gold ? "Gold Mine" : "Wood";
    }
}
