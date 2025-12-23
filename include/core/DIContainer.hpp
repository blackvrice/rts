//
// Created by black on 25. 12. 6..
//

#ifndef RTS_DICONTAINER_HPP
#define RTS_DICONTAINER_HPP
// include/rts/core/DIContainer.hpp
#pragma once
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <stdexcept>

namespace rts::core {

    class DIContainer : public std::enable_shared_from_this<DIContainer> {
    public:
        using Factory = std::function<std::shared_ptr<void>(DIContainer&)>;

        DIContainer() = default;
        explicit DIContainer(std::shared_ptr<DIContainer> parent);

        // ───────────── Register ─────────────
        template<typename T>
        void registerSingleton(std::function<std::shared_ptr<T>(DIContainer&)> factory);

        template<typename T>
        void registerTransient(std::function<std::shared_ptr<T>(DIContainer&)> factory);

        template<typename T>
        void registerInstance(std::shared_ptr<T> instance);

        // ───────────── Resolve ─────────────
        template<typename T>
        std::shared_ptr<T> resolve();

        // ───────────── Scope ─────────────
        std::shared_ptr<DIContainer> createScope();

    private:
        std::unordered_map<std::type_index, Factory> m_factories;
        std::unordered_map<std::type_index, std::shared_ptr<void>> m_singletons;
        std::shared_ptr<DIContainer> m_parent;

        std::shared_ptr<void> resolveInternal(const std::type_index& type);
    };

} // namespace rts::core


#endif //RTS_DICONTAINER_HPP