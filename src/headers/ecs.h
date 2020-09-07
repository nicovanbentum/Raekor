#pragma once

#include "buffer.h"
#include "mesh.h"
#include "rmath.h"

namespace Raekor {
namespace ecs {

    template<class ...Component>
    using view = entt::basic_view <entt::entity, entt::exclude_t<>, Component... > ;
    
    /*
    Old entity component system pre-dating the integration of entt
    With this you can create a scene class that contains multiple ComponentManager's, like:
    class Scene {
        ComponentManager<NameComponent> names;
        ComponentManager<MeshComponent> meshes;
    }

    It worked well but the need for a manager for each component resulted in an ugly API that wasn't that great to work with.
*/

typedef uint32_t Entity;

static Entity newEntity() {
    static Entity next = 0;
    return ++next;
}

template<typename ComponentType>
class ComponentManager {
public:
    inline bool contains(Entity entity) {
        return lookup.find(entity) != lookup.end();
    }

    ComponentType& create(Entity entity) {
        m_assert(entity != NULL, "Can't create component for null entity.");
        
        // add component
        components.push_back(ComponentType());

        // update the entity's component index
        lookup[entity] = components.size() - 1;

        // save the entity
        entities.push_back(entity);

        return components.back();
    }

    inline size_t getCount() const { return components.size(); }

    inline ComponentType& operator[](size_t index) { return components[index]; }
    inline const ComponentType& operator[](size_t index) const { return components[index]; }
    
    inline Entity getEntity(size_t index) { return entities[index]; }

    ComponentType* getComponent(Entity entity) {
        if (auto it = lookup.find(entity); it != lookup.end()) {
            return &components[it->second];
        }
        return nullptr;
    }

    void remove(Entity entity) {
        auto it = lookup.find(entity);
        if (it != lookup.end())
        {
            // Directly index into components and entities array:
            const size_t index = it->second;
            const Entity entity = entities[index];

            if (index < components.size() - 1)
            {
                // Swap out the dead element with the last one:
                components[index] = std::move(components.back()); // try to use move instead of copy
                entities[index] = entities.back();

                // Update the lookup table:
                lookup[entities[index]] = index;
            }

            // Shrink the container:
            components.pop_back();
            entities.pop_back();
            lookup.erase(entity);
        }
    }

    void clear() {
        components.clear();
        entities.clear();
        lookup.clear();
    }

private:
    std::vector< ComponentType> components;
    std::vector<Entity> entities;
    std::unordered_map<Entity, size_t> lookup;

};

} // ecs
} // raekor