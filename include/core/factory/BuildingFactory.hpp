//
// Created by black on 25. 12. 23..
//

#pragma once

#include <memory>
#include <unordered_map>

namespace rts::factory {

    class BuildingFactory {
    public:
        void registerBuilding(
            BuildingType type,
            data::BuildingStaticData data
        ) {
            m_staticData[type] = std::move(data);
        }

        std::unique_ptr<entity::BuildingEntity> create(BuildingType type) {
            return std::make_unique<entity::BuildingEntity>(
                m_staticData.at(type)
            );
        }

    private:
        std::unordered_map<BuildingType, data::BuildingStaticData> m_staticData;
    };

}