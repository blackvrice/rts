//
// Created by black on 25. 12. 25..
//

#pragma once
#include <vector>
#include <memory>
#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/manager/ILogicManger.hpp"

namespace rts::core::model {
    class Unit;
    class IGameElement;
    struct Vector2D;
}

namespace rts::manager {

    class GameLogicManager : public ILogicManager {
    public:
        GameLogicManager(command::LogicCommandBus& bus,
                         command::LogicCommandRouter& router);

        void start() override;
        void update() override;
        void tick(float dt) const;
        bool canMoveUnitTo(const core::model::Unit& unit, const core::model::Vector2D& pos) const;

        // Logic 전용 API
        void addElement(std::shared_ptr<core::model::IGameElement> element);
        void clearSelection();
        void selectElement(const std::shared_ptr<core::model::IGameElement>& element);
        const std::vector<std::shared_ptr<core::model::IElement>>& getElements() const override;

    private:
        // 모든 게임 오브젝트 (소유)
        std::vector<std::shared_ptr<core::model::IGameElement>> m_elements;

        // 선택된 유닛 (비소유 View)
        std::vector<std::weak_ptr<core::model::IGameElement>> m_selectedElements;
    };

} // namespace rts::manager
