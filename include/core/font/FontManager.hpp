// core/font/FontManager.hpp
#pragma once

#include <unordered_map>
#include <memory>
#include <string>

#include "FontTypes.hpp"
#include "FontMetrics.hpp"

namespace rts::core::font {

    class FontManager {
    public:
        static FontManager& instance();

        // Font registration (platform dependent)
        FontId registerFont(
            const std::string& name,
            std::shared_ptr<void> nativeFont
        );

        void unregisterFont(FontId id);

        bool hasFont(FontId id) const;

        // Native handle (플랫폼용)
        template<typename T>
        T* getNative(FontId id) const {
            auto it = m_fonts.find(id);
            if (it == m_fonts.end())
                return nullptr;
            return static_cast<T*>(it->second.native.get());
        }

        // Metrics
        const FontMetrics& metrics() const;

        void setMetricsProvider(
            std::unique_ptr<FontMetrics> metrics
        );

    private:
        FontManager() = default;

        struct FontEntry {
            std::string name;
            std::shared_ptr<void> native;
        };

        FontId m_nextId = 1;
        std::unordered_map<FontId, FontEntry> m_fonts;

        std::unique_ptr<FontMetrics> m_metrics;
    };

}
