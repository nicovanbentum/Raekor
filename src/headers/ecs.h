#pragma once

#include "buffer.h"
#include "mesh.h"
#include "texture.h"
#include "math.h"

namespace Raekor {
namespace ECS {

typedef uint32_t Entity;

static Entity newEntity() {
    static Entity next = 0;
    return ++next;
}

struct TransformComponent {
    glm::vec3 position      = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale         = { 1.0f, 1.0f, 1.0f };
    glm::vec3 rotation      = { 0.0f, 0.0f, 0.0f };

    glm::mat4 matrix        = glm::mat4(1.0f);

    glm::vec3 localPosition = { 0.0f, 0.0f, 0.0f };

    void recalculateMatrix() {
        matrix = glm::translate(glm::mat4(1.0f), position);
        auto rotationQuat = static_cast<glm::quat>(rotation);
        matrix = matrix * glm::toMat4(rotationQuat);
        matrix = glm::scale(matrix, scale);
    }
};

struct LightComponent {
    enum class Type {
        DIRECTIONAL, POINT
    };

    glm::vec3 position;
    glm::vec3 colour;


};

struct MeshComponent {
    struct subMesh {
        ECS::Entity material = NULL;
        uint32_t indexOffset;
        uint32_t indexCount;
    };

    std::vector<subMesh> subMeshes;
    
    std::vector<Vertex> vertices;
    std::vector<Face> indices;
    glVertexBuffer vertexBuffer;
    glIndexBuffer indexBuffer;

    std::array<glm::vec3, 2> aabb;

    void uploadVertices() {
        vertexBuffer.loadVertices(vertices.data(), vertices.size());
        vertexBuffer.setLayout({
            {"POSITION",    ShaderType::FLOAT3},
            {"UV",          ShaderType::FLOAT2},
            {"NORMAL",      ShaderType::FLOAT3},
            {"TANGENT",     ShaderType::FLOAT3},
            {"BINORMAL",    ShaderType::FLOAT3}
        });
    }

    void uploadIndices() {
        indexBuffer.loadFaces(indices.data(), indices.size());
    }
};

struct MeshRendererComponent {

};

struct MaterialComponent {
    std::unique_ptr<glTexture2D> albedo;
    std::unique_ptr<glTexture2D> normals;
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
    Entity createObject(const std::string& name);

    MeshComponent&  addMesh() {
        Entity entity = createObject("Mesh");
        return meshes.create(entity);
    }

    void remove(ECS::Entity entity) {
        names.remove(entity);
        transforms.remove(entity);
        meshes.remove(entity);
        materials.remove(entity);
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

class AssimpImporter {
public:
    void loadFromDisk(ECS::Scene& scene, const std::string& file);

private:
    void processAiNode(ECS::Scene& scene, const aiScene* aiscene, aiNode* node);

    // TODO: every mesh in the file is created as an Entity that has 1 name, 1 mesh and 1 material component
    // we might want to incorporate meshrenderers and seperate entities for materials
    void loadMesh(ECS::Scene& scene, aiMesh* assimpMesh, aiMaterial* assimpMaterial, aiMatrix4x4 localTransform);

    void loadTexturesAsync(const aiScene* scene, const std::string& directory);

private:
    std::unordered_map<std::string, Stb::Image> images;
};

static Entity pickMesh(ComponentManager<MeshComponent>& meshes, ComponentManager<TransformComponent>& transforms, Math::Ray& ray) {
    // if this never gets a value we conclude there were no hits
    std::optional<float> shortestDistance;
    ECS::Entity result = NULL;

    for (size_t i = 0; i < meshes.getCount(); i++) {
        ECS::Entity entity = meshes.getEntity(i);

        MeshComponent& mesh = meshes[i];

        // get the OBB transformation matrix
        TransformComponent* transform = transforms.getComponent(entity);
        const glm::mat4& worldTransform = transform ? transform->matrix : glm::mat4(1.0f);

        // convert AABB from local to world space
        std::array<glm::vec3, 2> worldAABB = {
            worldTransform* glm::vec4(mesh.aabb[0], 1.0),
            worldTransform * glm::vec4(mesh.aabb[1], 1.0)
        };

        // if the camera is inside a mesh's world AABB we skip it
        if (Math::pointInAABB(ray.origin, worldAABB[0], worldAABB[1])) {
            continue;
        }

        // check for ray hit
        auto hitResult = ray.hitsOBB(mesh.aabb[0], mesh.aabb[1], worldTransform);

        // if we hit do something
        if (hitResult.has_value()) {
            // if its the first iteration we init the shortest distance
            if (!shortestDistance.has_value()) {
                shortestDistance = hitResult;
                result = entity;
            } // every other iteration we check if the hits distance is shorter and update the result
            else if (hitResult.value() < shortestDistance.value()) {
                shortestDistance = hitResult;
                result = entity;
            }
        }
    }

    return result;
}

} // ecs
} // raekor