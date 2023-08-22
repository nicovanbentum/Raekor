#pragma once

#include "rtti.h"
#include "assets.h"
#include "animation.h"

namespace Raekor {

class INativeScript;

struct Name {
    RTTI_CLASS_HEADER(Name);

    std::string name;

    operator const std::string& () { return name; }
    bool operator==(const std::string& rhs) { return name == rhs; }
    Name& operator=(const std::string& rhs) { name = rhs; return *this; }
};


struct SCRIPT_INTERFACE Transform {
    RTTI_CLASS_HEADER(Transform);

    glm::vec3 scale     = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 position  = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::quat rotation  = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));

    glm::mat4 localTransform  = glm::mat4(1.0f);
    glm::mat4 worldTransform  = glm::mat4(1.0f);

    void Print();
    void Compose();
    void Decompose();
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

    bool IsRoot() const { return parent == entt::null; }
    bool HasChildren() const { return firstChild != entt::null; }
    bool IsConnected() const { return firstChild != entt::null && parent != entt::null; }
};

struct Mesh {
    RTTI_CLASS_HEADER(Mesh);
    
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

    entt::entity material = entt::null;

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
    JPH::BodyID bodyID;
    JPH::EMotionType motionType;
    JPH::BoxShapeSettings settings;
    JPH::MeshShapeSettings meshSettings;

    void CreateFromMesh(const Mesh& inMesh);
};


struct SoftBody {
    JPH::BodyID mBodyID;
    JPH::SoftBodySharedSettings mSharedSettings;
    JPH::SoftBodyCreationSettings mCreationSettings;

    void CreateFromMesh(const Mesh& inMesh);
};

struct Bone {
    uint32_t index;
    std::string name;
    std::vector<Bone> children;
};


struct Skeleton {
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
    RTTI_CLASS_HEADER(Material);

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

template<>
void clone<BoxCollider>(entt::registry& reg, entt::entity from, entt::entity to);

} // Raekor

namespace cereal {

template<typename Archive>
void save(Archive& archive, const Raekor::Transform& transform) {
    archive(transform.position, transform.rotation, transform.scale,
        transform.localTransform, transform.worldTransform);
}

template<typename Archive>
void load(Archive& archive, Raekor::Transform& transform) {
    archive(transform.position, transform.rotation, transform.scale,
        transform.localTransform, transform.worldTransform);
}

template<typename Archive>
void save(Archive& archive, const Raekor::DirectionalLight& light) {
    archive(light.colour, light.direction);
}


template<typename Archive>
void load(Archive& archive, Raekor::DirectionalLight& light) {
    archive(light.colour, light.direction);
}


template<typename Archive>
void serialize(Archive& archive, Raekor::PointLight& light) {
    archive(light.colour, light.colour);
}


template<class Archive>
void serialize(Archive& archive, Raekor::Node& node) {
    archive(node.parent, node.firstChild, node.nextSibling, node.prevSibling);
}


template<class Archive>
void save(Archive& archive, const Raekor::Mesh& mesh) {
    archive(
        cereal::make_nvp("positions", mesh.positions),
        cereal::make_nvp("uvs", mesh.uvs),
        cereal::make_nvp("normals", mesh.normals),
        cereal::make_nvp("tangents", mesh.tangents),
        cereal::make_nvp("indices", mesh.indices),
        cereal::make_nvp("material", mesh.material)
    );
}


template<class Archive>
void load(Archive& archive, Raekor::Mesh& mesh) {
    archive(
        cereal::make_nvp("positions", mesh.positions),
        cereal::make_nvp("uvs", mesh.uvs),
        cereal::make_nvp("normals", mesh.normals),
        cereal::make_nvp("tangents", mesh.tangents),
        cereal::make_nvp("indices", mesh.indices),
        cereal::make_nvp("material", mesh.material)
    );
}

template<class Archive>
void serialize(Archive& archive, JPH::Vec3& vec) {
    archive(vec.mF32);
}

template<class Archive>
void serialize(Archive& archive, JPH::BoxShapeSettings& settings) {
    archive(settings.mHalfExtent, settings.mConvexRadius);
}

template<class Archive>
void serialize(Archive& archive, Raekor::BoxCollider& collider) {
    archive(collider.motionType, collider.settings);
}


template<class Archive>
void save(Archive& archive, const Raekor::Material& mat) {
    archive(
        cereal::make_nvp("Albedo Map", mat.albedoFile),
        cereal::make_nvp("Normal Map", mat.normalFile),
        cereal::make_nvp("Metallic-Roughness Map", mat.metalroughFile),
        cereal::make_nvp("Base Color", mat.albedo),
        cereal::make_nvp("Metallic", mat.metallic),
        cereal::make_nvp("Roughness", mat.roughness),
        cereal::make_nvp("emissive", mat.emissive),
        cereal::make_nvp("Transparent", mat.isTransparent)
    );
}


template<class Archive>
void load(Archive& archive, Raekor::Material& mat) {
    archive(
        cereal::make_nvp("Albedo Map", mat.albedoFile),
        cereal::make_nvp("Normal Map", mat.normalFile),
        cereal::make_nvp("Metallic-Roughness Map", mat.metalroughFile),
        cereal::make_nvp("Base Color", mat.albedo),
        cereal::make_nvp("Metallic", mat.metallic),
        cereal::make_nvp("Roughness", mat.roughness),
        cereal::make_nvp("emissive", mat.emissive),
        cereal::make_nvp("Transparent", mat.isTransparent)
    );
}


template<class Archive>
void serialize(Archive& archive, Raekor::Name& name) {
    archive(cereal::make_nvp("name", name.name));
}

} // cereal
