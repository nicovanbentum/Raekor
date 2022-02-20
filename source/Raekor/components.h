#pragma once

#include "anim.h"
#include "script.h"
#include "assets.h"

namespace Raekor {

struct Name {
    std::string name;

    inline operator const std::string& () { return name; }
    inline bool operator==(const std::string& rhs) { return name == rhs; }
    inline Name& operator=(const std::string& rhs) { name = rhs; return *this; }
};



struct Transform {
    glm::vec3 scale     = { 1.0f, 1.0f, 1.0f };
    glm::vec3 position  = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation  = { 0.0f, 0.0f, 0.0f };

    glm::mat4 localTransform  = glm::mat4(1.0f);
    glm::mat4 worldTransform  = glm::mat4(1.0f);

    SCRIPT_INTERFACE void compose();
    SCRIPT_INTERFACE void decompose();
    SCRIPT_INTERFACE void print();
};



struct DirectionalLight {
    glm::vec4 direction = { 0.0f, -0.9f, 0.0f, 0.0f };
    glm::vec4 colour    = { 1.0f, 1.0f, 1.0f, 1.0f };
};



struct PointLight {
    glm::vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f };
    glm::vec4 colour   = { 1.0f, 1.0f, 1.0f, 1.0f };
};



struct Node {
    entt::entity parent      = entt::null;
    entt::entity firstChild  = entt::null;
    entt::entity prevSibling = entt::null;
    entt::entity nextSibling = entt::null;
};



struct Mesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    
    std::vector<uint32_t> indices;

    uint32_t vertexBuffer;
    uint32_t indexBuffer;

    std::array<glm::vec3, 2> aabb;

    entt::entity material = entt::null;

    void generateTangents();
    void generateNormals();
    void generateAABB();
    std::vector<float> getInterleavedVertices();
};


struct Bone {
    std::string name;
    glm::mat4 offset;
    std::vector<Bone> children;
};


struct Skeleton {
    glm::mat4 inverseGlobalTransform;
    std::vector<glm::vec4> m_BoneWeights;
    std::vector<glm::mat4> m_BoneOffsets;
    std::vector<glm::ivec4> m_BoneIndices;
    std::vector<glm::mat4> m_BoneTransforms;
    std::unordered_map<std::string, uint32_t> bonemapping;

    Bone m_Bones;
    std::vector<Animation> animations;

    void UpdateFromAnimation(Animation& animation, float TimeInSeconds);
    void UpdateBoneTransforms(const Animation& animation, float animationTime, Bone& pNode, const glm::mat4& parentTransform);

    // Skinning GPU buffers
    uint32_t boneIndexBuffer;
    uint32_t boneWeightBuffer;
    uint32_t boneTransformsBuffer;
    uint32_t skinnedVertexBuffer;
};



struct Material {
    // properties
    glm::vec4 albedo = glm::vec4(1.0f);
    glm::vec3 emissive = glm::vec3(0.0f);
    float metallic = 0.0f, roughness = 1.0f;

    // texture file paths
    std::string albedoFile, normalFile, metalroughFile;

    // GPU resources
    uint32_t gpuAlbedoMap = 0;
    uint32_t gpuNormalMap = 0;
    uint32_t gpuMetallicRoughnessMap = 0;

    // default material for newly spawned meshes
    static Material Default;
};



struct NativeScript {
    std::string file;
    ScriptAsset::Ptr asset;
    std::string procAddress;
    INativeScript* script = nullptr;
};



template<typename Component>
struct ComponentDescription {
    const char* name;
    using type = Component;
};



static constexpr auto Components = std::make_tuple (
    ComponentDescription<Name>{"Name"},
    ComponentDescription<Node>{"Node"},
    ComponentDescription<Mesh>{"Mesh"},
    ComponentDescription<Material>{"Material"},
    ComponentDescription<Transform>{"Transform"},
    ComponentDescription<PointLight>{"Point Light"},
    ComponentDescription<Skeleton>{"Skeleton"},
    ComponentDescription<DirectionalLight>{"Directional Light"},
    ComponentDescription<NativeScript>{"Native Script"}
);

template<typename T>
inline void clone(entt::registry& reg, entt::entity from, entt::entity to) {}

template<>
void clone<Transform>(entt::registry& reg, entt::entity from, entt::entity to);

template<>
void clone<Node>(entt::registry& reg, entt::entity from, entt::entity to);

template<>
void clone<Name>(entt::registry& reg, entt::entity from, entt::entity to);

template<>
void clone<Mesh>(entt::registry& reg, entt::entity from, entt::entity to);

template<>
void clone<Material>(entt::registry& reg, entt::entity from, entt::entity to);

} // raekor