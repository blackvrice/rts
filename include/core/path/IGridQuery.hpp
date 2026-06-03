// include/rts/path/IGridQuery.hpp
#pragma once
#include "GridTypes.hpp"

namespace rts::core::path {

    // PathManager가 월드에 요구하는 최소 정보
    class IGridQuery {
    public:
        virtual ~IGridQuery() = default;

        virtual int width()  const = 0;
        virtual int height() const = 0;

        // 타일 고정 장애물(벽/물/지형 등)
        virtual bool isBlockedStatic(GridPos p) const = 0;

        // 동적 점유(유닛/건물). 필요 없으면 false만 리턴해도 됨.
        virtual bool isBlockedDynamic(GridPos p) const = 0;

        // 맵 범위 체크
        virtual bool inBounds(GridPos p) const = 0;
    };

} // namespace rts::path
