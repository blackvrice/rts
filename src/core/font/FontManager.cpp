// core/font/FontManager.cpp
#include "core/font/FontManager.hpp"

#include <stdexcept>

namespace rts::core::font {

    FontManager& FontManager::instance() {
        static FontManager fm;
        return fm;
    }

    FontId FontManager::registerFont(
        const std::string& name,
        std::shared_ptr<void> nativeFont
    ) {
        FontId id = m_nextId++;
        m_fonts.emplace(id, FontEntry{ name, nativeFont });
        return id;
    }

    void FontManager::unregisterFont(FontId id) {
        m_fonts.erase(id);
    }

    bool FontManager::hasFont(FontId id) const {
        return m_fonts.contains(id);
    }

    const FontMetrics& FontManager::metrics() const {
        if (!m_metrics)
            throw std::runtime_error("FontMetrics not set");
        return *m_metrics;
    }

    void FontManager::setMetricsProvider(
        std::unique_ptr<FontMetrics> metrics
    ) {
        m_metrics = std::move(metrics);
    }

}
