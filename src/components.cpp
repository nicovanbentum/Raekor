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
}

void MeshComponent::uploadIndices() {
    indexBuffer.loadFaces(indices.data(), indices.size());
}

void MeshAnimationComponent::ReadNodeHierarchy(float animationTime, BoneTreeNode& pNode, const glm::mat4& parentTransform) {
    auto globalTransformation = glm::mat4(1.0f);

    bool hasAnimation = animation.boneAnimations.find(pNode.name) != animation.boneAnimations.end();

    if (bonemapping.find(pNode.name) != bonemapping.end() && pNode.name != boneTreeRootNode.name && hasAnimation) {

        glm::mat4 nodeTransform = glm::mat4(1.0f);

        auto& nodeAnim = animation.boneAnimations[pNode.name.c_str()];

        glm::vec3 translation = nodeAnim.getInterpolatedPosition(animationTime);
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(translation.x, translation.y, translation.z));

        glm::quat rotation = nodeAnim.getInterpolatedRotation(animationTime);
        glm::mat4 rotationMatrix = glm::toMat4(rotation);

        glm::vec3 scale = nodeAnim.getInterpolatedScale(animationTime);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));

        nodeTransform = translationMatrix * rotationMatrix * scaleMatrix;

        globalTransformation = parentTransform * nodeTransform;
        
        unsigned int boneIndex = bonemapping[pNode.name];
        boneInfos[boneIndex].finalTransformation = globalTransformation * boneInfos[boneIndex].boneOffset;
    }

    for (auto& child : pNode.children) {
        ReadNodeHierarchy(animationTime, child, globalTransformation);
    }
}

void MeshAnimationComponent::boneTransform(float TimeInSeconds) {
    float TicksPerSecond = animation.ticksPerSecond != 0.0 ? animation.ticksPerSecond : 25.0f;
    float TimeInTicks = TimeInSeconds * TicksPerSecond;
    float AnimationTime = fmod(TimeInTicks, animation.totalDuration);

    auto identity = glm::mat4(1.0f);
    ReadNodeHierarchy(AnimationTime, boneTreeRootNode, identity);

    boneTransforms.resize(boneCount);
    for (int i = 0; i < boneCount; i++) {
        boneTransforms[i] = boneInfos[i].finalTransformation;
    }
}

void MeshAnimationComponent::uploadRenderData(ECS::MeshComponent& mesh) {
    glCreateBuffers(1, &boneIndexBuffer);
    glNamedBufferData(boneIndexBuffer, boneIndices.size() * sizeof(glm::ivec4), boneIndices.data(), GL_STATIC_COPY);

    glCreateBuffers(1, &boneWeightBuffer);
    glNamedBufferData(boneWeightBuffer, boneWeights.size() * sizeof(glm::vec4), boneWeights.data(), GL_STATIC_COPY);

    glCreateBuffers(1, &boneTransformsBuffer);
    glNamedBufferData(boneTransformsBuffer, boneTransforms.size() * sizeof(glm::mat4), boneTransforms.data(), GL_DYNAMIC_READ);

    skinnedVertexBuffer.loadVertices(mesh.vertices.data(), mesh.vertices.size());
    skinnedVertexBuffer.setLayout({
        { "POSITION",    ShaderType::FLOAT3 },
        { "UV",          ShaderType::FLOAT2 },
        { "NORMAL",      ShaderType::FLOAT3 },
        { "TANGENT",     ShaderType::FLOAT3 },
        { "BINORMAL",    ShaderType::FLOAT3 },
        });
}

} // ECS
} // raekor