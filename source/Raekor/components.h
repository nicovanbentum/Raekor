#pragma once

#include "ecs.h"
#include "rtti.h"
#include "rmath.h"
#include "assets.h"
#include "animation.h"

namespace Raekor {

class INativeScript;

struct Name {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(Name);

    std::string name;

    operator const std::string& () { return name; }
    bool operator==(const std::string& rhs) { return name == rhs; }
    Name& operator=(const std::string& rhs) { name = rhs; return *this; }
};


struct SCRIPT_INTERFACE Transform {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(Transform);

    glm::vec3 scale     = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 position  = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::quat rotation  = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));

    glm::mat4 localTransform  = glm::mat4(1.0f);
    glm::mat4 worldTransform  = glm::mat4(1.0f);

    Vec3 GetScaleWorldSpace() const;
    Vec3 GetPositionWorldSpace() const;
    Quat GetRotationWorldSpace() const;

    void Print();
    void Compose();
    void Decompose();
};


struct DirectionalLight {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(DirectionalLight);

    const Vec4& GetColor() const { return colour; }
    const Vec4& GetDirection() const { return direction; }


    glm::vec4 direction = { 0.25f, -0.9f, 0.0f, 0.0f };
    glm::vec4 colour    = { 1.0f, 1.0f, 1.0f, 1.0f };
};


struct PointLight {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(PointLight);

    glm::vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f };
    glm::vec4 colour   = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct Light {

    glm::vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f };
    glm::vec4 colour = { 1.0f, 1.0f, 1.0f, 1.0f };
};


struct Node {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(Node);

    Entity parent      = NULL_ENTITY;
    Entity firstChild  = NULL_ENTITY;
    Entity prevSibling = NULL_ENTITY;
    Entity nextSibling = NULL_ENTITY;

    bool IsRoot() const { return parent == NULL_ENTITY; }
    bool HasChildren() const { return firstChild != NULL_ENTITY; }
    bool IsConnected() const { return firstChild != NULL_ENTITY && parent != NULL_ENTITY; }
};

struct Mesh {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(Mesh);
    
    std::vector<glm::vec3> positions; // ptr
    std::vector<glm::vec2> uvs; // ptr
    std::vector<glm::vec3> normals; // ptr
    std::vector<glm::vec3> tangents; // ptr
    
    std::vector<uint32_t> indices; // ptr

    // GPU resources
    uint32_t vertexBuffer   = 0;
    uint32_t indexBuffer    = 0;
    uint32_t BottomLevelAS  = 0;

    std::array<glm::vec3, 2> aabb;

    Entity material = NULL_ENTITY;

    float mLODFade = 0.0f;

    void CalculateTangents();
    void CalculateNormals();
    void CalculateAABB();
    std::vector<float> GetInterleavedVertices() const;
    uint32_t GetInterleavedStride() const;

    void Clear() {
        positions.clear();
        uvs.clear();
        normals.clear();
        tangents.clear();
        indices.clear();

    }
};

struct BoxCollider {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(BoxCollider);

    JPH::BodyID bodyID;
    JPH::EMotionType motionType;
    JPH::BoxShapeSettings settings;
    JPH::MeshShapeSettings meshSettings;

    void CreateFromMesh(const Mesh& inMesh);
};


struct SoftBody {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(SoftBody);

    JPH::BodyID mBodyID;
    JPH::SoftBodySharedSettings mSharedSettings;
    JPH::SoftBodyCreationSettings mCreationSettings;

    void CreateFromMesh(const Mesh& inMesh);
};

struct Bone {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(Bone);

    uint32_t index;
    std::string name;
    std::vector<Bone> children;
};


struct Skeleton {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(Skeleton);

    glm::mat4 inverseGlobalTransform;
    std::vector<glm::vec4> boneWeights; // ptr
    std::vector<glm::ivec4> boneIndices; // ptr
    std::vector<glm::mat4> boneOffsetMatrices; // ptr
    std::vector<glm::mat4> boneTransformMatrices; // ptr

    Bone boneHierarchy;
    std::vector<Animation> animations; // ptr

    void UpdateFromAnimation(Animation& animation, float TimeInSeconds);
    void UpdateBoneTransforms(const Animation& animation, float animationTime, Bone& pNode, const glm::mat4& parentTransform);

    // Skinning GPU buffers
    uint32_t boneIndexBuffer;
    uint32_t boneWeightBuffer;
    uint32_t boneTransformsBuffer;
    uint32_t skinnedVertexBuffer;
};


struct Material {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(Material);

    // properties
    glm::vec4 albedo = glm::vec4(1.0f);
    glm::vec3 emissive = glm::vec3(0.0f);
    float metallic = 0.0f;
    float roughness = 1.0f;
    bool isTransparent = false;

    // texture file paths
    std::string albedoFile; // ptr
    std::string normalFile;  // ptr
    std::string metalroughFile; // ptr

    // GPU resources
    uint32_t gpuAlbedoMap = 0;
    uint32_t gpuNormalMap = 0;
    uint32_t gpuMetallicRoughnessMap = 0;

    bool IsLoaded() const { return gpuAlbedoMap != 0 && gpuNormalMap != 0 && gpuMetallicRoughnessMap != 0; }

    // default material for newly spawned meshes
    static Material Default;
};


struct NativeScript {
    RTTI_DECLARE_TYPE_NO_VIRTUAL(NativeScript);

    std::string file; // ptr
    ScriptAsset::Ptr asset;
    std::string procAddress; // ptr
    INativeScript* script = nullptr;
};


template<typename Component>
struct ComponentDescription {
    const char* name;
    using type = Component;
};


static constexpr auto Components = std::make_tuple (
    ComponentDescription<Name>              {"Name"},
    ComponentDescription<Node>              {"Node"},
    ComponentDescription<Transform>         {"Transform"},
    ComponentDescription<Mesh>              {"Mesh"},
    ComponentDescription<SoftBody>          {"Soft Body"},
    ComponentDescription<Material>          {"Material"},
    ComponentDescription<PointLight>        {"Point Light"},
    ComponentDescription<DirectionalLight>  {"Directional Light"},
    ComponentDescription<BoxCollider>       {"Box Collider"},
    ComponentDescription<Skeleton>          {"Skeleton"},
    ComponentDescription<NativeScript>      {"Native Script"}
);

void gRegisterComponentTypes();

template<typename T>
inline void clone(ecs::ECS& reg, Entity from, Entity to) {}

template<>
void clone<Transform>(ecs::ECS& reg, Entity from, Entity to);

template<>
void clone<Node>(ecs::ECS& reg, Entity from, Entity to);

template<>
void clone<Name>(ecs::ECS& reg, Entity from, Entity to);

template<>
void clone<Mesh>(ecs::ECS& reg, Entity from, Entity to);

template<>
void clone<Material>(ecs::ECS& reg, Entity from, Entity to);

template<>
void clone<BoxCollider>(ecs::ECS& reg, Entity from, Entity to);

} // Raekor
