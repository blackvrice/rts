//
// Created by black on 25. 12. 23..
//
#include <core/DIContainer.hpp>

namespace rts::core {

    DIContainer::DIContainer(std::shared_ptr<DIContainer> parent)
        : m_parent(std::move(parent)) {}

    std::shared_ptr<DIContainer> DIContainer::createScope() {
        return std::make_shared<DIContainer>(shared_from_this());
    }

    std::shared_ptr<void> DIContainer::resolveInternal(const std::type_index& type) {
        // 1️⃣ Singleton 캐시
        if (auto it = m_singletons.find(type); it != m_singletons.end())
            return it->second;

        // 2️⃣ Factory 존재
        if (auto it = m_factories.find(type); it != m_factories.end())
            return it->second(*this);

        // 3️⃣ Parent 탐색
        if (m_parent)
            return m_parent->resolveInternal(type);

        throw std::runtime_error("DIContainer: type not registered");
    }

    // ───────────── Template Impl ─────────────

    template<typename T>
    void DIContainer::registerSingleton(
        std::function<std::shared_ptr<T>(DIContainer&)> factory) {

        m_factories[typeid(T)] = [factory](DIContainer& c) {
            auto instance = factory(c);
            c.m_singletons[typeid(T)] = instance;
            return instance;
        };
    }

    template<typename T>
    void DIContainer::registerTransient(
        std::function<std::shared_ptr<T>(DIContainer&)> factory) {

        m_factories[typeid(T)] = [factory](DIContainer& c) {
            return factory(c);
        };
    }

    template<typename T>
    void DIContainer::registerInstance(std::shared_ptr<T> instance) {
        m_singletons[typeid(T)] = instance;
    }

    template<typename T>
    std::shared_ptr<T> DIContainer::resolve() {
        return std::static_pointer_cast<T>(
            resolveInternal(typeid(T))
        );
    }

    // 🔥 템플릿 명시적 인스턴스화 (필요한 타입만 추가)
    template void DIContainer::registerSingleton<int>(std::function<std::shared_ptr<int>(DIContainer&)>);
    template void DIContainer::registerTransient<int>(std::function<std::shared_ptr<int>(DIContainer&)>);
    template std::shared_ptr<int> DIContainer::resolve<int>();

}