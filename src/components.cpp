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

const aiNodeAnim* MeshAnimationComponent::findNodeAnim(const aiAnimation* animation, const char* nodeName) {
    for (unsigned int i = 0; i < animation->mNumChannels; i++) {

        aiNodeAnim* nodeAnim = animation->mChannels[i];

        if (strcmp(nodeAnim->mNodeName.C_Str(), nodeName) == 0) {
            return nodeAnim;
        }
    }

    return nullptr;
}

unsigned int MeshAnimationComponent::FindPosition(float AnimationTime, const BoneAnimation* pNodeAnim) {
    for (unsigned int i = 0; i < pNodeAnim->positionKeys.size() - 1; i++) {
        if (AnimationTime < (float)pNodeAnim->positionKeys[i + 1].mTime) {
            return i;
        }
    }
    return 0;
}

unsigned int MeshAnimationComponent::FindRotation(float AnimationTime, const BoneAnimation* pNodeAnim) {
    for (unsigned int i = 0; i < pNodeAnim->rotationkeys.size() - 1; i++) {
        if (AnimationTime < (float)pNodeAnim->rotationkeys[i + 1].mTime) {
            return i;
        }
    }
    return 0;
}

unsigned int MeshAnimationComponent::FindScaling(float AnimationTime, const BoneAnimation* pNodeAnim) {
    for (unsigned int i = 0; i < pNodeAnim->scaleKeys.size() - 1; i++) {
        if (AnimationTime < (float)pNodeAnim->scaleKeys[i + 1].mTime) {
            return i;
        }
    }
    return 0;
}

glm::vec3 MeshAnimationComponent::InterpolateTranslation(float animationTime, const BoneAnimation* nodeAnim) {
    if (nodeAnim->positionKeys.size() == 1) {
        // No interpolation necessary for single value
        auto v = nodeAnim->positionKeys[0].mValue;
        return { v.x, v.y, v.z };
    }

    uint32_t PositionIndex = FindPosition(animationTime, nodeAnim);
    uint32_t NextPositionIndex = (PositionIndex + 1);

    float DeltaTime = (float)(nodeAnim->positionKeys[NextPositionIndex].mTime - nodeAnim->positionKeys[PositionIndex].mTime);
    float Factor = (animationTime - (float)nodeAnim->positionKeys[PositionIndex].mTime) / DeltaTime;
    if (Factor < 0.0f) Factor = 0.0f;

    const aiVector3D& Start = nodeAnim->positionKeys[PositionIndex].mValue;
    const aiVector3D& End = nodeAnim->positionKeys[NextPositionIndex].mValue;
    aiVector3D Delta = End - Start;
    auto aiVec = Start + Factor * Delta;
    return { aiVec.x, aiVec.y, aiVec.z };
}

glm::quat MeshAnimationComponent::InterpolateRotation(float animationTime, const BoneAnimation* nodeAnim) {
    if (nodeAnim->rotationkeys.size() == 1) {
        // No interpolation necessary for single value
        auto v = nodeAnim->rotationkeys[0].mValue;
        return glm::quat(v.w, v.x, v.y, v.z);
    }

    uint32_t RotationIndex = FindRotation(animationTime, nodeAnim);
    uint32_t NextRotationIndex = (RotationIndex + 1);

    float DeltaTime = (float)(nodeAnim->rotationkeys[NextRotationIndex].mTime - nodeAnim->rotationkeys[RotationIndex].mTime);
    float Factor = (animationTime - (float)nodeAnim->rotationkeys[RotationIndex].mTime) / DeltaTime;
    if (Factor < 0.0f) Factor = 0.0f;

    const aiQuaternion& StartRotationQ = nodeAnim->rotationkeys[RotationIndex].mValue;
    const aiQuaternion& EndRotationQ = nodeAnim->rotationkeys[NextRotationIndex].mValue;
    auto q = aiQuaternion();
    aiQuaternion::Interpolate(q, StartRotationQ, EndRotationQ, Factor);
    q = q.Normalize();
    return glm::quat(q.w, q.x, q.y, q.z);
}

glm::vec3 MeshAnimationComponent::InterpolateScale(float animationTime, const BoneAnimation* nodeAnim) {
    if (nodeAnim->scaleKeys.size() == 1) {
        // No interpolation necessary for single value
        auto v = nodeAnim->scaleKeys[0].mValue;
        return { v.x, v.y, v.z };
    }

    uint32_t index = FindScaling(animationTime, nodeAnim);
    uint32_t nextIndex = (index + 1);

    float deltaTime = (float)(nodeAnim->scaleKeys[nextIndex].mTime - nodeAnim->scaleKeys[index].mTime);
    float factor = (animationTime - (float)nodeAnim->scaleKeys[index].mTime) / deltaTime;
    if (factor < 0.0f) factor = 0.0f;

    const auto& start = nodeAnim->scaleKeys[index].mValue;
    const auto& end = nodeAnim->scaleKeys[nextIndex].mValue;
    auto delta = end - start;
    auto aiVec = start + factor * delta;
    return { aiVec.x, aiVec.y, aiVec.z };
}

glm::mat4 MeshAnimationComponent::aiMat4toGLM(const aiMatrix4x4& from) {
    glm::mat4 to;
    //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}

void MeshAnimationComponent::ReadNodeHierarchy(float animationTime, const BoneTreeNode* pNode, const glm::mat4& parentTransform) {
    auto globalTransformation = glm::mat4(1.0f);

    bool hasAnimation = animation.boneAnimations.find(pNode->name) != animation.boneAnimations.end();

    if (bonemapping.find(pNode->name) != bonemapping.end() && pNode != boneTreeRootNode && hasAnimation) {

        glm::mat4 nodeTransform = glm::mat4(1.0f);

        auto& nodeAnim = animation.boneAnimations[pNode->name.c_str()];

        glm::vec3 translation = InterpolateTranslation(animationTime, &nodeAnim);
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(translation.x, translation.y, translation.z));

        glm::quat rotation = InterpolateRotation(animationTime, &nodeAnim);
        glm::mat4 rotationMatrix = glm::toMat4(rotation);

        glm::vec3 scale = InterpolateScale(animationTime, &nodeAnim);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));

        nodeTransform = translationMatrix * rotationMatrix * scaleMatrix;

        globalTransformation = parentTransform * nodeTransform;
        
        unsigned int boneIndex = bonemapping[pNode->name];
        boneInfos[boneIndex].finalTransformation = globalTransformation * boneInfos[boneIndex].boneOffset;
    }

    for (auto& child : pNode->children) {
        ReadNodeHierarchy(animationTime, &child, globalTransformation);
    }
}

void MeshAnimationComponent::boneTransform(float TimeInSeconds) {
    float TicksPerSecond = animation.ticksPerSecond != 0.0 ? animation.ticksPerSecond : 25.0f;
    float TimeInTicks = TimeInSeconds * TicksPerSecond;
    float AnimationTime = fmod(TimeInTicks, animation.TotalDuration);

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