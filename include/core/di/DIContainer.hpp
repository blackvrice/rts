#pragma once
#include <memory>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <any>
#include <tuple>
#include <utility>

namespace rts::core {

class DIContainer {
public:
    enum class Lifetime { Singleton, Transient, Scoped };

private:
    // ---------------- Factory type-erasure ----------------
    struct IFactoryBase {
        virtual ~IFactoryBase() = default;
        virtual std::shared_ptr<void> create(DIContainer& di, const std::any& packedArgs) = 0;
        virtual std::type_index argsType() const = 0; // tuple<...> typeid, or typeid(void)
    };

    template<typename T, typename... Args>
    struct FactoryImpl final : IFactoryBase {
        using Fn = std::function<std::shared_ptr<T>(DIContainer&, Args...)>;
        Fn fn;

        explicit FactoryImpl(Fn f) : fn(std::move(f)) {}

        std::shared_ptr<void> create(DIContainer& di, const std::any& packedArgs) override {
            if constexpr (sizeof...(Args) == 0) {
                // No-arg factory expects packedArgs to be empty or typeid(void)
                return std::static_pointer_cast<void>(fn(di));
            } else {
                using TupleT = std::tuple<std::decay_t<Args>...>;
                if (!packedArgs.has_value())
                    throw std::runtime_error("DIContainer: resolve(args...) called but factory expects args");
                if (packedArgs.type() != typeid(TupleT))
                    throw std::runtime_error("DIContainer: argument signature mismatch for resolve(args...)");

                const auto& tup = std::any_cast<const TupleT&>(packedArgs);
                return std::apply(
                    [&](auto&&... unpacked) {
                        return std::static_pointer_cast<void>(
                            fn(di, std::forward<decltype(unpacked)>(unpacked)...)
                        );
                    },
                    tup
                );
            }
        }

        std::type_index argsType() const override {
            if constexpr (sizeof...(Args) == 0) return typeid(void);
            else return typeid(std::tuple<std::decay_t<Args>...>);
        }
    };

    // ---------------- Registration ----------------
    struct Registration {
        Lifetime lifetime{};
        std::shared_ptr<IFactoryBase> factory{};
        std::shared_ptr<void> singletonInstance{};
        std::type_index createdArgsType{ typeid(void) }; // for singleton/scoped safety
        bool hasCreatedArgsType{ false };
    };

    using Registry = std::unordered_map<std::type_index, Registration>;
    Registry m_registry;

    using ScopeMap = std::unordered_map<std::type_index, std::shared_ptr<void>>;
    std::vector<ScopeMap> m_scopes;

public:
    DIContainer() = default;
    ~DIContainer() = default;

    // ---------------- Registration APIs ----------------

    // instance singleton
    template<typename T>
    void registerSingleton(std::shared_ptr<T> instance) {
        Registration reg;
        reg.lifetime = Lifetime::Singleton;
        reg.singletonInstance = std::static_pointer_cast<void>(instance);
        reg.factory = nullptr;
        m_registry[typeid(T)] = std::move(reg);
    }

    // no-arg singleton
    template<typename T>
    void registerSingleton(std::function<std::shared_ptr<T>(DIContainer&)> factory) {
        Registration reg;
        reg.lifetime = Lifetime::Singleton;
        reg.factory = std::make_shared<FactoryImpl<T>>(std::move(factory));
        m_registry[typeid(T)] = std::move(reg);
    }

    // arg singleton
    template<typename T, typename... Args>
    void registerSingleton(std::function<std::shared_ptr<T>(DIContainer&, Args...)> factory) {
        Registration reg;
        reg.lifetime = Lifetime::Singleton;
        reg.factory = std::make_shared<FactoryImpl<T, Args...>>(std::move(factory));
        m_registry[typeid(T)] = std::move(reg);
    }

    // no-arg transient
    template<typename T>
    void registerTransient(std::function<std::shared_ptr<T>(DIContainer&)> factory) {
        Registration reg;
        reg.lifetime = Lifetime::Transient;
        reg.factory = std::make_shared<FactoryImpl<T>>(std::move(factory));
        m_registry[typeid(T)] = std::move(reg);
    }

    // arg transient
    template<typename T, typename... Args>
    void registerTransient(std::function<std::shared_ptr<T>(DIContainer&, Args...)> factory) {
        Registration reg;
        reg.lifetime = Lifetime::Transient;
        reg.factory = std::make_shared<FactoryImpl<T, Args...>>(std::move(factory));
        m_registry[typeid(T)] = std::move(reg);
    }

    // no-arg scoped
    template<typename T>
    void registerScoped(std::function<std::shared_ptr<T>(DIContainer&)> factory) {
        Registration reg;
        reg.lifetime = Lifetime::Scoped;
        reg.factory = std::make_shared<FactoryImpl<T>>(std::move(factory));
        m_registry[typeid(T)] = std::move(reg);
    }

    // arg scoped
    template<typename T, typename... Args>
    void registerScoped(std::function<std::shared_ptr<T>(DIContainer&, Args...)> factory) {
        Registration reg;
        reg.lifetime = Lifetime::Scoped;
        reg.factory = std::make_shared<FactoryImpl<T, Args...>>(std::move(factory));
        m_registry[typeid(T)] = std::move(reg);
    }

    template<typename T>
    void unregisterType() { m_registry.erase(typeid(T)); }

    // ---------------- Resolve APIs ----------------

    template<typename T>
    std::shared_ptr<T> resolve() {
        return resolveInternal<T>(std::any{});
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> resolve(Args&&... args) {
        using TupleT = std::tuple<std::decay_t<Args>...>;
        return resolveInternal<T>(std::any{ TupleT{ std::forward<Args>(args)... } });
    }

    template<typename T>
    std::shared_ptr<T> tryResolve() {
        if (!isRegistered<T>()) return nullptr;
        return resolve<T>();
    }

    template<typename T>
    bool isRegistered() const {
#if __cpp_lib_unordered_map_contains >= 201411L
        return m_registry.contains(typeid(T));
#else
        return m_registry.find(typeid(T)) != m_registry.end();
#endif
    }

    // ---------------- Scope control ----------------
    void beginScope() { m_scopes.emplace_back(); }

    void endScope() {
        if (m_scopes.empty())
            throw std::runtime_error("DIContainer: no active scope");
        m_scopes.pop_back();
    }

    bool hasScope() const { return !m_scopes.empty(); }

    struct ScopeGuard {
        DIContainer& di;
        explicit ScopeGuard(DIContainer& d) : di(d) { di.beginScope(); }
        ~ScopeGuard() { di.endScope(); }
        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;
    };

    // ---------------- Utilities ----------------
    void clear() { m_registry.clear(); m_scopes.clear(); }

    void dump() const {
        std::cout << "[DIContainer]\n";
        for (auto const& [type, reg] : m_registry) {
            std::cout << " - " << type.name() << " : ";
            switch (reg.lifetime) {
                case Lifetime::Singleton: std::cout << "Singleton"; break;
                case Lifetime::Transient: std::cout << "Transient"; break;
                case Lifetime::Scoped:    std::cout << "Scoped";    break;
            }
            std::cout << "\n";
        }
    }

private:
    template<typename T>
    std::shared_ptr<T> resolveInternal(const std::any& packedArgs) {
        auto it = m_registry.find(typeid(T));
        if (it == m_registry.end())
            throw std::runtime_error("DIContainer: type not registered");

        Registration& reg = it->second;

        switch (reg.lifetime) {
            case Lifetime::Singleton:
                return resolveSingleton<T>(reg, packedArgs);
            case Lifetime::Transient:
                return resolveTransient<T>(reg, packedArgs);
            case Lifetime::Scoped:
                return resolveScoped<T>(reg, packedArgs);
        }
        throw std::runtime_error("DIContainer: invalid lifetime");
    }

    template<typename T>
    std::shared_ptr<T> resolveSingleton(Registration& reg, const std::any& packedArgs) {
        if (reg.singletonInstance)
            return std::static_pointer_cast<T>(reg.singletonInstance);

        if (!reg.factory)
            throw std::runtime_error("DIContainer: singleton factory missing");

        // Optional safety: singleton이 args 시그니처로 생성되면 이후 다른 시그니처 호출을 막기
        enforceCreatedArgsType(reg, reg.factory->argsType());

        reg.singletonInstance = reg.factory->create(*this, packedArgs);
        return std::static_pointer_cast<T>(reg.singletonInstance);
    }

    template<typename T>
    std::shared_ptr<T> resolveTransient(Registration& reg, const std::any& packedArgs) {
        if (!reg.factory)
            throw std::runtime_error("DIContainer: transient factory missing");
        return std::static_pointer_cast<T>(reg.factory->create(*this, packedArgs));
    }

    template<typename T>
    std::shared_ptr<T> resolveScoped(Registration& reg, const std::any& packedArgs) {
        if (m_scopes.empty())
            throw std::runtime_error("DIContainer: resolveScoped without scope");

        auto& scope = m_scopes.back();
        auto found = scope.find(typeid(T));
        if (found != scope.end())
            return std::static_pointer_cast<T>(found->second);

        if (!reg.factory)
            throw std::runtime_error("DIContainer: scoped factory missing");

        // Optional safety: scoped도 args 시그니처 고정 (scope 내에서)
        enforceCreatedArgsType(reg, reg.factory->argsType());

        auto instance = reg.factory->create(*this, packedArgs);
        scope[typeid(T)] = instance;
        return std::static_pointer_cast<T>(instance);
    }

    // singleton/scoped가 “어떤 args 시그니처로 생성되는지” 고정하기 위한 안전장치
    void enforceCreatedArgsType(Registration& reg, std::type_index currentArgsType) {
        if (!reg.hasCreatedArgsType) {
            reg.createdArgsType = currentArgsType;
            reg.hasCreatedArgsType = true;
            return;
        }
        if (reg.createdArgsType != currentArgsType) {
            throw std::runtime_error("DIContainer: resolve() signature differs from first creation for Singleton/Scoped");
        }
    }
};

} // namespace rts::core
