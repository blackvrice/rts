// include/rts/ecs/Registry.hpp
#pragma once

#include "Entity.hpp"
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>

namespace rts::ecs {
    // ────────────────────────────────────────────────
    // Base interface for component storage
    // ────────────────────────────────────────────────
    class IComponentStorage {
    public:
        virtual ~IComponentStorage() = default;

        virtual void remove(Entity e) = 0;
    };


    // ────────────────────────────────────────────────
    // T component storage
    // ────────────────────────────────────────────────
    template<typename T>
    class ComponentStorage : public IComponentStorage {
    public:

        // ----------------------
        // 기존: non-const version
        // ----------------------
        T* get(Entity e) {
            auto it = m_data.find(e);
            return it == m_data.end() ? nullptr : &it->second;
        }

        // ----------------------
        // 추가: const 버전
        // ----------------------
        const T* get(Entity e) const {
            auto it = m_data.find(e);
            return it == m_data.end() ? nullptr : &it->second;
        }

        T& add(Entity e, const T& value = T{}) {
            return m_data[e] = value;
        }

        void remove(Entity e) override {
            m_data.erase(e);
        }

        auto& data() { return m_data; }
        const auto& data() const { return m_data; }
        bool has(Entity e) const {
            return m_data.find(e) != m_data.end();
        }
    private:
        std::unordered_map<Entity, T> m_data;
    };


    // ────────────────────────────────────────────────
    // ⭐ Registry class (missing before)
    // ────────────────────────────────────────────────
    class Registry {
    public:
        // Create a new entity
        Entity createEntity() {
            Entity e = ++m_lastEntity;
            m_entities.push_back(e);
            return e;
        }

        // Destroy entity: remove from all component storage
        void destroyEntity(Entity e) {
            for (auto &[_, storage]: m_storages)
                storage->remove(e);
        }

        // Access storage<T>, auto-create if missing
        template<typename T>
        ComponentStorage<T> &storage() {
            std::type_index key(typeid(T));

            auto it = m_storages.find(key);
            if (it == m_storages.end()) {
                auto ptr = std::make_unique<ComponentStorage<T> >();
                auto raw = ptr.get();
                m_storages.emplace(key, std::move(ptr));
                return *raw;
            }
            return *static_cast<ComponentStorage<T> *>(it->second.get());
        }

        // Add component
        template<typename T>
        T &add(Entity e, const T &component = T{}) {
            return storage<T>().add(e, component);
        }

        // Get component pointer
        template<typename T>
        T *get(Entity e) {
            return storage<T>().get(e);
        }

        // Check component existence
        template<typename T>
        bool has(Entity e) {
            return storage<T>().has(e);
        }

        const std::vector<Entity> &entities() const {
            return m_entities;
        }

        template<typename T>
        const ComponentStorage<T> &storage() const {
            auto ti = std::type_index(typeid(T));
            auto it = m_storages.find(ti);
            if (it == m_storages.end()) {
                throw std::runtime_error("storage<T>() const: component not found");
            }
            return *static_cast<const ComponentStorage<T> *>(it->second.get());
        }

    private:
        Entity m_lastEntity{0};
        std::vector<Entity> m_entities;

        std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage> > m_storages;
    };
} // namespace rts::ecs
