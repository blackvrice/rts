//
// Created by black on 25. 12. 6..
//

// include/rts/model/Snapshot.hpp
#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <vector>

#include "UnitSnapshot.hpp"

namespace rts::model {

    struct Snapshot {
        std::vector<UnitSnapshot> units;
    };

} // namespace rts::model
