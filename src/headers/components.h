#pragma once

#include "ecs.h"
#include "anim.h"

namespace Raekor {
namespace ecs {

struct TransformComponent {
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };

    glm::mat4 matrix = glm::mat4(1.0f);
    glm::mat4 worldTransform = glm::mat4(1.0f);

    glm::vec3 localPosition = { 0.0f, 0.0f, 0.0f };

    void recalculateMatrix();
};

struct DirectionalLightComponent {
    struct ShaderBuffer {
        glm::vec4 direction = { 0.0f, 0.0f, 0.0f, 0.0f };
        glm::vec4 colour = { 1.0f, 1.0f, 1.0f, 1.0f };
    } buffer;

    float brightness;
};

struct PointLightComponent {
    struct ShaderBuffer {
        glm::vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f };
        glm::vec4 colour = { 1.0f, 1.0f, 1.0f, 1.0f };
    } buffer;

    float brightness;
};

struct NodeComponent {
    entt::entity parent = entt::null;
    bool hasChildren = false;
};

struct BoneInfo {
    glm::mat4 boneOffset;
    glm::mat4 finalTransformation;
};

struct MeshComponent {

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glVertexBuffer vertexBuffer;
    glIndexBuffer indexBuffer;

    std::array<glm::vec3, 2> aabb;

    entt::entity material = entt::null;

    void generateAABB();
    
    void uploadVertices();
    void uploadIndices();
};

struct BoneTreeNode {
    std::string name;
    std::vector<BoneTreeNode> children;
};

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

    unsigned int boneIndexBuffer;
    unsigned int boneWeightBuffer;
    unsigned int boneTransformsBuffer;
    glVertexBuffer skinnedVertexBuffer;
};

struct MaterialComponent {
    std::string name;
    std::string albedoFile, normalFile, mrFile;
    glm::vec4 baseColour = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallic = 1.0f, roughness = 1.0f;

    // GPU resources
    std::shared_ptr<glTexture2D> albedo;
    std::shared_ptr<glTexture2D> normals;
    std::shared_ptr<glTexture2D> metalrough;

    void uploadRenderData() {
        albedo = std::make_shared<glTexture2D>();
        albedo->bind();
        albedo->init(1, 1, { GL_SRGB_ALPHA, GL_RGBA, GL_FLOAT }, glm::value_ptr(baseColour));
        albedo->setFilter(Sampling::Filter::None);
        albedo->setWrap(Sampling::Wrap::Repeat);

        normals = std::make_shared<glTexture2D>();
        normals->bind();
        constexpr auto tbnAxis = glm::vec<4, float>(0.5f, 0.5f, 1.0f, 1.0f);
        normals->init(1, 1, { GL_RGBA16F, GL_RGBA, GL_FLOAT }, glm::value_ptr(tbnAxis));
        normals->setFilter(Sampling::Filter::None);
        normals->setWrap(Sampling::Wrap::Repeat);

        metalrough = std::make_shared<glTexture2D>();
        auto metalRoughnessValue = glm::vec4(metallic, roughness, 0.0f, 1.0f);
        metalrough->bind();
        metalrough->init(1, 1, { GL_RGBA16F, GL_RGBA, GL_FLOAT }, glm::value_ptr(metalRoughnessValue));
        metalrough->setFilter(Sampling::Filter::None);
        metalrough->setWrap(Sampling::Wrap::Repeat);
    }
    void uploadRenderData(const std::unordered_map<std::string, Stb::Image>& images);
};

struct NameComponent {
    std::string name;

    inline operator const std::string& () { return name; }
    inline bool operator==(const std::string& rhs) { return name == rhs; }
    inline NameComponent& operator=(const std::string& rhs) { name = rhs; return *this; }
};

template<typename T>
inline void clone(entt::registry& reg, entt::entity from, entt::entity to) {}

class cloner {
private:
    using clone_fn_type = void(entt::registry& reg, entt::entity from, entt::entity to);
    
public:
    cloner();
    static cloner* getSingleton();
    clone_fn_type* getFunction(entt::id_type id_type);

private:
    std::unordered_map<entt::id_type, clone_fn_type*> clone_functions;
};

} // ECS
} // raekor