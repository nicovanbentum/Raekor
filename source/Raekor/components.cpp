#include "pch.h"
#include "components.h"
#include "assets.h"
#include "systems.h"
#include "primitives.h"

namespace Raekor
{

SCRIPT_INTERFACE void Transform::compose() {
    localTransform = glm::translate(glm::mat4(1.0f), position);
    localTransform *= glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);
    localTransform = glm::scale(localTransform, scale);
}



SCRIPT_INTERFACE void Transform::decompose() {
    glm::vec3 skew;
    glm::quat quat;
    glm::vec4 perspective;
    glm::decompose(localTransform, scale, quat, position, skew, perspective);
    glm::extractEulerAngleXYZ(localTransform, rotation.x, rotation.y, rotation.z);
}



SCRIPT_INTERFACE void Transform::print() {
    std::cout << glm::to_string(rotation) << '\n';
}



void Mesh::generateTangents() {
    tangents.resize(positions.size());
    for (size_t i = 0; i < indices.size(); i += 3) {
        auto v0 = positions[indices[i]];
        auto v1 = positions[indices[i + 1]];
        auto v2 = positions[indices[i + 2]];

        glm::vec3 normal = glm::cross((v1 - v0), (v2 - v0));

        glm::vec3 deltaPos;
        if (v0 == v1) {
            deltaPos = v2 - v0;
        } else {
            deltaPos = v1 - v0;
        }

        glm::vec2 uv0 = uvs[indices[i]];
        glm::vec2 uv1 = uvs[indices[i + 1]];
        glm::vec2 uv2 = uvs[indices[i + 2]];

        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;

        glm::vec3 tan;

        if (deltaUV1.s != 0) {
            tan = deltaPos / deltaUV1.s;
        } else {
            tan = deltaPos / 1.0f;
        }

        tan = glm::normalize(tan - glm::dot(normal, tan) * normal);

        tangents[indices[i]] = glm::vec4(tan, 1.0f);
        tangents[indices[i + 1]] = glm::vec4(tan, 1.0f);
        tangents[indices[i + 2]] = glm::vec4(tan, 1.0f);
    }
}



void Mesh::generateNormals() {
    normals = decltype(normals)(positions.size(), glm::vec3(0.0f));

    for (auto i = 0; i < indices.size(); i += 3) {
        auto normal = glm::normalize(glm::cross(
            positions[indices[i]] - positions[indices[i + 1]], 
            positions[indices[i + 1]] - positions[indices[i + 2]]
        ));

        normals[indices[i]] += normal;
        normals[indices[i + 1]] += normal;
        normals[indices[i + 2]] += normal;
    }

    for (auto& normal : normals) {
        normal = glm::normalize(normal / 3.0f);
    }
}



void Mesh::generateAABB() {
    aabb[0] = positions[0];
    aabb[1] = positions[1];
    for (const auto& v : positions) {
        aabb[0] = glm::min(aabb[0], v);
        aabb[1] = glm::max(aabb[1], v);
    }
}



std::vector<float> Mesh::getInterleavedVertices() {
    std::vector<float> vertices;
    vertices.reserve(
        3 * positions.size() +
        2 * uvs.size() +
        3 * normals.size() +
        3 * tangents.size()
    );

    const bool hasUVs = !uvs.empty();
    const bool hasNormals = !normals.empty();
    const bool hasTangents = !tangents.empty();

    for (auto i = 0; i < positions.size(); i++) {
        auto position = positions[i];
        vertices.push_back(position.x);
        vertices.push_back(position.y);
        vertices.push_back(position.z);

        if (hasUVs) {
            auto uv = uvs[i];
            vertices.push_back(uv.x);
            vertices.push_back(uv.y);
        }

        if (hasNormals) {
            auto normal = normals[i];
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
        }

        if (hasTangents) {
            auto tangent = tangents[i];
            vertices.push_back(tangent.x);
            vertices.push_back(tangent.y);
            vertices.push_back(tangent.z);
        }
    }
    
    return vertices;
}



void Skeleton::UpdateBoneTransforms(const Animation& animation, float animationTime, Bone& pNode, const glm::mat4& parentTransform) {
    auto globalTransformation = glm::mat4(1.0f);

    bool hasAnimation = animation.boneAnimations.find(pNode.name) != animation.boneAnimations.end();

    if (bonemapping.find(pNode.name) != bonemapping.end() && pNode.name != m_Bones.name && hasAnimation) {

        glm::mat4 nodeTransform = glm::mat4(1.0f);

        const auto& nodeAnim = animation.boneAnimations.at(pNode.name);

        glm::vec3 translation = nodeAnim.getInterpolatedPosition(animationTime);
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(translation.x, translation.y, translation.z));

        glm::quat rotation = nodeAnim.getInterpolatedRotation(animationTime);
        glm::mat4 rotationMatrix = glm::toMat4(rotation);

        glm::vec3 scale = nodeAnim.getInterpolatedScale(animationTime);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));

        nodeTransform = translationMatrix * rotationMatrix * scaleMatrix;

        globalTransformation = parentTransform * nodeTransform;

        unsigned int boneIndex = bonemapping[pNode.name];
        m_BoneTransforms[boneIndex] = globalTransformation * m_BoneOffsets[boneIndex];
    }

    for (auto& child : pNode.children) {
        UpdateBoneTransforms(animation, animationTime, child, globalTransformation);
    }
}



void Skeleton::UpdateFromAnimation(Animation& animation, float dt) {
    /*
        This is bugged, Assimp docs say totalDuration is in ticks, but the actual value is real world time in milliseconds
        see https://github.com/assimp/assimp/issues/2662
    */
    animation.runningTime += dt;
    if (animation.runningTime > animation.totalDuration) {
        animation.runningTime = 0;
    }

    auto identity = glm::mat4(1.0f);
    UpdateBoneTransforms(animation, animation.runningTime, m_Bones, identity);
}


Material Material::Default;


template<>
void clone<Transform>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& component = reg.get<Transform>(from);
    reg.emplace<Transform>(to, component);
}




template<>
void clone<Node>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& fromNode = reg.get<Node>(from);
    auto& toNode = reg.emplace<Node>(to);
    if (fromNode.parent != entt::null) {
        NodeSystem::append(reg, reg.get<Node>(fromNode.parent), toNode);
    }
}



template<>
void clone<Name>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& component = reg.get<Name>(from);
    reg.emplace<Name>(to, component);
}



template<>
void clone<Mesh>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& from_component = reg.get<Mesh>(from);
    auto& to_component = reg.emplace<Mesh>(to, from_component);
}



template<>
void clone<Material>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& from_component = reg.get<Material>(from);
    auto& to_component = reg.emplace<Material>(to, from_component);
}

} // raekor