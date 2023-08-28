#pragma once

#include "rtti.h"
#include "archive.h"

namespace Raekor {

using Entity = uint32_t;;
constexpr auto NULL_ENTITY = Entity(UINT32_MAX);

}

namespace Raekor::ecs {

template<typename T>
class SparseSet;

class ISparseSet {
public:
    virtual ~ISparseSet() = default;

    virtual void    Clear() = 0;
    virtual size_t  Length() const = 0;
    virtual void    Remove(Entity inEntity) = 0;
    virtual bool    Contains(Entity inEntity) const = 0;

    virtual void    Read(BinaryReadArchive& inArchive) = 0;
    virtual void    Read(JSON::ReadArchive& inArchive) = 0;
    virtual void    Write(BinaryWriteArchive& ioArchive) = 0;
    virtual void    Write(JSON::WriteArchive& ioArchive) = 0;

    template<typename T> SparseSet<T>* GetDerived()       { return static_cast<SparseSet<T>*>(this); }
    template<typename T> SparseSet<T>* GetDerived() const { return static_cast<SparseSet<T>*>(this); }

};

template<typename T>
class SparseSet : public ISparseSet {

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
    virtual ~SparseSet() { Clear(); }

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

    const T& Get(Entity entity) const {
        return storage[sparse[entity]];
    }

    void Remove(Entity entity) override final {
        if (!Contains(entity))
            return;

        // set the current component to whatever is in the back of the storage
        storage[sparse[entity]] = storage.back();
        // set the current entity (packed) to whatever is in the back of the packed entities
        entities[sparse[entity]] = entities.back();
        sparse[entities.back()] = sparse[entity];

        storage.pop_back();
        entities.pop_back();
    }

    bool Contains(Entity entity) const override final {
        if (entity >= sparse.size())
            return false;

        if (sparse[entity] >= entities.size())
            return false;

        return entities[sparse[entity]] == entity;
    }

    void Clear() override final {
        sparse.clear();
        storage.clear();
        entities.clear();
    }

    size_t Length() const override final { return storage.size(); }

    void Read(BinaryReadArchive& ioArchive) override final {
        ReadFileBinary(ioArchive.GetFile(), entities);
        ReadFileBinary(ioArchive.GetFile(), sparse);

        auto storage_size = 0ull;
        ReadFileBinary(ioArchive.GetFile(), storage_size);
        storage.resize(storage_size);
        
        for (auto& component : storage)
            ioArchive >> component;
    }
    void Read(JSON::ReadArchive& ioArchive) override final {}

    void Write(BinaryWriteArchive& ioArchive) override final {
        WriteFileBinary(ioArchive.GetFile(), entities);
        WriteFileBinary(ioArchive.GetFile(), sparse);

        WriteFileBinary(ioArchive.GetFile(), storage.size());

        for (const auto& component : storage)
            ioArchive << component;
    }

    void Write(JSON::WriteArchive& ioArchive) override final {
        
    }
    
    View Each() { return View(*this); }

    Slice<T> GetStorage() const { return Slice(storage.data(), storage.size()); }
    Slice<Entity> GetEntities() const { return Slice(entities.data(), entities.size()); }

    auto begin() { return EachIterator(storage.begin(), entities.begin()); }
    auto end() { return EachIterator(storage.end(), entities.end()); }

    std::vector<T> storage;
    std::vector<Entity> entities;
    std::vector<uint32_t> sparse;
};



class ECS {
public:
    template<typename Component>
    SparseSet<Component>* GetSparseSet() { 
        if (m_Components.find(gGetRTTI<Component>().GetHash()) == m_Components.end())
            m_Components[gGetRTTI<Component>().GetHash()] = new SparseSet<Component>();
        return m_Components.at(gGetTypeHash<Component>())->GetDerived<Component>(); 
    }

    template<typename Component>
    const SparseSet<Component>* GetSparseSet() const { 
        if (m_Components.find(gGetRTTI<Component>().GetHash()) == m_Components.end())
            m_Components[gGetRTTI<Component>().GetHash()] = new SparseSet<Component>();
        return m_Components.at(gGetTypeHash<Component>())->GetDerived<Component>(); 
    }

    Entity Create() { 
        return m_Entities.emplace_back(uint32_t(m_Entities.size())); 
    }
    
    void Destroy(Entity inEntity) {
        for (auto& [type_id, components] : m_Components) {
            if (components->Contains(inEntity))
                components->Remove(inEntity);
        }
    }

    template<typename Component>
    Component& Add(Entity entity) {
        if (m_Components.find(gGetRTTI<Component>().GetHash()) == m_Components.end())
            m_Components[gGetRTTI<Component>().GetHash()] = new SparseSet<Component>();

        return GetSparseSet<Component>()->Insert(entity, Component());
    }

    template<typename Component>
    Component& Add(Entity entity, const Component& inComponent) {
        if (m_Components.find(gGetRTTI<Component>().GetHash()) == m_Components.end())
            m_Components[gGetRTTI<Component>().GetHash()] = new SparseSet<Component>();

        return GetSparseSet<Component>()->Insert(entity, inComponent);
    }

    template<typename Component>
    Component& _GetInternal(Entity entity) {
        assert(m_Components.find(gGetRTTI<Component>().GetHash()) != m_Components.end());
        return GetSparseSet<Component>()->Get(entity);
    }

    template<typename Component>
    const Component& _GetInternal(Entity entity) const {
        assert(m_Components.find(gGetRTTI<Component>().GetHash()) != m_Components.end());
        return GetSparseSet<Component>()->Get(entity);
    }

    template<typename Component>
    uint32_t Count() const {
        if (m_Components.contains(RTTI_OF(Component).GetHash()))
            return uint32_t(GetSparseSet<Component>()->Length());
        else
            return 0;
    }

    template<typename ...Components>
    auto Get(Entity entity) -> decltype(auto) {
        if constexpr (sizeof...(Components) == 1)
            return _GetInternal<Components...>(entity);
        else
            return std::tie(_GetInternal<Components>(entity)...);
    }

    template<typename ...Components>
    auto Get(Entity entity) const -> decltype(auto) {
        if constexpr (sizeof...(Components) == 1)
            return _GetInternal<Components...>(entity);
        else
            return std::tie(_GetInternal<Components>(entity)...);
    }

    template<typename Component>
    const Component* GetPtr(Entity inEntity) const {
        if (m_Components.contains(RTTI_OF(Component).GetHash()) && Has<Component>(inEntity))
            return &GetSparseSet<Component>()->Get(inEntity);
        else
            return nullptr;
    }

    template<typename Component>
    Slice<Component> GetStorage() const {
        if (m_Components.contains(RTTI_OF(Component).GetHash()))
            return GetSparseSet<Component>()->GetStorage();
        else
            return Slice<Component>((const Component*)nullptr, uint64_t(0));
    }

    template<typename Component>
    Slice<Entity> GetEntities() const {
        if (m_Components.contains(RTTI_OF(Component).GetHash()))
            return GetSparseSet<Component>()->GetEntities();
        else
            return Slice<Entity>((const Entity*)nullptr, uint64_t(0));
    }

    void Clear() {
        for (const auto& [type_id, components] : m_Components)
            delete components;

        m_Entities.clear();
        m_Components.clear();
    }

    template<typename ...Components>
    bool Has(Entity entity) const {
        bool has_all = true;

        (..., [&]() {
            if (m_Components.find(RTTI_OF(Components).GetHash()) != m_Components.end()) {
                if (!GetSparseSet<Components>()->Contains(entity))
                    has_all = false;
            }
            else
                has_all = false;
            }());

        return has_all;
    }

    template<typename Component>
    void Remove(Entity entity) {
        if (m_Components.find(RTTI_OF(Component).GetHash()) != m_Components.end())
            GetSparseSet<Component>()->Remove(entity);
    }

    friend class EachIterator;
    friend class ConstEachIterator;

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
    class ConstEachIterator {
    public:
        ConstEachIterator() = delete;
        ConstEachIterator(const ECS& ecs, std::vector<Entity>::const_iterator inIter) : ecs(ecs), it(inIter) {
            while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it))
                it++;
        }

        ConstEachIterator(ConstEachIterator& rhs) { ecs = rhs.ecs; it = rhs.it; }
        ConstEachIterator(ConstEachIterator&& rhs) { ecs = rhs.ecs; it = rhs.it; }

        using iterator_category = std::forward_iterator_tag;
        using value_type = std::tuple<Entity, Components&...>;

        bool operator==(const ConstEachIterator& rhs) { return it == rhs.it; }
        bool operator!=(const ConstEachIterator& rhs) { return it != rhs.it; }

        ConstEachIterator& operator++() {
            do {
                it++;
            } while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it));

            return *this;
        }

        ConstEachIterator operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        auto operator*() -> std::tuple<Entity, const Components&...> {
            return std::tuple_cat(std::make_tuple(*it), ecs.Get<Components...>(*it));
        }

    private:
        const ECS& ecs;
        std::vector<Entity>::const_iterator it;
    };

    template <typename ...Components>
    class ComponentView {
    public:
        ComponentView(ECS& ecs) : ecs(ecs) {}
        ComponentView(ComponentView& rhs) { ecs = rhs.ecs; }

        Entity Front() const { return NULL_ENTITY; } // TODO: pls fix
        bool IsEmpty() const { return begin() == end(); }

        auto begin() { return EachIterator<Components...>(ecs, ecs.m_Entities.begin()); }
        auto end() { return EachIterator<Components...>(ecs, ecs.m_Entities.end()); }

    private:
        ECS& ecs;
    };

    template <typename ...Components>
    class ConstComponentView {
    public:
        ConstComponentView(const ECS& ecs) : ecs(ecs) {}
        ConstComponentView(ConstComponentView& rhs) { ecs = rhs.ecs; }

        Entity Front() const { return NULL_ENTITY; } // TODO: pls fix
        bool IsEmpty() const { return begin() == end(); }

        auto begin() const { return ConstEachIterator<Components...>(ecs, ecs.m_Entities.begin()); }
        auto end() const { return ConstEachIterator<Components...>(ecs, ecs.m_Entities.end()); }

    private:
        const ECS& ecs;
    };

    template<typename ...Components>
    ComponentView<Components...> GetView() {
        return ComponentView<Components...>(*this);
    }
    
    template<typename ...Components>
    auto Each() -> decltype(auto) {
        if constexpr (sizeof...(Components) == 1)
            return GetSparseSet<Components...>()->Each();
        else
            return ComponentView<Components...>(*this);
    }

    template<typename ...Components>
    auto Each() const -> decltype(auto) {
        if constexpr (sizeof...(Components) == 1)
            return GetSparseSet<Components...>()->Each();
        else
            return ConstComponentView<Components...>(*this);
    }

    template<typename Fn>
    void Visit(Entity inEntity, Fn&& inVisitFunc) const {
        for (const auto& [type_id, sparse_set] : m_Components) {
            if (sparse_set->Contains(inEntity))
                inVisitFunc(type_id);
        }
    }

    bool IsValid(Entity inEntity) const {
        if (inEntity == NULL_ENTITY)
            return false;

        for (auto entity : m_Entities)
            if (entity == inEntity)
                return true;

        return false;
    }

    bool IsEmpty() const { return m_Entities.empty(); }

protected:
    std::vector<Entity> m_Entities;
    mutable std::unordered_map<uint32_t, ISparseSet*> m_Components;
};

void RunTests();

} // raekor