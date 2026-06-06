#include "core/model/ResourceNode.hpp"
#include <algorithm>
#include <string>

namespace rts::core::model {
    ResourceNode::ResourceNode(Vector2D pos, ResourceType type, int totalAmount, int gatherAmount)
        : m_position(pos)
        , m_type(type)
        , m_remaining(totalAmount)
        , m_gatherAmount(gatherAmount) {}

    bool ResourceNode::tryGather(int& amountOut) {
        if (m_remaining <= 0) {
            amountOut = 0;
            return false;
        }
        amountOut = std::min(m_remaining, m_gatherAmount);
        m_remaining -= amountOut;
        return true;
    }

    std::string ResourceNode::displayName() const {
        return m_type == ResourceType::Gold ? "Gold Mine" : "Wood";
    }
}
