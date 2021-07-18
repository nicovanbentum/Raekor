#include "pch.h"
#include "components.h"
#include "assets.h"
#include "systems.h"

namespace Raekor
{
namespace ecs
{

void TransformComponent::compose() {
    localTransform = glm::translate(glm::mat4(1.0f), position);
    localTransform *= glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);
    localTransform = glm::scale(localTransform, scale);
}

void TransformComponent::decompose() {
    glm::vec3 skew;
    glm::quat quat;
    glm::vec4 perspective;
    glm::decompose(localTransform, scale, quat, position, skew, perspective);
    glm::extractEulerAngleXYZ(localTransform, rotation.x, rotation.y, rotation.z);
}

/////////////////////////////////////////////////////////////////////////////////////////

void MeshComponent::generateTangents() {
    // calculate tangents
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

        tangents[indices[i]] = tan;
        tangents[indices[i + 1]] = tan;
        tangents[indices[i + 2]] = tan;
    }
}

void MeshComponent::generateAABB() {
    aabb[0] = positions[0];
    aabb[1] = positions[1];
    for (auto& v : positions) {
        aabb[0] = glm::min(aabb[0], v);
        aabb[1] = glm::max(aabb[1], v);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<float> MeshComponent::getVertexData() {
    std::vector<float> vertices;
    vertices.reserve(
        3 * positions.size() +
        2 * uvs.size() +
        3 * normals.size() +
        3 * tangents.size() +
        3 * bitangents.size()
    );

    const bool hasUVs = !uvs.empty();
    const bool hasNormals = !normals.empty();
    const bool hasTangents = !tangents.empty();
    const bool hasBitangents = !bitangents.empty();

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

        if (hasBitangents) {
            auto bitangent = bitangents[i];
            vertices.push_back(bitangent.x);
            vertices.push_back(bitangent.y);
            vertices.push_back(bitangent.z);
        }
    }

    return vertices;
}

/////////////////////////////////////////////////////////////////////////////////////////

void MeshComponent::destroy() {
    vertexBuffer.destroy();
    indexBuffer.destroy();
}

/////////////////////////////////////////////////////////////////////////////////////////

void MeshComponent::uploadVertices() {
    auto vertices = getVertexData();

    std::vector<Element> layout;
    if (!positions.empty()) {
        layout.emplace_back("POSITION", ShaderType::FLOAT3);
    }
    if (!uvs.empty()) {
        layout.emplace_back("TEXCOORD", ShaderType::FLOAT2);
    }
    if (!normals.empty()) {
        layout.emplace_back("NORMAL", ShaderType::FLOAT3);
    }
    if (!tangents.empty()) {
        layout.emplace_back("TANGENT", ShaderType::FLOAT3);
    }
    if (!bitangents.empty()) {
        layout.emplace_back("BINORMAL", ShaderType::FLOAT3);
    }

    vertexBuffer.loadVertices(vertices.data(), vertices.size());
    vertexBuffer.setLayout(layout);
}

/////////////////////////////////////////////////////////////////////////////////////////

void MeshComponent::uploadIndices() {
    indexBuffer.loadIndices(indices.data(), indices.size());
}

/////////////////////////////////////////////////////////////////////////////////////////

void AnimationComponent::ReadNodeHierarchy(float animationTime, BoneTreeNode& pNode, const glm::mat4& parentTransform) {
    auto globalTransformation = glm::mat4(1.0f);

    bool hasAnimation = animation.boneAnimations.find(pNode.name) != animation.boneAnimations.end();

    if (bonemapping.find(pNode.name) != bonemapping.end() && pNode.name != boneTreeRootNode.name && hasAnimation) {

        glm::mat4 nodeTransform = glm::mat4(1.0f);

        auto& nodeAnim = animation.boneAnimations[pNode.name.c_str()];

        glm::vec3 translation = nodeAnim.getInterpolatedPosition(animationTime);
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(translation.x, translation.y, translation.z));

        glm::quat rotation = nodeAnim.getInterpolatedRotation(animationTime);
        glm::mat4 rotationMatrix = glm::toMat4(rotation);

        glm::vec3 scale = nodeAnim.getInterpolatedScale(animationTime);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));

        nodeTransform = translationMatrix * rotationMatrix * scaleMatrix;

        globalTransformation = parentTransform * nodeTransform;

        unsigned int boneIndex = bonemapping[pNode.name];
        boneInfos[boneIndex].finalTransformation = globalTransformation * boneInfos[boneIndex].boneOffset;
    }

    for (auto& child : pNode.children) {
        ReadNodeHierarchy(animationTime, child, globalTransformation);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

void AnimationComponent::boneTransform(float dt) {
    /*
        This is bugged, Assimp docs say totalDuration is in ticks, but the actual value is real world time in milliseconds
        see https://github.com/assimp/assimp/issues/2662
    */
    animation.runningTime += dt;
    if (animation.runningTime > animation.totalDuration) {
        animation.runningTime = 0;
    }

    auto identity = glm::mat4(1.0f);
    ReadNodeHierarchy(animation.runningTime, boneTreeRootNode, identity);

    boneTransforms.resize(boneCount);
    for (int i = 0; i < boneCount; i++) {
        boneTransforms[i] = boneInfos[i].finalTransformation;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

void AnimationComponent::uploadRenderData(ecs::MeshComponent& mesh) {
    glCreateBuffers(1, &boneIndexBuffer);
    glNamedBufferData(boneIndexBuffer, boneIndices.size() * sizeof(glm::ivec4), boneIndices.data(), GL_STATIC_COPY);

    glCreateBuffers(1, &boneWeightBuffer);
    glNamedBufferData(boneWeightBuffer, boneWeights.size() * sizeof(glm::vec4), boneWeights.data(), GL_STATIC_COPY);

    glCreateBuffers(1, &boneTransformsBuffer);
    glNamedBufferData(boneTransformsBuffer, boneTransforms.size() * sizeof(glm::mat4), boneTransforms.data(), GL_DYNAMIC_READ);

    auto originalMeshBuffer = mesh.getVertexData();
    skinnedVertexBuffer.loadVertices(originalMeshBuffer.data(), originalMeshBuffer.size());
    skinnedVertexBuffer.setLayout(
        {
            { "POSITION",    ShaderType::FLOAT3 },
            { "UV",          ShaderType::FLOAT2 },
            { "NORMAL",      ShaderType::FLOAT3 },
            { "TANGENT",     ShaderType::FLOAT3 },
            { "BINORMAL",    ShaderType::FLOAT3 },
        });
}

/////////////////////////////////////////////////////////////////////////////////////////

void AnimationComponent::destroy() {
    glDeleteBuffers(1, &boneIndexBuffer);
    glDeleteBuffers(1, &boneWeightBuffer);
    glDeleteBuffers(1, &boneTransformsBuffer);
    skinnedVertexBuffer.destroy();
}

/////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::createAlbedoTexture() {
    glDeleteTextures(1, &albedo);

    glCreateTextures(GL_TEXTURE_2D, 1, &albedo);
    glTextureStorage2D(albedo, 1, GL_RGBA16F, 1, 1);
    glTextureSubImage2D(albedo, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(baseColour));

    glTextureParameteri(albedo, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(albedo, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(albedo, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(albedo, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(albedo, GL_TEXTURE_WRAP_R, GL_REPEAT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::createAlbedoTexture(std::shared_ptr<TextureAsset> texture) {
    if (!texture) {
        return;
    }

    auto header = texture->getHeader();
    auto dataPtr = texture->getData();
    albedoFile = texture->getPath().string();

    glDeleteTextures(1, &albedo);
    glCreateTextures(GL_TEXTURE_2D, 1, &albedo);
    glTextureStorage2D(albedo, header.dwMipMapCount, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, header.dwWidth, header.dwHeight);

    for (unsigned int mip = 0; mip < 1; mip++) {
        glm::ivec2 dimensions = { std::max(header.dwWidth >> mip, 1ul), std::max(header.dwHeight >> mip, 1ul) };
        size_t dataSize = std::max(1, ((dimensions.x + 3) / 4)) * std::max(1, ((dimensions.y + 3) / 4)) * 16;
        glCompressedTextureSubImage2D(albedo, mip, 0, 0, dimensions.x, dimensions.y, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, (GLsizei)dataSize, dataPtr);
        dataPtr += dimensions.x * dimensions.y;
    }

    glGenerateTextureMipmap(albedo);

    glTextureParameteri(albedo, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(albedo, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::createNormalTexture() {
    glDeleteTextures(1, &normals);

    constexpr auto tbnAxis = glm::vec<4, float>(0.5f, 0.5f, 1.0f, 1.0f);
    glCreateTextures(GL_TEXTURE_2D, 1, &normals);
    glTextureStorage2D(normals, 1, GL_RGBA16F, 1, 1);
    glTextureSubImage2D(normals, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(tbnAxis));
    glTextureParameteri(normals, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(normals, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(normals, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(normals, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(normals, GL_TEXTURE_WRAP_R, GL_REPEAT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::createNormalTexture(std::shared_ptr<TextureAsset> texture) {
    if (!texture) {
        return;
    }

    glDeleteTextures(1, &normals);

    auto header = texture->getHeader();
    auto dataPtr = texture->getData();

    glCreateTextures(GL_TEXTURE_2D, 1, &normals);
    glTextureStorage2D(normals, header.dwMipMapCount, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, header.dwWidth, header.dwHeight);

    for (unsigned int mip = 0; mip < 1; mip++) {
        glm::ivec2 size = { header.dwWidth >> mip, header.dwHeight >> mip };
        size_t dataSize = std::max(1, ((size.x + 3) / 4)) * std::max(1, ((size.y + 3) / 4)) * 16;
        glCompressedTextureSubImage2D(normals, mip, 0, 0, size.x, size.y, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, (GLsizei)dataSize, dataPtr);
        dataPtr += size.x * size.y;
    }

    glTextureParameteri(normals, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(normals, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateTextureMipmap(normals);

    normalFile = texture->getPath().string();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::createMetalRoughTexture() {
    glDeleteTextures(1, &metalrough);

    auto metalRoughnessValue = glm::vec4(0.0f, roughness, metallic, 1.0f);
    glCreateTextures(GL_TEXTURE_2D, 1, &metalrough);
    glTextureStorage2D(metalrough, 1, GL_RGBA16F, 1, 1);
    glTextureSubImage2D(metalrough, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(metalRoughnessValue));
    glTextureParameteri(metalrough, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(metalrough, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(metalrough, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(metalrough, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(metalrough, GL_TEXTURE_WRAP_R, GL_REPEAT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::createMetalRoughTexture(std::shared_ptr<TextureAsset> texture) {
    if (!texture) {
        return;
    }

    glDeleteTextures(1, &metalrough);

    auto header = texture->getHeader();
    auto dataPtr = texture->getData();

    glCreateTextures(GL_TEXTURE_2D, 1, &metalrough);
    auto mipmapLevels = static_cast<GLsizei>(1 + std::floor(std::log2(std::max(header.dwWidth, header.dwHeight))));
    glTextureStorage2D(metalrough, mipmapLevels, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, header.dwWidth, header.dwHeight);

    for (unsigned int mip = 0; mip < 1; mip++) {
        glm::ivec2 size = { header.dwWidth >> mip, header.dwHeight >> mip };
        size_t dataSize = std::max(1, ((size.x + 3) / 4)) * std::max(1, ((size.y + 3) / 4)) * 16;
        glCompressedTextureSubImage2D(metalrough, mip, 0, 0, size.x, size.y, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, (GLsizei)dataSize, dataPtr);
        dataPtr += size.x * size.y;
    }

    glGenerateTextureMipmap(metalrough);

    glTextureParameteri(metalrough, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(metalrough, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    mrFile = texture->getPath().string();
}

/////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::destroy() {
    glDeleteTextures(1, &albedo);
    glDeleteTextures(1, &normals);
    glDeleteTextures(1, &metalrough);
    albedo = 0, normals = 0, metalrough = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

MaterialComponent MaterialComponent::Default;

/////////////////////////////////////////////////////////////////////////////////////////

template<>
void clone<TransformComponent>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& component = reg.get<TransformComponent>(from);
    reg.emplace<TransformComponent>(to, component);
}

/////////////////////////////////////////////////////////////////////////////////////////


template<>
void clone<NodeComponent>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& fromNode = reg.get<NodeComponent>(from);
    auto& toNode = reg.emplace<NodeComponent>(to);
    if (fromNode.parent != entt::null) {
        NodeSystem::append(reg, reg.get<NodeComponent>(fromNode.parent), toNode);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

template<>
void clone<NameComponent>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& component = reg.get<NameComponent>(from);
    reg.emplace<NameComponent>(to, component);
}

/////////////////////////////////////////////////////////////////////////////////////////

template<>
void clone<MeshComponent>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& from_component = reg.get<MeshComponent>(from);
    auto& to_component = reg.emplace<MeshComponent>(to, from_component);
    to_component.uploadVertices();
    to_component.uploadIndices();
}

/////////////////////////////////////////////////////////////////////////////////////////

template<>
void clone<MaterialComponent>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& from_component = reg.get<MaterialComponent>(from);
    auto& to_component = reg.emplace<MaterialComponent>(to, from_component);
}

} // ECS
} // raekor