#pragma once

#include "ecs.h"

namespace Raekor {
namespace ECS {

struct TransformComponent {
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };

    glm::mat4 matrix = glm::mat4(1.0f);

    glm::vec3 localPosition = { 0.0f, 0.0f, 0.0f };

    void recalculateMatrix();
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

    void generateAABB();
    
    void uploadVertices();
    void uploadIndices();
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

} // ECS
} // raekor