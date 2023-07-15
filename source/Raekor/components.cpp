#include "pch.h"
#include "components.h"
#include "timer.h"
#include "assets.h"
#include "systems.h"
#include "primitives.h"

namespace Raekor {

RTTI_CLASS_CPP(Name) {
    RTTI_MEMBER_CPP(Name, "Name", name);
}


RTTI_CLASS_CPP(Transform) {
    RTTI_MEMBER_CPP(Transform, "Scale", scale);
    RTTI_MEMBER_CPP(Transform, "Position", position);
    RTTI_MEMBER_CPP(Transform, "Rotation", rotation);
}


SCRIPT_INTERFACE void Transform::Compose() {
    localTransform = glm::translate(glm::mat4(1.0f), position);
    localTransform = localTransform * glm::toMat4(rotation);
    localTransform = glm::scale(localTransform, scale);
}


SCRIPT_INTERFACE void Transform::Decompose() {
    position = localTransform[3];

    for (int i = 0; i < 3; i++)
        scale[i] = glm::length(glm::vec3(localTransform[i]));

    const auto rotation_matrix = glm::mat3(
        glm::vec3(localTransform[0]) / scale[0],
        glm::vec3(localTransform[1]) / scale[1],
        glm::vec3(localTransform[2]) / scale[2]
    );

    rotation = glm::quat_cast(rotation_matrix);
}


SCRIPT_INTERFACE void Transform::Print() {
    std::cout << glm::to_string(rotation) << '\n';
}


void Mesh::CalculateTangents() {
    tangents.resize(positions.size());
    for (size_t i = 0; i < indices.size(); i += 3) {
        auto v0 = positions[indices[i]];
        auto v1 = positions[indices[i + 1]];
        auto v2 = positions[indices[i + 2]];

        glm::vec3 normal = glm::cross((v1 - v0), (v2 - v0));
        glm::vec3 deltaPos = v0 == v1 ? v2 - v0 : v1 - v0;

        glm::vec2 uv0 = uvs[indices[i]];
        glm::vec2 uv1 = uvs[indices[i + 1]];
        glm::vec2 uv2 = uvs[indices[i + 2]];

        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;

        glm::vec3 tan;

        if (deltaUV1.s != 0)
            tan = deltaPos / deltaUV1.s;
        else
            tan = deltaPos / 1.0f;

        tan = glm::normalize(tan - glm::dot(normal, tan) * normal);

        tangents[indices[i]] = glm::vec4(tan, 1.0f);
        tangents[indices[i + 1]] = glm::vec4(tan, 1.0f);
        tangents[indices[i + 2]] = glm::vec4(tan, 1.0f);
    }
}


void Mesh::CalculateNormals() {
    normals.resize(positions.size(), glm::vec3(0.0f));

    for (auto i = 0; i < indices.size(); i += 3) {
        auto normal = glm::normalize(glm::cross(
            positions[indices[i]] - positions[indices[i + 1]], 
            positions[indices[i + 1]] - positions[indices[i + 2]]
        ));

        normals[indices[i]] += normal;
        normals[indices[i + 1]] += normal;
        normals[indices[i + 2]] += normal;
    }

    for (auto& normal : normals)
        normal = glm::normalize(normal / 3.0f);
}


void Mesh::CalculateAABB() {
    if (positions.size() < 2)
        return;

    aabb[0] = positions[0];
    aabb[1] = positions[1];

    for (const auto& v : positions) {
        aabb[0] = glm::min(aabb[0], v);
        aabb[1] = glm::max(aabb[1], v);
    }
}


std::vector<float> Mesh::GetInterleavedVertices() const {
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

        if (hasTangents && i < tangents.size()) {
            auto tangent = tangents[i];
            vertices.push_back(tangent.x);
            vertices.push_back(tangent.y);
            vertices.push_back(tangent.z);
        }
    }
    
    return vertices;
}



uint32_t Mesh::GetInterleavedStride() const {
    auto stride = 0u;
    stride += sizeof(glm::vec3) * !positions.empty();
    stride += sizeof(glm::vec2) * !uvs.empty();
    stride += sizeof(glm::vec3) * !normals.empty();
    stride += sizeof(glm::vec3) * !tangents.empty();
    return stride;
}



void Skeleton::UpdateBoneTransforms(const Animation& animation, float animationTime, Bone& pNode, const glm::mat4& parentTransform) {
    auto global_transform = glm::mat4(1.0f);

    bool has_animation = animation.m_BoneAnimations.find(pNode.index) != animation.m_BoneAnimations.end();

    if (pNode.index != boneHierarchy.index && has_animation) {
        const auto& node_anim = animation.m_BoneAnimations.at(pNode.index);

        auto translation = node_anim.GetInterpolatedPosition(animationTime);
        auto translation_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(translation.x, translation.y, translation.z));

        auto rotation = node_anim.GetInterpolatedRotation(animationTime);
        auto rotation_matrix = glm::toMat4(rotation);

        auto scale = node_anim.GetInterpolatedScale(animationTime);
        auto scale_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));

        auto nodeTransform = translation_matrix * rotation_matrix * scale_matrix;

        global_transform = parentTransform * nodeTransform;
        
        boneTransformMatrices[pNode.index] = global_transform * boneOffsetMatrices[pNode.index];
    }

    for (auto& child : pNode.children)
        UpdateBoneTransforms(animation, animationTime, child, global_transform);
}


void Skeleton::UpdateFromAnimation(Animation& animation, float dt) {
    /*
        This is bugged, Assimp docs say totalDuration is in ticks, but the actual value is real world time in milliseconds
        see https://github.com/assimp/assimp/issues/2662
    */
    animation.m_RunningTime += Timer::sToMilliseconds(dt);

    if (animation.m_RunningTime > animation.m_TotalDuration)
        animation.m_RunningTime = 0;

    auto identity = glm::mat4(1.0f);
    UpdateBoneTransforms(animation, animation.m_RunningTime, boneHierarchy, identity);
}

RTTI_CLASS_CPP(Material) {
    RTTI_MEMBER_CPP(Material, "Base Color",             albedo);
    RTTI_MEMBER_CPP(Material, "Base Emissive",          emissive);
    RTTI_MEMBER_CPP(Material, "Metallic",               metallic);
    RTTI_MEMBER_CPP(Material, "Roughness",              roughness);
    RTTI_MEMBER_CPP(Material, "Alpha Cutout",           isTransparent);
    RTTI_MEMBER_CPP(Material, "Albedo Map",             albedoFile);
    RTTI_MEMBER_CPP(Material, "Normal Map",             normalFile);
    RTTI_MEMBER_CPP(Material, "Metallic-Roughness Map", metalroughFile);
}

Material Material::Default;


void gRegisterComponentTypes() {
    RTTIFactory::Register(RTTI_OF(Name));
    RTTIFactory::Register(RTTI_OF(Transform));
    RTTIFactory::Register(RTTI_OF(Material));
}


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
        NodeSystem::sAppend(reg, fromNode.parent, reg.get<Node>(fromNode.parent), to, toNode);
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


template<>
void clone<BoxCollider>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& from_component = reg.get<BoxCollider>(from);
    auto& to_component = reg.emplace<BoxCollider>(to, from_component);

    // Invalidate the copied bodyID so it gets registered next update
    to_component.bodyID = JPH::BodyID(); 
}

} // raekor