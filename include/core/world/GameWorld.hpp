//
// Created by black on 26. 2. 1..
//

#pragma once

#include <memory>   // ✅ shared_ptr
#include <vector>   // ✅ vector

#include <core/map/TileMapSoA.hpp>
#include <core/world/GameWorldGridQuery.hpp>
#include <core/manager/PathManager.hpp>

namespace rts::core::model { class IElement; } // ✅ forward (정확한 네임스페이스가 이게 맞아야 함)

namespace rts::core::world {

    class GameWorld {
    public:
        GameWorld()
            : m_gridQuery(*this)
            , m_pathManager()  // ✅ PathManager가 grid ctor를 받는 버전일 때만 유지
        {}

        void addElement(const std::shared_ptr<model::IElement>& element) {
            m_elements.push_back(element);
        }

        // ✅ "void"가 아니라 int 반환 + const 여야 GameWorldGridQuery에서 width()/height()가 return 가능
        int gridWidth() const noexcept;
        int gridHeight() const noexcept;

        const std::vector<std::shared_ptr<model::IElement>>& getElements() const {
            return m_elements;
        }

        manager::PathManager& path() { return m_pathManager; }
        const manager::PathManager& path() const { return m_pathManager; }

        void onCollisionChanged() {
            // ✅ PathManager에 bumpCollisionVersion()이 있어야 함
            m_pathManager.bumpCollisionVersion();
        }

    private:
        map::TileMapSoA m_tileMap;
        std::vector<std::shared_ptr<model::IElement>> m_elements;

        world::GameWorldGridQuery m_gridQuery;
        manager::PathManager m_pathManager;
    };

} // namespace rts::core::world
