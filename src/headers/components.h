#pragma once

#include "ecs.h"

namespace Raekor {
namespace ECS {

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


};

struct PointLightComponent {
    struct ShaderBuffer {
        glm::vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f };
        glm::vec4 colour = { 1.0f, 1.0f, 1.0f, 1.0f };
    } buffer;


};

struct NodeComponent {
    ECS::Entity parent = NULL;
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

    void generateAABB();
    
    void uploadVertices();
    void uploadIndices();
};

struct BoneTreeNode {
    std::string name;
    std::vector<BoneTreeNode> children;
};

struct BoneAnimation {
    std::vector<aiVectorKey> positionKeys;
    std::vector<aiQuatKey> rotationkeys;
    std::vector<aiVectorKey> scaleKeys;
};

struct Animation {
    std::string name;
    float ticksPerSecond;
    float TotalDuration;
    std::unordered_map<std::string, BoneAnimation> boneAnimations;
};

struct MeshAnimationComponent {
    MeshAnimationComponent() {}
    ~MeshAnimationComponent() {
        auto buffersToDelete = { boneIndexBuffer, boneWeightBuffer, boneTransformsBuffer };
        glDeleteBuffers(3, buffersToDelete.begin());
    }

    std::vector<glm::vec4> boneWeights;
    std::vector<glm::ivec4> boneIndices;

    int boneCount = 0;
    std::vector<BoneInfo> boneInfos;
    glm::mat4 inverseGlobalTransform;
    std::vector<glm::mat4> boneTransforms;
    std::unordered_map<std::string, uint32_t> bonemapping;

    Animation animation;
    BoneTreeNode* boneTreeRootNode;

    const aiNodeAnim* findNodeAnim(const aiAnimation* animation, const char* nodeName);

    unsigned int FindPosition(float AnimationTime, const BoneAnimation* pNodeAnim);
    unsigned int FindRotation(float AnimationTime, const BoneAnimation* pNodeAnim);
    unsigned int FindScaling(float AnimationTime, const BoneAnimation* pNodeAnim);

    glm::vec3 InterpolateTranslation(float animationTime, const BoneAnimation* nodeAnim);
    glm::quat InterpolateRotation(float animationTime, const BoneAnimation* nodeAnim);
    glm::vec3 InterpolateScale(float animationTime, const BoneAnimation* nodeAnim);

    static glm::mat4 aiMat4toGLM(const aiMatrix4x4& from);;

    void ReadNodeHierarchy(float animationTime, const BoneTreeNode* pNode, const glm::mat4& parentTransform);

    void boneTransform(float TimeInSeconds);

    void uploadRenderData(ECS::MeshComponent& mesh);

    unsigned int boneIndexBuffer;
    unsigned int boneWeightBuffer;
    unsigned int boneTransformsBuffer;
    glVertexBuffer skinnedVertexBuffer;
};

struct MaterialComponent {
    std::unique_ptr<glTexture2D> albedo;
    std::unique_ptr<glTexture2D> normals;
    std::unique_ptr<glTexture2D> metalrough;
};

struct NameComponent {
    std::string name;
};

} // ECS
} // raekor