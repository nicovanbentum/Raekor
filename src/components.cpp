#include "pch.h"
#include "components.h"

namespace Raekor {
namespace ECS {

void TransformComponent::recalculateMatrix() {
    matrix = glm::translate(glm::mat4(1.0f), position);
    auto rotationQuat = static_cast<glm::quat>(rotation);
    matrix = matrix * glm::toMat4(rotationQuat);
    matrix = glm::scale(matrix, scale);
}

void MeshComponent::generateAABB() {
    aabb[0] = vertices[0].pos;
    aabb[1] = vertices[1].pos;
    for (auto& v : vertices) {
        aabb[0] = glm::min(aabb[0], v.pos);
        aabb[1] = glm::max(aabb[1], v.pos);
    }
}

void MeshComponent::uploadVertices() {
    vertexBuffer.loadVertices(vertices.data(), vertices.size());
    vertexBuffer.setLayout({
        { "POSITION",    ShaderType::FLOAT3 },
        { "UV",          ShaderType::FLOAT2 },
        { "NORMAL",      ShaderType::FLOAT3 },
        { "TANGENT",     ShaderType::FLOAT3 },
        { "BINORMAL",    ShaderType::FLOAT3 },
        });

    glCreateBuffers(1, &boneIndexBuffer);

    glNamedBufferData(boneIndexBuffer, boneIndices.size() * sizeof(glm::ivec4), boneIndices.data(), GL_STATIC_COPY);
    
    glCreateBuffers(1, &boneWeightBuffer);
    glNamedBufferData(boneWeightBuffer, boneWeights.size() * sizeof(glm::vec4), boneWeights.data(), GL_STATIC_COPY);

    glCreateBuffers(1, &boneTransformsBuffer);
    glNamedBufferData(boneTransformsBuffer, boneTransforms.size() * sizeof(glm::mat4), boneTransforms.data(), GL_DYNAMIC_READ);

    skinnedVertexBuffer.loadVertices(vertices.data(), vertices.size());
    skinnedVertexBuffer.setLayout({
        { "POSITION",    ShaderType::FLOAT3 },
        { "UV",          ShaderType::FLOAT2 },
        { "NORMAL",      ShaderType::FLOAT3 },
        { "TANGENT",     ShaderType::FLOAT3 },
        { "BINORMAL",    ShaderType::FLOAT3 },
    });
}

void MeshComponent::uploadIndices() {
    indexBuffer.loadFaces(indices.data(), indices.size());
}

} // ECS
} // raekor