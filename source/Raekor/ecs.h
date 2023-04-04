#pragma once

#include "rtti.h"

namespace Raekor::ecs {

typedef uint32_t Entity;

template<typename T>
class SparseSet {

    class EachIterator {
    public:
        EachIterator() = delete;
        EachIterator(typename std::vector<T>::iterator t_iter, typename std::vector<Entity>::iterator e_iter)
            : t_iter(t_iter), e_iter(e_iter) {}

        using value_type = std::tuple<Entity, T&>;
        using iterator_category = std::forward_iterator_tag;

        bool operator==(const EachIterator& rhs) { return (t_iter == rhs.t_iter) && (e_iter == rhs.e_iter); }
        bool operator!=(const EachIterator& rhs) { return (t_iter != rhs.t_iter) && (e_iter != rhs.e_iter); }
        
        EachIterator& operator++() {
            t_iter++;
            e_iter++;
            return *this;
        }

        EachIterator operator++(int) {
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

        auto begin() { return EachIterator(set.storage.begin(), set.entities.begin()); }
        auto end() { return EachIterator(set.storage.end(), set.entities.end()); }

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

        // grow the sparse vector exponentially when we need to make room
        if (sparse.size() <= entity) {
            auto sparse_temp = sparse;
            sparse.resize(entity + 1);
            memcpy(sparse.data(), sparse_temp.data(), sparse_temp.size());
        }

        sparse[entity] = uint32_t(entities.size() - 1);
        return storage.emplace_back(t);
    }

    T& Get(Entity entity) {
        return storage[sparse[entity]];
    }

    void Remove(Entity entity) {
        if (!Contains(entity))
            return;

        // set the current component to whatever is in the back of the storage
        Get(entity) = storage.back();
        // set the current entity (packed) to whatever is in the back of the packed entities
        entities[sparse[entity]] = entities.back();
        sparse[entities.back()] = sparse[entity];

        storage.pop_back();
        entities.pop_back();
    }

    bool Contains(Entity entity) {
        if (entity >= sparse.size())
            return false;

        if (sparse[entity] >= entities.size())
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

    auto begin() { return EachIterator(storage.begin(), entities.begin()); }
    auto end() { return EachIterator(storage.end(), entities.end()); }

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
        /* TODO */ 
    }

    template<typename Component>
    Component& Add(Entity entity) {
        if (m_Components.find(RTTI_OF(Component).GetHash()) == m_Components.end())
            m_Components[RTTI_OF(Component).GetHash()] = new SparseSet<Component>();

        auto set = static_cast<SparseSet<Component>*>(m_Components.at(RTTI_OF(Component).GetHash()));
        return set->Insert(entity, Component());
    }

    template<typename Component>
    auto& _GetInternal(Entity entity) {
        assert(m_Components.find(RTTI_OF(Component).GetHash()) != m_Components.end());
        auto set = static_cast<SparseSet<Component>*>(m_Components.at(RTTI_OF(Component).GetHash()));
        return set->Get(entity);
    }

    template<typename ...Components>
    auto Get(Entity entity) {
        if constexpr (sizeof...(Components) == 1)
            return _GetInternal<Components...>(entity);
        else
            return std::tie(_GetInternal<Components>(entity)...);
    }

    void Clear() {
        m_Entities.clear();
        m_Components.clear();
    }

    template<typename ...Components>
    bool Has(Entity entity) {
        bool has_all = true;

        (..., [&]() {
            if (m_Components.find(RTTI_OF(Components).GetHash()) != m_Components.end()) {
                auto set = static_cast<SparseSet<Components>*>(m_Components.at(RTTI_OF(Components).GetHash()));

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
        if (m_Components.find(RTTI_OF(Component).GetHash()) != m_Components.end()) {
            auto set = static_cast<SparseSet<Component>*>(m_Components.at(RTTI_OF(Component).GetHash()));
            set->Remove(entity);
        }
    }

    friend class EachIterator;

    template <typename ...Components> 
    class EachIterator {
    public:
        EachIterator() = delete;
        EachIterator(ECS& ecs, std::vector<Entity>::iterator inIter) : ecs(ecs), it(inIter) {
            while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it))
                it++;
        }

        EachIterator(EachIterator& rhs) { ecs = rhs.ecs; it = rhs.it; }
        EachIterator(EachIterator&& rhs) { ecs = rhs.ecs; it = rhs.it; }

        using iterator_category = std::forward_iterator_tag;
        using value_type = std::tuple<Entity, Components&...>;

        bool operator==(const EachIterator& rhs) { return it == rhs.it; }
        bool operator!=(const EachIterator& rhs) { return it != rhs.it; }

        EachIterator& operator++() {
            do {
                it++;
            } while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it));

            return *this;
        }

        EachIterator operator++(int) {
            auto tmp = *this;
            ++* this;
            return tmp;
        }

        auto operator*() -> std::tuple<Entity, Components&...> {
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

        auto begin() { return EachIterator<Components...>(ecs, ecs.m_Entities.begin()); }
        auto end() { return EachIterator<Components...>(ecs, ecs.m_Entities.end()); }

    private:
        ECS& ecs;
    };

    template<typename ...Components>
    auto Each() {
        if constexpr (sizeof...(Components) == 1)
            return static_cast<SparseSet<Components...>*>(m_Components.at(RTTI_OF(Components).GetHash()...))->Each();
        else
            return ComponentView<Components...>(*this);
    }

private:
    std::vector<Entity> m_Entities;
    std::unordered_map<uint32_t, void*> m_Components;
};

} // raekor