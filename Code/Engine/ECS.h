#pragma once

#include "RTTI.h"
#include "Iter.h"
#include "Archive.h"

namespace RK {

enum Entity : uint32_t 
{ 
    Null = UINT32_MAX 
};

RTTI_DECLARE_TYPE_PRIMITIVE(Entity);

using EntityHierarchy = BinaryRelations::OneToMany<Entity, Entity>;

template<typename T>
class ComponentStorage;

class IComponentStorage
{
public:
	virtual ~IComponentStorage() = default;

	virtual RTTI&   GetRTTI() const = 0;

	virtual void    WriteTable(File& ioFile) const = 0;
	virtual bool    ReadTable(File& ioFile) = 0;

	virtual void    Clear() = 0;
	virtual size_t  Length() const = 0;
	virtual void	Add(Entity inEntity) = 0;
	virtual void    Remove(Entity inEntity) = 0;
	virtual bool    Contains(Entity inEntity) const = 0;
	virtual void	Copy(Entity inFrom, Entity inTo) = 0;
	virtual void	Move(IComponentStorage& ioFrom, Entity inFrom, Entity inTo) = 0;
	virtual UniquePtr<IComponentStorage> Clone() const = 0;

	virtual void    Read(BinaryReadArchive& inArchive) = 0;
	virtual void    Read(JSON::ReadArchive& inArchive) = 0;
	virtual void	Read(Entity inEntity, BinaryReadArchive& inArchive) = 0;
	virtual void	Read(Entity inEntity, JSON::ReadArchive& inArchive) = 0;

	virtual void    Write(BinaryWriteArchive& ioArchive) = 0;
	virtual void    Write(JSON::WriteArchive& ioArchive) = 0;
	virtual void	Write(Entity inEntity, BinaryWriteArchive& inArchive) = 0;
	virtual void	Write(Entity inEntity, JSON::WriteArchive& inArchive) = 0;

	bool IsEmpty() const { return Length() == 0; }

	template<typename T> 
    ComponentStorage<T>* GetDerived() { return static_cast<ComponentStorage<T>*>( this ); }

	template<typename T> 
    ComponentStorage<T>* GetDerived() const { return static_cast<ComponentStorage<T>*>( this ); }

};

template<typename T>
class ComponentStorage : public IComponentStorage
{
	class EachIterator
	{
	public:
		EachIterator() = delete;
		EachIterator(typename Array<T>::iterator t_iter, typename Array<Entity>::iterator e_iter)
			: t_iter(t_iter), e_iter(e_iter)
		{
		}

		using value_type = std::tuple<Entity, T&>;
		using iterator_category = std::forward_iterator_tag;

		bool operator==(const EachIterator& rhs) { return ( t_iter == rhs.t_iter ) && ( e_iter == rhs.e_iter ); }
		bool operator!=(const EachIterator& rhs) { return ( t_iter != rhs.t_iter ) && ( e_iter != rhs.e_iter ); }

		EachIterator& operator++()
		{
			t_iter++;
			e_iter++;
			return *this;
		}

		EachIterator operator++(int)
		{
			auto tmp = *this;
			++*this;
			return tmp;
		}

		auto operator*() -> value_type
		{
			return std::forward_as_tuple(*e_iter, *t_iter);
		}

	private:
		typename Array<T>::iterator t_iter;
		typename Array<Entity>::iterator e_iter;
	};

	class ConstEachIterator
	{
	public:
		ConstEachIterator() = delete;
		ConstEachIterator(typename Array<T>::const_iterator t_iter, typename Array<Entity>::const_iterator e_iter)
			: t_iter(t_iter), e_iter(e_iter)
		{
		}

		using value_type = std::tuple<Entity, const T&>;
		using iterator_category = std::forward_iterator_tag;

		bool operator==(const ConstEachIterator& rhs) { return ( t_iter == rhs.t_iter ) && ( e_iter == rhs.e_iter ); }
		bool operator!=(const ConstEachIterator& rhs) { return ( t_iter != rhs.t_iter ) && ( e_iter != rhs.e_iter ); }

		ConstEachIterator& operator++()
		{
			t_iter++;
			e_iter++;
			return *this;
		}

		ConstEachIterator operator++(int)
		{
			auto tmp = *this;
			++*this;
			return tmp;
		}

		auto operator*() -> value_type
		{
			return std::forward_as_tuple(*e_iter, *t_iter);
		}

	private:
		typename std::vector<T>::const_iterator t_iter;
		typename std::vector<Entity>::const_iterator e_iter;
	};

	class View
	{
	public:
		View(ComponentStorage<T>& storage) : storage(storage) {}
		View(View& rhs) { storage = rhs.storage; }

		auto begin() { return EachIterator(storage.m_Components.begin(), storage.m_Entities.begin()); }
		auto end() { return EachIterator(storage.m_Components.end(), storage.m_Entities.end()); }

	private:
		ComponentStorage<T>& storage;
	};

	class ConstView
	{
	public:
		ConstView(const ComponentStorage<T>& storage) : storage(storage) {}
		ConstView(View& rhs) { storage = rhs.storage; }

		auto begin() { return ConstEachIterator(storage.m_Components.begin(), storage.m_Entities.begin()); }
		auto end() { return ConstEachIterator(storage.m_Components.end(), storage.m_Entities.end()); }

	private:
		const ComponentStorage<T>& storage;
	};

public:
	virtual ~ComponentStorage() { Clear(); }

	RTTI& GetRTTI() const override final { return RTTI_OF<T>(); }

	void WriteTable(File& ioFile) const override final
	{
		WriteFileBinary(ioFile, m_Entities);

		const RTTI& rtti = RTTI_OF<T>();

		uint32_t member_count = 0;
		for (const auto& member : rtti)
			member_count += ( member->GetSerializeType() & SERIALIZE_BINARY ) != 0;

		WriteFileBinary(ioFile, member_count);

		for (const auto& member : rtti)
		{
			if (( member->GetSerializeType() & SERIALIZE_BINARY ) == 0)
				continue;

			WriteFileBinary(ioFile, member->GetCustomNameHash());

			const uint64_t size_position = uint64_t(ioFile.tellp());
			WriteFileBinary(ioFile, uint64_t(0));

			for (const T& component : m_Components)
				member->ToBinary(ioFile, &component);

			const uint64_t end_position = uint64_t(ioFile.tellp());
			const uint64_t column_size = end_position - size_position - sizeof(uint64_t);

			ioFile.seekp(size_position);
			WriteFileBinary(ioFile, column_size);
			ioFile.seekp(end_position);
		}
	}

	bool ReadTable(File& ioFile) override final
	{
		Clear();

		ReadFileBinary(ioFile, m_Entities);
		m_Components.resize(m_Entities.size());

		for (const auto& [index, entity] : gEnumerate(m_Entities))
		{
			if (m_Sparse.size() <= entity)
				m_Sparse.resize(entity + 1);

			m_Sparse[entity] = uint32_t(index);
		}

		const RTTI& rtti = RTTI_OF<T>();
		bool all_members_read = true;

		uint32_t member_count = 0;
		ReadFileBinary(ioFile, member_count);

		for (uint32_t member_index = 0; member_index < member_count; member_index++)
		{
			uint32_t name_hash = 0;
			uint64_t column_size = 0;
			ReadFileBinary(ioFile, name_hash);
			ReadFileBinary(ioFile, column_size);

			const uint64_t column_start = uint64_t(ioFile.tellg());

			Member* member = nullptr;

			for (const auto& rtti_member : rtti)
			{
				if (rtti_member->GetCustomNameHash() == name_hash && ( rtti_member->GetSerializeType() & SERIALIZE_BINARY ))
				{
					member = rtti_member.get();
					break;
				}
			}

			if (member)
			{
				try
				{
					for (T& component : m_Components)
						member->FromBinary(ioFile, &component);
				}
				catch (const std::exception&)
				{
					ioFile.setstate(std::ios::failbit);
				}
			}

			if (!ioFile || uint64_t(ioFile.tellg()) != column_start + column_size)
			{
				if (member)
				{
					all_members_read = false;
					gLogWarning("ECS", "Member \"{}\" of component {} changed its layout, it was not loaded", member->GetCustomName(), rtti.GetTypeName());

					const T default_component = T();
					for (T& component : m_Components)
						member->CopyValue(&default_component, &component);
				}

				ioFile.clear();
			}

			ioFile.seekg(column_start + column_size);
		}

		return all_members_read;
	}

	template<typename U>
	T& Insert(Entity entity, U&& t)
	{
		if (Contains(entity))
		{
			T& existing_t = Get(entity);
			existing_t = std::forward<U>(t);
			return existing_t;
		}

		m_Entities.push_back(entity);

		// grow the m_Sparse vector exponentially when we need to make room
		if (m_Sparse.size() <= entity)
		{
			auto sparse_temp = m_Sparse;
			m_Sparse.resize(entity + 1);
			memcpy(m_Sparse.data(), sparse_temp.data(), sparse_temp.size());
		}

		m_Sparse[entity] = uint32_t(m_Entities.size() - 1);
		return m_Components.emplace_back(std::forward<U>(t));
	}

	T& Get(Entity entity)
	{
		return m_Components[m_Sparse[entity]];
	}

	const T& Get(Entity entity) const
	{
		return m_Components[m_Sparse[entity]];
	}

	int GetPackedIndex(Entity entity) const
	{
		if (!Contains(entity))
			return -1;

		return int(m_Sparse[entity]);
	}

	void Copy(Entity inFrom, Entity inTo) override final
	{
		T component = Get(inFrom);
		Insert(inTo, component);
	}

	void Move(IComponentStorage& ioFrom, Entity inFrom, Entity inTo) override final
	{
		Insert(inTo, std::move(ioFrom.GetDerived<T>()->Get(inFrom)));
	}

	UniquePtr<IComponentStorage> Clone() const override final
	{
		return std::make_unique<ComponentStorage<T>>(*this);
	}

	void Add(Entity inEntity) override final
	{
		Insert(inEntity, T {});
	}

	void Remove(Entity entity) override final
	{
		if (!Contains(entity))
			return;

		// set the current component to whatever is in the back of the m_Components
		m_Components[m_Sparse[entity]] = m_Components.back();
		// set the current entity (packed) to whatever is in the back of the packed m_Entities
		m_Entities[m_Sparse[entity]] = m_Entities.back();
		m_Sparse[m_Entities.back()] = m_Sparse[entity];

		m_Components.pop_back();
		m_Entities.pop_back();
	}

	bool Contains(Entity entity) const override final
	{
		if (entity >= m_Sparse.size())
			return false;

		if (m_Sparse[entity] >= m_Entities.size())
			return false;

		return m_Entities[m_Sparse[entity]] == entity;
	}

	void Clear() override final
	{
		m_Sparse.clear();
		m_Entities.clear();
		m_Components.clear();
	}

	size_t Length() const override final { return m_Components.size(); }

	void Read(BinaryReadArchive& ioArchive) override final
	{
		ReadFileBinary(ioArchive.GetFile(), m_Entities);
		ReadFileBinary(ioArchive.GetFile(), m_Sparse);

		size_t storage_size = 0ull;
		ReadFileBinary(ioArchive.GetFile(), storage_size);
		m_Components.resize(storage_size);

		for (T& component : m_Components)
			ioArchive >> component;
	}
	void Read(JSON::ReadArchive& ioArchive) override final {}

	void Read(Entity inEntity, BinaryReadArchive& ioArchive) override final
	{
		if (!Contains(inEntity))
			return;

		ioArchive >> Get(inEntity);
	}

	void Read(Entity inEntity, JSON::ReadArchive& ioArchive) override final
	{
		if (!Contains(inEntity))
			return;

		ioArchive >> Get(inEntity);
	}

	void Write(BinaryWriteArchive& ioArchive) override final
	{
		WriteFileBinary(ioArchive.GetFile(), m_Entities);
		WriteFileBinary(ioArchive.GetFile(), m_Sparse);

		WriteFileBinary(ioArchive.GetFile(), m_Components.size());

		for (const T& component : m_Components)
			ioArchive << component;
	}

	void Write(Entity inEntity, BinaryWriteArchive& ioArchive) override final
	{
		if (!Contains(inEntity))
			return;

		ioArchive << Get(inEntity);
	}

	void Write(Entity inEntity, JSON::WriteArchive& ioArchive) override final
	{
		if (!Contains(inEntity))
			return;

		ioArchive << Get(inEntity);
	}

	void Write(JSON::WriteArchive& ioArchive) override final {}

	View Each() { return View(*this); }
	ConstView Each() const { return ConstView(*this); }

	const Array<T>& GetComponents() const { return m_Components; }
	const Array<Entity>& GetEntities() const { return m_Entities; }

	auto begin() { return EachIterator(m_Components.begin(), m_Entities.begin()); }
	auto end() { return EachIterator(m_Components.end(), m_Entities.end()); }

	Array<T> m_Components;
	Array<Entity> m_Entities;
	Array<uint32_t> m_Sparse;
};



class ComponentRegistry
{
public:
	using CreateStorageFunction = UniquePtr<IComponentStorage>(*)();

	struct Entry
	{
		RTTI* mRTTI = nullptr;
		CreateStorageFunction mCreateStorage = nullptr;
	};

	template<typename Component>
	void Register()
	{
		RTTI& rtti = RTTI_OF<Component>();
		g_RTTIFactory.Register(rtti);

		m_Entries[rtti.GetHash()] = Entry
		{
			.mRTTI = &rtti,
			.mCreateStorage = []() -> UniquePtr<IComponentStorage> { return std::make_unique<ComponentStorage<Component>>(); }
		};
	}

	const Entry* Find(uint32_t inHash) const
	{
		const auto entry = m_Entries.find(inHash);
		return entry != m_Entries.end() ? &entry->second : nullptr;
	}

	auto begin() const { return m_Entries.begin(); }
	auto end() const { return m_Entries.end(); }

private:
	HashMap<uint32_t, Entry> m_Entries;
};

extern RK_API ComponentRegistry g_ComponentRegistry;


class ECStorage
{
public:
	ECStorage()
	{
		for (const auto& [hash, entry] : g_ComponentRegistry)
			m_Components[hash] = entry.mCreateStorage();
	}

	template<typename Component>
	ComponentStorage<Component>* GetComponentStorage()
	{
		return m_Components.at(RTTI_HASH<Component>())->GetDerived<Component>();
	}

	template<typename Component>
	const ComponentStorage<Component>* GetComponentStorage() const
	{
		return m_Components.at(RTTI_HASH<Component>())->GetDerived<Component>();
	}

	auto EachComponentStorage() { return std::views::values(m_Components); }

	template<typename Component>
	Slice<const Component> GetComponents()
	{
		if (ComponentStorage<Component>* storage = GetComponentStorage<Component>())
			return storage->GetComponents();
		return {};
	}

	Entity Create()
	{
		const Entity entity = Entity(m_NextEntity++);

		if (m_EntityIndices.size() <= entity)
			m_EntityIndices.resize(size_t(entity) + 1, UINT32_MAX);

		m_EntityIndices[entity] = uint32_t(m_Entities.size());
		return m_Entities.emplace_back(entity);
	}

	void Destroy(Entity inEntity)
	{
		for (auto& [type_id, components] : m_Components)
		{
			if (components->Contains(inEntity))
				components->Remove(inEntity);
		}

		if (!Exists(inEntity))
			return;

		const uint32_t index = m_EntityIndices[inEntity];
		const Entity last = m_Entities.back();

		m_Entities[index] = last;
		m_EntityIndices[last] = index;

		m_Entities.pop_back();
		m_EntityIndices[inEntity] = UINT32_MAX;
	}

	template<typename Component>
	Component& Add(Entity entity)
	{
		return GetComponentStorage<Component>()->Insert(entity, Component());
	}

	template<typename Component>
	Component& Add(Entity inEntity, const Component& inComponent)
	{
		return GetComponentStorage<Component>()->Insert(inEntity, inComponent);
	}

	void Add(Entity inEntity, const RTTI* inRTTI)
	{
		m_Components[inRTTI->GetHash()]->Add(inEntity);
	}

	template<typename Component>
	void Register()
	{
		EnsureExists<Component>();
	}

	template<typename Component>
	Component& _GetInternal(Entity inEntity)
	{
		return GetComponentStorage<Component>()->Get(inEntity);
	}

	template<typename Component>
	const Component& _GetInternal(Entity inEntity) const
	{
		return GetComponentStorage<Component>()->Get(inEntity);
	}

	template<typename Component>
	uint32_t Count() const
	{
		return uint32_t(GetComponentStorage<Component>()->Length());
	}

	template<typename Component>
	bool Any() const
	{
		return m_Components.contains(RTTI_HASH<Component>()) && Count<Component>() > 0;
	}

	template<typename ...Components>
	auto Get(Entity entity) -> decltype( auto )
	{
		if constexpr (sizeof...( Components ) == 1)
			return _GetInternal<Components...>(entity);
		else
			return std::tie(_GetInternal<Components>(entity)...);
	}

	template<typename ...Components>
	auto Get(Entity entity) const -> decltype( auto )
	{
		if constexpr (sizeof...( Components ) == 1)
			return _GetInternal<Components...>(entity);
		else
			return std::tie(_GetInternal<Components>(entity)...);
	}

	template<typename Component>
	const Component* GetPtr(Entity inEntity) const
	{
		if (Has<Component>(inEntity))
			return &GetComponentStorage<Component>()->Get(inEntity);
		else
			return nullptr;
	}

	template<typename Component>
	Component* GetPtr(Entity inEntity)
	{
		if (Has<Component>(inEntity))
			return &GetComponentStorage<Component>()->Get(inEntity);
		else
			return nullptr;
	}

	template<typename Component>
	const Array<Component>& GetStorage() const
	{
		return GetComponentStorage<Component>()->GetComponents();
	}

	template<typename Component>
	const Array<Entity>& GetEntities() const
	{
		return GetComponentStorage<Component>()->GetEntities();
	}

	template<typename Component>
	uint32_t GetPackedIndex(Entity inEntity) const
	{
		return GetComponentStorage<Component>()->GetPackedIndex(inEntity);
	}

	void Clear()
	{
		for (const auto& [type_id, components] : m_Components)
			components->Clear();

		m_Entities.clear();
		m_EntityIndices.clear();
		m_NextEntity = 0;
	}

	template<typename ...Components>
	bool Has(Entity inEntity) const
	{
        if (inEntity == Entity::Null)
            return false;

        bool has_all = true;

        ( ..., [&]()
        {
            if (!GetComponentStorage<Components>()->Contains(inEntity))
                has_all = false;
        }( ) );

        return has_all;
	}

	bool Has(Entity inEntity, const RTTI* inRTTI)
	{
		if (!m_Components.contains(inRTTI->GetHash()))
			return false;

		return m_Components[inRTTI->GetHash()]->Contains(inEntity);
	}

	bool Exists(Entity inEntity) const
	{
		if (inEntity == Entity::Null || inEntity >= m_EntityIndices.size())
			return false;

		const uint32_t index = m_EntityIndices[inEntity];
		return index < m_Entities.size() && m_Entities[index] == inEntity;
	}

	template<typename Component>
	void Remove(Entity entity)
	{
		GetComponentStorage<Component>()->Remove(entity);
	}

	friend class EachIterator;
	friend class ConstEachIterator;

	template <typename ...Components>
	class EachIterator
	{
	public:
		EachIterator() = delete;
		EachIterator(ECStorage& ecs, Array<Entity>::iterator inIter) : ecs(ecs), it(inIter)
		{
			while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it))
				it++;
		}

		EachIterator(EachIterator& rhs) { ecs = rhs.ecs; it = rhs.it; }
		EachIterator(EachIterator&& rhs) { ecs = rhs.ecs; it = rhs.it; }

		using iterator_category = std::forward_iterator_tag;
		using value_type = std::tuple<Entity, Components&...>;

		bool operator==(const EachIterator& rhs) { return it == rhs.it; }
		bool operator!=(const EachIterator& rhs) { return it != rhs.it; }

		EachIterator& operator++()
		{
			do
			{
				it++;
			} while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it));

			return *this;
		}

		EachIterator operator++(int)
		{
			auto tmp = *this;
			++*this;
			return tmp;
		}

		auto operator*() -> std::tuple<Entity, Components&...>
		{
			return std::tuple_cat(std::make_tuple(*it), ecs.Get<Components...>(*it));
		}

	private:
		ECStorage& ecs;
		Array<Entity>::iterator it;
	};

	template <typename ...Components>
	class ConstEachIterator
	{
	public:
		ConstEachIterator() = delete;
		ConstEachIterator(const ECStorage& ecs, Array<Entity>::const_iterator inIter) : ecs(ecs), it(inIter)
		{
			while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it))
				it++;
		}

		ConstEachIterator(ConstEachIterator& rhs) { ecs = rhs.ecs; it = rhs.it; }
		ConstEachIterator(ConstEachIterator&& rhs) { ecs = rhs.ecs; it = rhs.it; }

		using iterator_category = std::forward_iterator_tag;
		using value_type = std::tuple<Entity, Components&...>;

		bool operator==(const ConstEachIterator& rhs) { return it == rhs.it; }
		bool operator!=(const ConstEachIterator& rhs) { return it != rhs.it; }

		ConstEachIterator& operator++()
		{
			do
			{
				it++;
			} while (it != ecs.m_Entities.end() && !ecs.Has<Components...>(*it));

			return *this;
		}

		ConstEachIterator operator++(int)
		{
			auto tmp = *this;
			++*this;
			return tmp;
		}

		auto operator*() -> std::tuple<Entity, const Components&...>
		{
			return std::tuple_cat(std::make_tuple(*it), ecs.Get<Components...>(*it));
		}

	private:
		const ECStorage& ecs;
		Array<Entity>::const_iterator it;
	};

	template <typename ...Components>
	class ComponentView
	{
	public:
		ComponentView(ECStorage& ecs) : ecs(ecs) {}
		ComponentView(ComponentView& rhs) { ecs = rhs.ecs; }

		Entity Front() const { return Entity::Null; } // TODO: pls fix
		bool IsEmpty() const { return begin() == end(); }

		auto begin() { return EachIterator<Components...>(ecs, ecs.m_Entities.begin()); }
		auto end() { return EachIterator<Components...>(ecs, ecs.m_Entities.end()); }

	private:
		ECStorage& ecs;
	};

	template <typename ...Components>
	class ConstComponentView
	{
	public:
		ConstComponentView(const ECStorage& ecs) : ecs(ecs) {}
		ConstComponentView(ConstComponentView& rhs) { ecs = rhs.ecs; }

		Entity Front() const { return Entity::INVALID; } // TODO: pls fix
		bool IsEmpty() const { return begin() == end(); }

		auto begin() const { return ConstEachIterator<Components...>(ecs, ecs.m_Entities.begin()); }
		auto end() const { return ConstEachIterator<Components...>(ecs, ecs.m_Entities.end()); }

	private:
		const ECStorage& ecs;
	};

	template<typename ...Components>
	ComponentView<Components...> GetView()
	{
		return ComponentView<Components...>(*this);
	}

	template<typename ...Components>
	auto Each() -> decltype( auto )
	{
		if constexpr (sizeof...( Components ) == 1)
			return GetComponentStorage<Components...>()->Each();
		else
			return ComponentView<Components...>(*this);
	}

	template<typename ...Components>
	auto Each() const -> decltype( auto )
	{
		if constexpr (sizeof...( Components ) == 1)
			return GetComponentStorage<Components...>()->Each();
		else
			return ConstComponentView<Components...>(*this);
	}

	template<typename Fn>
	void Visit(Entity inEntity, Fn&& inVisitFunc) const
	{
		for (const auto& [type_id, sparse_set] : m_Components)
		{
			if (sparse_set->Contains(inEntity))
				inVisitFunc(type_id);
		}
	}

	template<typename Component>
	void EnsureExists()
	{
		if (!m_Components.contains(RTTI_HASH<Component>()))
			m_Components[RTTI_HASH<Component>()] = std::make_unique<ComponentStorage<Component>>();
	}

	IComponentStorage* GetComponentStorage(uint32_t inHash)
	{
		const auto storage = m_Components.find(inHash);
		return storage != m_Components.end() ? storage->second.get() : nullptr;
	}

	template<typename Component>
	bool Contains(Entity inEntity) const
	{
		return GetComponentStorage<Component>()->Contains(inEntity);
	}

	const Array<Entity>& GetEntities() const { return m_Entities; }

    auto begin() { return std::begin(m_Components); }
    auto end() { return std::end(m_Components); }

	bool IsEmpty() const { return m_Entities.empty(); }

	void Swap(ECStorage& ioOther)
	{
		std::swap(m_Entities, ioOther.m_Entities);
		std::swap(m_EntityIndices, ioOther.m_EntityIndices);
		std::swap(m_NextEntity, ioOther.m_NextEntity);
		std::swap(m_Components, ioOther.m_Components);
	}

	void CopyFrom(const ECStorage& inOther)
	{
		m_Entities = inOther.m_Entities;
		m_EntityIndices = inOther.m_EntityIndices;
		m_NextEntity = inOther.m_NextEntity;

		for (const auto& [hash, storage] : inOther.m_Components)
			m_Components[hash] = storage->Clone();
	}

protected:
	void RebuildEntityIndices()
	{
		m_NextEntity = 0;
		m_EntityIndices.clear();

		for (Entity entity : m_Entities)
			m_NextEntity = std::max(m_NextEntity, uint32_t(entity) + 1);

		m_EntityIndices.resize(m_NextEntity, UINT32_MAX);

		for (uint32_t index = 0; index < m_Entities.size(); index++)
			m_EntityIndices[m_Entities[index]] = index;
	}

	Array<Entity> m_Entities;
	Array<uint32_t> m_EntityIndices;
	uint32_t m_NextEntity = 0;
	mutable HashMap<uint32_t, UniquePtr<IComponentStorage>> m_Components;
};

void RunECStorageTests();

} // raekor