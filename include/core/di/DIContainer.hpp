#pragma once
#include <memory>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <stdexcept>
#include <vector>
#include <iostream>

namespace rts::core {

class DIContainer {
public:
    enum class Lifetime {
        Singleton,
        Transient,
        Scoped
    };

private:
    struct Registration {
        Lifetime lifetime;
        std::function<std::shared_ptr<void>(DIContainer&)> factory;
        std::shared_ptr<void> singletonInstance;
    };

    using Registry = std::unordered_map<std::type_index, Registration>;
    Registry m_registry;

    // Scope storage
    std::vector<std::unordered_map<std::type_index, std::shared_ptr<void>>> m_scopes;

public:
    DIContainer() = default;
    ~DIContainer() = default;

    /* ===============================
     * Registration
     * =============================== */

    template<typename T>
    void registerSingleton(std::shared_ptr<T> instance) {
        m_registry[typeid(T)] = Registration{
            Lifetime::Singleton,
            [](DIContainer&) { return nullptr; },
            instance
        };
    }

    template<typename T>
    void registerSingleton(std::function<std::shared_ptr<T>(DIContainer&)> factory) {
        m_registry[typeid(T)] = Registration{
            Lifetime::Singleton,
            [factory](DIContainer& di) {
                return std::static_pointer_cast<void>(factory(di));
            },
            nullptr
        };
    }

    template<typename T>
    void registerTransient(std::function<std::shared_ptr<T>(DIContainer&)> factory) {
        m_registry[typeid(T)] = Registration{
            Lifetime::Transient,
            [factory](DIContainer& di) {
                return std::static_pointer_cast<void>(factory(di));
            },
            nullptr
        };
    }

    template<typename T>
    void registerScoped(std::function<std::shared_ptr<T>(DIContainer&)> factory) {
        m_registry[typeid(T)] = Registration{
            Lifetime::Scoped,
            [factory](DIContainer& di) {
                return std::static_pointer_cast<void>(factory(di));
            },
            nullptr
        };
    }

    template<typename T>
    void unregister() {
        m_registry.erase(typeid(T));
    }

    /* ===============================
     * Resolve
     * =============================== */

    template<typename T>
    std::shared_ptr<T> resolve() {
        auto it = m_registry.find(typeid(T));
        if (it == m_registry.end())
            throw std::runtime_error("DIContainer: type not registered");

        Registration& reg = it->second;

        switch (reg.lifetime) {
        case Lifetime::Singleton:
            if (!reg.singletonInstance) {
                reg.singletonInstance = reg.factory(*this);
            }
            return std::static_pointer_cast<T>(reg.singletonInstance);

        case Lifetime::Transient:
            return std::static_pointer_cast<T>(reg.factory(*this));

        case Lifetime::Scoped:
            return resolveScoped<T>(reg);
        }

        throw std::runtime_error("DIContainer: invalid lifetime");
    }

    template<typename T>
    std::shared_ptr<T> tryResolve() {
        if (!isRegistered<T>())
            return nullptr;
        return resolve<T>();
    }

    template<typename T>
    bool isRegistered() const {
        return m_registry.contains(typeid(T));
    }

    /* ===============================
     * Scope control
     * =============================== */

    void beginScope() {
        m_scopes.emplace_back();
    }

    void endScope() {
        if (m_scopes.empty())
            throw std::runtime_error("DIContainer: no active scope");
        m_scopes.pop_back();
    }

    bool hasScope() const {
        return !m_scopes.empty();
    }

    /* ===============================
     * Utilities
     * =============================== */

    void clear() {
        m_registry.clear();
        m_scopes.clear();
    }

    void dump() const {
        std::cout << "[DIContainer]\n";
        for (auto& [type, reg] : m_registry) {
            std::cout << " - " << type.name() << " : ";
            switch (reg.lifetime) {
            case Lifetime::Singleton: std::cout << "Singleton"; break;
            case Lifetime::Transient: std::cout << "Transient"; break;
            case Lifetime::Scoped: std::cout << "Scoped"; break;
            }
            std::cout << "\n";
        }
    }

private:
    template<typename T>
    std::shared_ptr<T> resolveScoped(Registration& reg) {
        if (m_scopes.empty())
            throw std::runtime_error("DIContainer: resolveScoped without scope");

        auto& scope = m_scopes.back();
        auto it = scope.find(typeid(T));
        if (it != scope.end()) {
            return std::static_pointer_cast<T>(it->second);
        }

        auto instance = reg.factory(*this);
        scope[typeid(T)] = instance;
        return std::static_pointer_cast<T>(instance);
    }
};

} // namespace rts::core
