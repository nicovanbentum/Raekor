#pragma once

#include "rtti.h"

namespace Raekor::ecs {

typedef uint32_t Entity;

template<typename T>
class SparseSet {
    
    class Iterator {
    public:
        Iterator() = delete;
        Iterator(typename std::vector<T>::iterator t_iter, typename std::vector<Entity>::iterator e_iter)
            : t_iter(t_iter), e_iter(e_iter) {}

        using value_type = std::tuple<Entity, T&>;
        using iterator_category = std::forward_iterator_tag;

        bool operator==(const Iterator& rhs) { return (t_iter == rhs.t_iter) && (e_iter == rhs.e_iter); }
        bool operator!=(const Iterator& rhs) { return (t_iter != rhs.t_iter) && (e_iter != rhs.e_iter); }
        
        Iterator& operator++() {
            t_iter++;
            e_iter++;
            return *this;
        }

        Iterator operator++(int) {
            auto tmp = *this;
            ++* this;
            return tmp;
        }


        auto operator*() -> value_type {
            return std::forward_as_tuple(*e_iter, *t_iter);
        }

    private:
        typename std::vector<T>::iterator t_iter;
        typename std::vector<Entity>::iterator e_iter;
    };

    class View {
    public:
        View(SparseSet<T>& set) : set(set) {}
        View(View& rhs) { set = rhs.set; }

        auto begin() { return Iterator(set.storage.begin(), set.entities.begin()); }
        auto end() { return Iterator(set.storage.end(), set.entities.end()); }

    private:
        SparseSet<T>& set;
    };

public:
    T& Insert(Entity entity, const T& t) {
        if (Contains(entity)) {
            auto& existing_t = Get(entity);
            existing_t = t;
            return existing_t;
        }

        entities.push_back(entity);

        if (sparse.size() <= entity)
            sparse.resize(entity + 1);

        sparse[entity] = static_cast<uint32_t>(entities.size() - 1);
        return storage.emplace_back(t);
    }

    T& Get(Entity entity) {
        return storage[sparse[entity]];
    }

    void Remove(Entity entity) {
        if (!Contains(entity))
            return;

        Get(entity) = storage.back();
        entities[sparse[entity]] = entities.back();

        storage.pop_back();
        entities.pop_back();

        sparse[entities[sparse[entity]]] = sparse[entity];
    }

    bool Contains(Entity entity) {
        if (entity >= sparse.size())
            return false;

        return entities[sparse[entity]] == entity;
    }

    void Clear() {
        sparse.clear();
        storage.clear();
        entities.clear();
    }

    View Each() { return View(*this); }
    size_t Length() { return storage.size(); }

    auto begin() { return Iterator(storage.begin(), entities.begin()); }
    auto end() { return Iterator(storage.end(), entities.end()); }

    std::vector<T> storage;
    std::vector<Entity> entities;
    std::vector<uint32_t> sparse;
};


class ECS {

public:
    Entity Create() {
        return m_Entities.emplace_back(uint32_t(m_Entities.size()));
    }

    void Destroy(Entity entity) {
    }

    template<typename Component>
    Component& Add(Entity entity) {
        if (!m_Components.contains(Component::sGetRTTI()))
            m_Components.insert({ Component::sGetRTTI(), new SparseSet<Component>() });

        auto set = static_cast<SparseSet<Component>*>(m_Components.at(Component::sGetRTTI()));
        return set->Insert(entity, Component());
    }

    template<typename Component>
    auto& _GetInternal(Entity entity) {
        assert(m_Components.contains(Component::sGetRTTI()));
        auto set = static_cast<SparseSet<Component>*>(m_Components.at(Component::sGetRTTI()));
        return set->Get(entity);
    }

    template<typename ...Components>
    auto&& Get(Entity entity) {
        if constexpr (sizeof...(Components) == 1)
            return _GetInternal<Components...>(entity);
        else
            return std::forward_as_tuple(_GetInternal<Components>(entity)...);
    }

    void Clear() {
        m_Entities.clear();
        m_Components.clear();
    }

    template<typename ...Components>
    bool Has(Entity entity) {
        bool has_all = true;

        (..., [&]() {
            if (m_Components.contains(Components::sGetRTTI())) {
                auto set = static_cast<SparseSet<Components>*>(m_Components.at(Components::sGetRTTI()));

                if (!set->Contains(entity))
                    has_all = false;
            }
            else
                has_all = false;
            }());

        return has_all;
    }

    template<typename Component>
    void Remove(Entity entity) {
        if (m_Components.contains(Component::sGetRTTI())) {
            auto set = static_cast<SparseSet<Component>*>(m_Components.at(Component::sGetRTTI()));
            set->Remove(entity);
        }
    }

    friend class Iterator;
    template <typename ...Components> 
    class Iterator {
    public:
        Iterator() = delete;
        Iterator(ECS& ecs, std::vector<Entity>::iterator inIter) : ecs(ecs), it(inIter) {
            while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it))
                it++;
        }

        Iterator(Iterator& rhs) { ecs = rhs.ecs; it = rhs.it; }
        Iterator(Iterator&& rhs) { ecs = rhs.ecs; it = rhs.it; }

        using iterator_category = std::forward_iterator_tag;
        using value_type = std::tuple<Entity, Components&...>;

        bool operator==(const Iterator& rhs) { return it == rhs.it; }
        bool operator!=(const Iterator& rhs) { return it != rhs.it; }

        Iterator& operator++() {
            do {
                it++;
            } while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it));

            return *this;
        }

        Iterator operator++(int) {
            auto tmp = *this;
            ++* this;
            return tmp;
        }

        auto operator*() -> value_type {
            return std::tuple_cat(std::make_tuple(*it), ecs.Get<Components...>(*it));
        }

    private:
        ECS& ecs;
        std::vector<Entity>::iterator it;
    };

    template <typename ...Components>
    class ComponentView {
    public:
        ComponentView(ECS& ecs) : ecs(ecs) {}
        ComponentView(ComponentView& rhs) { ecs = rhs.ecs; }

        auto begin() { return Iterator<Components...>(ecs, ecs.m_Entities.begin()); }
        auto end() { return Iterator<Components...>(ecs, ecs.m_Entities.end()); }

    private:
        ECS& ecs;
    };

    template<typename ...Components>
    auto Each() {
        if constexpr (sizeof...(Components) == 1)
            return static_cast<SparseSet<Components...>*>(m_Components.at(Components::sGetRTTI()...))->Each();
        else
            return ComponentView<Components...>(*this);
    }

private:
    std::vector<Entity> m_Entities;
    std::unordered_map<RTTI, void*> m_Components;
};

} // raekor