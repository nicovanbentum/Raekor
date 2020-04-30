#pragma once

namespace Raekor {
namespace ECS {

typedef uint32_t Entity;

static Entity newEntity() {
    static Entity next = 0;
    return ++next;
}

static Entity newNamedEntity(const char* name) {
    Entity entity = newEntity();

    
}

struct TransformComponent {
    glm::vec3 position;
    glm::vec3 scale;
    glm::vec3 rotation;
    glm::mat4 matrix;

    void recalculateMatrix() {
        matrix = glm::mat4(1.0f);
        matrix = glm::translate(glm::mat4(1.0f), position);
        auto rotationQuat = static_cast<glm::quat>(rotation);
        matrix = matrix * glm::toMat4(rotationQuat);
        matrix = glm::scale(matrix, scale);
    }
};

struct MeshComponent {

};

struct MeshRendererComponent {

};

struct MaterialComponent {

};

struct NameComponent {
    std::string name;
};

template<typename ComponentType>
class ComponentManager {
public:
    inline bool contains(Entity entity) {
        return lookup.find(entity) != lookup.end();
    }

    ComponentType& create(Entity entity) {
        if (entity == NULL) {
            throw std::runtime_error("entity is null");
        }
        
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
        if (it == lookup.end()) return;

        const size_t index = it->second;
        const Entity entity = entities[index];

        if (index < components.size() - 1) {
            components[index] = std::move(components.back());
            entities[index] = entities.back();
            
            lookup[entities[index]] = index;
        }

        components.pop_back();
        entities.pop_back();
        lookup.erase(entity);
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

class Scene {
public:
    void createObject(const char* name) {
        Entity entity = newEntity();
        names.create(entity).name = name;
        entities.push_back(entity);
    }

public:
    ComponentManager<NameComponent> names;
    ComponentManager<TransformComponent> transforms;
    ComponentManager<MeshComponent> meshes;
    ComponentManager<MeshRendererComponent> meshRenderers;
    ComponentManager<MaterialComponent> materials;

public:
    std::vector<Entity> entities;
};


} // ecs
} // raekor