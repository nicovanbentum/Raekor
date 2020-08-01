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
    std::vector<Triangle> indices;

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
    glm::vec4 baseColour = { 1.0, 1.0, 1.0, 1.0 };
    float metallic = 1.0f, roughness = 1.0f;

    // GPU resources
    std::shared_ptr<glTexture2D> albedo;
    std::shared_ptr<glTexture2D> normals;
    std::shared_ptr<glTexture2D> metalrough;

    void uploadRenderData(const std::unordered_map<std::string, Stb::Image>& images);
};

struct NameComponent {
    std::string name;

    inline operator const std::string& () { return name; }
    inline bool operator==(const std::string& rhs) { return name == rhs; }
    inline NameComponent& operator=(const std::string& rhs) { name = rhs; }
};


} // ECS
} // raekor