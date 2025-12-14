//
// Created by black on 25. 12. 6..
//

#ifndef RTS_SELECTION_HPP
#define RTS_SELECTION_HPP
// include/rts/component/Selection.hpp
#pragma once

namespace rts::component {

    struct Selection {
        mutable bool selected{false};
    };

} // namespace rts::component

#endif //RTS_SELECTION_HPP