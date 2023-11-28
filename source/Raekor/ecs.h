#pragma once

#include "rtti.h"
#include "archive.h"

namespace Raekor {

using Entity = uint32_t;;
constexpr auto NULL_ENTITY = Entity(UINT32_MAX);

}

namespace Raekor::ecs {

template<typename T>
class ComponentStorage;

class IComponentStorage
{
public:
	virtual ~IComponentStorage() = default;

	virtual void    Clear() = 0;
	virtual size_t  Length() const = 0;
	virtual void    Remove(Entity inEntity) = 0;
	virtual bool    Contains(Entity inEntity) const = 0;

	virtual void    Read(BinaryReadArchive& inArchive) = 0;
	virtual void    Read(JSON::ReadArchive& inArchive) = 0;
	virtual void    Write(BinaryWriteArchive& ioArchive) = 0;
	virtual void    Write(JSON::WriteArchive& ioArchive) = 0;

	bool IsEmpty() const { return Length() != 0; }

	template<typename T> ComponentStorage<T>* GetDerived() { return static_cast<ComponentStorage<T>*>( this ); }
	template<typename T> ComponentStorage<T>* GetDerived() const { return static_cast<ComponentStorage<T>*>( this ); }

};

template<typename T>
class ComponentStorage : public IComponentStorage
{
	class EachIterator
	{
	public:
		EachIterator() = delete;
		EachIterator(typename std::vector<T>::iterator t_iter, typename std::vector<Entity>::iterator e_iter)
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
		typename std::vector<T>::iterator t_iter;
		typename std::vector<Entity>::iterator e_iter;
	};

	class ConstEachIterator
	{
	public:
		ConstEachIterator() = delete;
		ConstEachIterator(typename std::vector<T>::const_iterator t_iter, typename std::vector<Entity>::const_iterator e_iter)
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
		View(ComponentStorage<T>& set) : set(set) {}
		View(View& rhs) { set = rhs.set; }

		auto begin() { return EachIterator(set.m_Components.begin(), set.m_Entities.begin()); }
		auto end() { return EachIterator(set.m_Components.end(), set.m_Entities.end()); }

	private:
		ComponentStorage<T>& set;
	};

	class ConstView
	{
	public:
		ConstView(const ComponentStorage<T>& set) : set(set) {}
		ConstView(View& rhs) { set = rhs.set; }

		auto begin() { return ConstEachIterator(set.m_Components.begin(), set.m_Entities.begin()); }
		auto end() { return ConstEachIterator(set.m_Components.end(), set.m_Entities.end()); }

	private:
		const ComponentStorage<T>& set;
	};

public:
	virtual ~ComponentStorage() { Clear(); }

	T& Insert(Entity entity, const T& t)
	{
		if (Contains(entity))
		{
			auto& existing_t = Get(entity);
			existing_t = t;
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
		return m_Components.emplace_back(t);
	}

	T& Get(Entity entity)
	{
		return m_Components[m_Sparse[entity]];
	}

	const T& Get(Entity entity) const
	{
		return m_Components[m_Sparse[entity]];
	}

	uint32_t GetSparseIndex(Entity entity) const
	{
		if (!Contains(entity))
			return UINT32_MAX;

		return m_Sparse[entity];
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
		m_Components.clear();
		m_Entities.clear();
	}

	size_t Length() const override final { return m_Components.size(); }

	void Read(BinaryReadArchive& ioArchive) override final
	{
		ReadFileBinary(ioArchive.GetFile(), m_Entities);
		ReadFileBinary(ioArchive.GetFile(), m_Sparse);

		auto storage_size = 0ull;
		ReadFileBinary(ioArchive.GetFile(), storage_size);
		m_Components.resize(storage_size);

		for (auto& component : m_Components)
			ioArchive >> component;
	}
	void Read(JSON::ReadArchive& ioArchive) override final {}

	void Write(BinaryWriteArchive& ioArchive) override final
	{
		WriteFileBinary(ioArchive.GetFile(), m_Entities);
		WriteFileBinary(ioArchive.GetFile(), m_Sparse);

		WriteFileBinary(ioArchive.GetFile(), m_Components.size());

		for (const auto& component : m_Components)
			ioArchive << component;
	}

	void Write(JSON::WriteArchive& ioArchive) override final {}

	View Each() { return View(*this); }
	ConstView Each() const { return ConstView(*this); }

	Slice<T> GetComponents() const { return Slice(m_Components.data(), m_Components.size()); }
	Slice<Entity> GetEntities() const { return Slice(m_Entities.data(), m_Entities.size()); }
	Slice<Entity> GetSparseEntities() const { return Slice(m_Sparse.data(), m_Sparse.size()); }

	auto begin() { return EachIterator(m_Components.begin(), m_Entities.begin()); }
	auto end() { return EachIterator(m_Components.end(), m_Entities.end()); }

	std::vector<T> m_Components;
	std::vector<Entity> m_Entities;
	std::vector<uint32_t> m_Sparse;
};



class ECS
{
public:
	template<typename Component>
	ComponentStorage<Component>* GetComponentStorage()
	{
		return m_Components.at(gGetTypeHash<Component>())->GetDerived<Component>();
	}

	template<typename Component>
	const ComponentStorage<Component>* GetComponentStorage() const
	{
		return m_Components.at(gGetTypeHash<Component>())->GetDerived<Component>();
	}

	Entity Create()
	{
		return m_Entities.emplace_back(uint32_t(m_Entities.size()));
	}

	void Destroy(Entity inEntity)
	{
		for (auto& [type_id, components] : m_Components)
		{
			if (components->Contains(inEntity))
				components->Remove(inEntity);
		}
	}

	template<typename Component>
	Component& Add(Entity entity)
	{
		return GetComponentStorage<Component>()->Insert(entity, Component());
	}

	template<typename Component>
	Component& Add(Entity entity, const Component& inComponent)
	{
		return GetComponentStorage<Component>()->Insert(entity, inComponent);
	}

	template<typename Component>
	void Register()
	{
		if (!m_Components.contains(gGetTypeHash<Component>()))
			m_Components[gGetRTTI<Component>().mHash] = new ecs::ComponentStorage<Component>();
	}

	template<typename Component>
	Component& _GetInternal(Entity entity)
	{
		return GetComponentStorage<Component>()->Get(entity);
	}

	template<typename Component>
	const Component& _GetInternal(Entity entity) const
	{
		return GetComponentStorage<Component>()->Get(entity);
	}

	template<typename Component>
	uint32_t Count() const
	{
		return uint32_t(GetComponentStorage<Component>()->Length());
	}

	template<typename Component>
	bool Any() const
	{
		return m_Components.contains(gGetTypeHash<Component>());
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
	Slice<Component> GetStorage() const
	{
		return GetComponentStorage<Component>()->GetComponents();
	}

	template<typename Component>
	Slice<Entity> GetEntities() const
	{
		return GetComponentStorage<Component>()->GetEntities();
	}

	template<typename Component>
	Slice<Entity> GetSparseEntities() const
	{
		return GetComponentStorage<Component>()->GetSparseEntities();
	}

	template<typename Component>
	uint32_t GetSparseIndex(Entity inEntity) const
	{
		return GetComponentStorage<Component>()->GetSparseIndex(inEntity);
	}

	void Clear()
	{
		for (const auto& [type_id, components] : m_Components)
			components->Clear();

		m_Entities.clear();
	}

	template<typename ...Components>
	bool Has(Entity entity) const
	{
		bool has_all = true;

		( ..., [&]()
		{
			if (!GetComponentStorage<Components>()->Contains(entity))
				has_all = false;
		}( ) );

		return has_all;
	}

	bool Exists(Entity inEntity) const
	{
		if (inEntity == NULL_ENTITY)
			return false;

		for (auto entity : m_Entities)
			if (entity == inEntity)
				return true;

		return false;
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
		EachIterator(ECS& ecs, std::vector<Entity>::iterator inIter) : ecs(ecs), it(inIter)
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
		ECS& ecs;
		std::vector<Entity>::iterator it;
	};

	template <typename ...Components>
	class ConstEachIterator
	{
	public:
		ConstEachIterator() = delete;
		ConstEachIterator(const ECS& ecs, std::vector<Entity>::const_iterator inIter) : ecs(ecs), it(inIter)
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
		const ECS& ecs;
		std::vector<Entity>::const_iterator it;
	};

	template <typename ...Components>
	class ComponentView
	{
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
	class ConstComponentView
	{
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
	bool Contains(Entity inEntity) const
	{
		return GetComponentStorage<Component>()->Contains(inEntity);
	}

	bool IsEmpty() const { return m_Entities.empty(); }

protected:
	std::vector<Entity> m_Entities;
	mutable std::unordered_map<size_t, IComponentStorage*> m_Components;
};

void RunTests();

} // raekor