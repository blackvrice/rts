//
// Created by black on 25. 12. 6..
//

#ifndef RTS_DICONTAINER_HPP
#define RTS_DICONTAINER_HPP
// include/rts/core/DIContainer.hpp
#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <stdexcept>

namespace rts::core {

    class DIContainer {
    public:
        DIContainer() = default;

        template<typename T, typename... Args>
        std::shared_ptr<T> registerSingleton(Args&&... args) {
            auto typeId = std::type_index(typeid(T));
            auto it = m_singletons.find(typeId);
            if (it != m_singletons.end()) {
                return std::static_pointer_cast<T>(it->second);
            }

            auto instance = std::make_shared<T>(std::forward<Args>(args)...);
            m_singletons.emplace(typeId, instance);
            return instance;
        }

        template<typename T>
        std::shared_ptr<T> resolve() const {
            auto typeId = std::type_index(typeid(T));
            auto it = m_singletons.find(typeId);
            if (it == m_singletons.end()) {
                throw std::runtime_error("DIContainer: type not registered");
            }
            return std::static_pointer_cast<T>(it->second);
        }

        template<typename T>
        bool isRegistered() const {
            auto typeId = std::type_index(typeid(T));
            return m_singletons.find(typeId) != m_singletons.end();
        }

    private:
        std::unordered_map<std::type_index, std::shared_ptr<void>> m_singletons;
    };

} // namespace rts::core


#endif //RTS_DICONTAINER_HPP