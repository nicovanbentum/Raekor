#pragma once

#include "buffer.h"
#include "mesh.h"
#include "texture.h"

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
    struct subMesh {
        ECS::Entity material = NULL;
        uint32_t indexOffset;
        uint32_t indexCount;
    };

    std::string name = "NULL";
    Shape shape;

    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    std::vector<subMesh> subMeshes;

    glVertexBuffer vertexBuffer;
    glIndexBuffer indexBuffer;

    void init(Shape basicShape = Shape::None) {
        switch (basicShape) {
            case Shape::None: name = ""; break;
            case Shape::Cube: name = "Cube"; break;
            case Shape::Quad: name = "Quad"; break;
        }

        if (basicShape == Shape::None) {
            return;
        }

        vertices = std::vector<Vertex>(cubeVertices);
        indices = std::vector<Index>(cubeIndices);

        // upload to GPU
        vertexBuffer.loadVertices(vertices.data(), vertices.size());
        indexBuffer.loadIndices(indices.data(), indices.size());

        std::puts("uploaded cube to gpu");
    }
};

struct MeshRendererComponent {

};

struct MaterialComponent {
    glTexture2D albedo;
    glTexture2D normals;
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

class Scene {
public:
    void createObject(const char* name);
    void attachCube(Entity entity);
    void loadFromFile(const std::string& file);

public:
    ComponentManager<NameComponent> names;
    ComponentManager<TransformComponent> transforms;
    ComponentManager<MeshComponent> meshes;
    ComponentManager<MeshRendererComponent> meshRenderers;
    ComponentManager<MaterialComponent> materials;

public:
    std::vector<Entity> entities;
};

// SYSTEMS TODO: turn this into classes and seperate files
// TODO: figure out how we do sub mesh transforms
static void renderGeometryWithMaterials(ComponentManager<MeshComponent> meshes, ComponentManager<MaterialComponent> materials) {
    for (size_t i = 0; i < meshes.getCount(); i++) {
        Entity entity = meshes.getEntity(i);
        auto mesh = meshes.getComponent(entity);
        auto material = materials.getComponent(entity);
        if (!mesh && !material) continue;
        
        mesh->vertexBuffer.bind();
        mesh->indexBuffer.bind();
        material->albedo.bindToSlot(0);
        material->normals.bindToSlot(3);

        // TODO: this only works because it's a per mesh vertex buffer,
        // in the future we want a single vertex buffer for imported geometry and use submeshes to index into that vertex buffer
        glDrawElements(GL_TRIANGLES, mesh->indexBuffer.count, GL_UNSIGNED_INT, nullptr);
    }
}

static void renderGeometry(ComponentManager<MeshComponent>& meshes, ComponentManager<TransformComponent>& transforms) {
    for (size_t i = 0; i < meshes.getCount(); i++) {
        Entity entity = meshes.getEntity(i);
        auto mesh = meshes.getComponent(entity);
        if (!mesh) continue;

        mesh->vertexBuffer.bind();
        mesh->indexBuffer.bind();
        
        // TODO: this only works because it's a per mesh vertex buffer,
        // in the future we want a single vertex buffer for imported geometry and use submeshes to index into that vertex buffer
        glDrawElements(GL_TRIANGLES, mesh->indexBuffer.count, GL_UNSIGNED_INT, nullptr);
    }
}

} // ecs
} // raekor