#pragma once

#include "buffer.h"
#include "anim.h"
#include "script.h"
#include "assets.h"

namespace Raekor {
namespace ecs {

struct NameComponent {
    std::string name;

    inline operator const std::string& () { return name; }
    inline bool operator==(const std::string& rhs) { return name == rhs; }
    inline NameComponent& operator=(const std::string& rhs) { name = rhs; return *this; }
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct TransformComponent {
    glm::vec3 scale     = { 1.0f, 1.0f, 1.0f };
    glm::vec3 position  = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation  = { 0.0f, 0.0f, 0.0f };

    glm::mat4 localTransform  = glm::mat4(1.0f);
    glm::mat4 worldTransform  = glm::mat4(1.0f);

    bool dirty = true;

    void compose();
    void decompose();
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct DirectionalLightComponent {
    struct ShaderBuffer {
        glm::vec4 direction = { 0.0f, 0.0f, 0.0f, 0.0f };
        glm::vec4 colour = { 1.0f, 1.0f, 1.0f, 1.0f };
    } buffer;

    float brightness;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct PointLightComponent {
    struct ShaderBuffer {
        glm::vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f };
        glm::vec4 colour = { 1.0f, 1.0f, 1.0f, 1.0f };
    } buffer;

    float brightness;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct NodeComponent {
    entt::entity parent = entt::null;
    entt::entity firstChild = entt::null;
    entt::entity prevSibling = entt::null;
    entt::entity nextSibling = entt::null;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct MeshComponent {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;

    std::vector<uint32_t> indices;

    glVertexBuffer vertexBuffer;
    glIndexBuffer indexBuffer;

    std::array<glm::vec3, 2> aabb;

    entt::entity material = entt::null;

    void generateTangents();
    void generateAABB();
    void uploadIndices();
    void uploadVertices();
    std::vector<float> getVertexData();
    void destroy();
};

//////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: move these next two structs somewhere else
struct BoneInfo {
    glm::mat4 boneOffset;
    glm::mat4 finalTransformation;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct BoneTreeNode {
    std::string name;
    std::vector<BoneTreeNode> children;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct MeshAnimationComponent {
    std::vector<glm::vec4> boneWeights;
    std::vector<glm::ivec4> boneIndices;

    int boneCount = 0;
    std::vector<BoneInfo> boneInfos;
    glm::mat4 inverseGlobalTransform;
    std::vector<glm::mat4> boneTransforms;
    std::unordered_map<std::string, uint32_t> bonemapping;

    Animation animation;
    BoneTreeNode boneTreeRootNode;

    void ReadNodeHierarchy(float animationTime, BoneTreeNode& pNode, const glm::mat4& parentTransform);

    void boneTransform(float TimeInSeconds);

    void uploadRenderData(ecs::MeshComponent& mesh);

    void destroy();

    unsigned int boneIndexBuffer;
    unsigned int boneWeightBuffer;
    unsigned int boneTransformsBuffer;
    glVertexBuffer skinnedVertexBuffer;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct MaterialComponent {
    glm::vec4 baseColour = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallic = 1.0f, roughness = 1.0f;
    std::string albedoFile, normalFile, mrFile;

    // GPU resources
    unsigned int albedo = 0;
    unsigned int normals = 0;
    unsigned int metalrough = 0;

    void createAlbedoTexture();
    void createAlbedoTexture(std::shared_ptr<TextureAsset> texture);

    void createNormalTexture();
    void createNormalTexture(std::shared_ptr<TextureAsset> texture);

    void createMetalRoughTexture();
    void createMetalRoughTexture(std::shared_ptr<TextureAsset> texture);

    void destroy();

    static MaterialComponent Default;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct NativeScriptComponent {
    HMODULE hmodule;
    NativeScript* script;
    std::string procAddress;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

template<typename Component>
struct ComponentDescription {
    const char* name;
    using type = Component;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

static constexpr auto Components = std::make_tuple (
    ComponentDescription<NameComponent>{"Name"},
    ComponentDescription<NodeComponent>{"Node"},
    ComponentDescription<MeshComponent>{"Mesh"},
    ComponentDescription<MaterialComponent>{"Material"},
    ComponentDescription<TransformComponent>{"Transform"},
    ComponentDescription<PointLightComponent>{"Point Light"},
    ComponentDescription<MeshAnimationComponent>{"Mesh Animation"},
    ComponentDescription<DirectionalLightComponent>{"Directional Light"},
    ComponentDescription<NativeScriptComponent>{"Native Script"}
);

//////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
inline void clone(entt::registry& reg, entt::entity from, entt::entity to) {}

template<>
void clone<TransformComponent>(entt::registry& reg, entt::entity from, entt::entity to);

template<>
void clone<NodeComponent>(entt::registry& reg, entt::entity from, entt::entity to);

template<>
void clone<NameComponent>(entt::registry& reg, entt::entity from, entt::entity to);

template<>
void clone<MeshComponent>(entt::registry& reg, entt::entity from, entt::entity to);

template<>
void clone<MaterialComponent>(entt::registry& reg, entt::entity from, entt::entity to);

} // ECS
} // raekor