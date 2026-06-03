#include "core/manager/PathManager.hpp"

namespace rts::core::manager {
    void PathManager::bumpCollisionVersion() {
        clearCache();
    }
}
