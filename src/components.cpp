#include "pch.h"
#include "components.h"

namespace Raekor {
namespace ecs {

void TransformComponent::recalculateMatrix() {
    matrix = glm::translate(glm::mat4(1.0f), position);
    auto rotationQuat = static_cast<glm::quat>(rotation);
    matrix = matrix * glm::toMat4(rotationQuat);
    matrix = glm::scale(matrix, scale);
}

/////////////////////////////////////////////////////////////////////////////////////////

void MeshComponent::generateAABB() {
    aabb[0] = vertices[0].pos;
    aabb[1] = vertices[1].pos;
    for (auto& v : vertices) {
        aabb[0] = glm::min(aabb[0], v.pos);
        aabb[1] = glm::max(aabb[1], v.pos);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////////////////

void MeshComponent::uploadIndices() {
    indexBuffer.loadIndices(indices.data(), indices.size());
}

/////////////////////////////////////////////////////////////////////////////////////////

void MeshAnimationComponent::ReadNodeHierarchy(float animationTime, BoneTreeNode& pNode, const glm::mat4& parentTransform) {
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

void MeshAnimationComponent::boneTransform(float dt) {
    /*
        This is bugged, Assimp docs say totalDuration is in ticks, but the actual value is real world time in milliseconds
        see https://github.com/assimp/assimp/issues/2662
    */
    animation.runningTime += dt;
    if (animation.runningTime > animation.totalDuration) animation.runningTime = 0;

    auto identity = glm::mat4(1.0f);
    ReadNodeHierarchy(animation.runningTime, boneTreeRootNode, identity);

    boneTransforms.resize(boneCount);
    for (int i = 0; i < boneCount; i++) {
        boneTransforms[i] = boneInfos[i].finalTransformation;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

void MeshAnimationComponent::uploadRenderData(ecs::MeshComponent& mesh) {
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

/////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::createAlbedoTexture() {
    if (albedo) glDeleteTextures(1, albedo.get());
    albedo = std::make_shared<unsigned int>();

    glCreateTextures(GL_TEXTURE_2D, 1, albedo.get());
    glTextureStorage2D(*albedo, 1, GL_SRGB8_ALPHA8, 1, 1);
    glTextureSubImage2D(*albedo, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(baseColour));

    glTextureParameteri(*albedo, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(*albedo, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(*albedo, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(*albedo, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(*albedo, GL_TEXTURE_WRAP_R, GL_REPEAT);
}

void MaterialComponent::createAlbedoTexture(const Stb::Image& image) {
    if (albedo) glDeleteTextures(1, albedo.get());
    albedo = std::make_shared<unsigned int>();

    glCreateTextures(GL_TEXTURE_2D, 1, albedo.get());
    auto mipmapLevels = static_cast<GLsizei>(1 + std::floor(std::log2(std::max(image.w, image.h))));
    glTextureStorage2D(*albedo, mipmapLevels, GL_SRGB8_ALPHA8, image.w, image.h);
    glTextureSubImage2D(*albedo, 0, 0, 0, image.w, image.h, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);
    glTextureParameteri(*albedo, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(*albedo, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateTextureMipmap(*albedo);
}

void MaterialComponent::createNormalTexture() {
    if (normals) glDeleteTextures(1, normals.get());
    normals = std::make_shared<unsigned int>();

    constexpr auto tbnAxis = glm::vec<4, float>(0.5f, 0.5f, 1.0f, 1.0f);
    glCreateTextures(GL_TEXTURE_2D, 1, normals.get());
    glTextureStorage2D(*normals, 1, GL_RGBA16F, 1, 1);
    glTextureSubImage2D(*normals, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(tbnAxis));
    glTextureParameteri(*normals, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(*normals, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(*normals, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(*normals, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(*normals, GL_TEXTURE_WRAP_R, GL_REPEAT);
}

void MaterialComponent::createNormalTexture(const Stb::Image& image)
{
    if (normals) glDeleteTextures(1, normals.get());
    normals = std::make_shared<unsigned int>();

    glCreateTextures(GL_TEXTURE_2D, 1, normals.get());
    auto mipmapLevels = static_cast<GLsizei>(1 + std::floor(std::log2(std::max(image.w, image.h))));
    glTextureStorage2D(*normals, mipmapLevels, GL_RGBA8, image.w, image.h);
    glTextureSubImage2D(*normals, 0, 0, 0, image.w, image.h, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);
    glTextureParameteri(*normals, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(*normals, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateTextureMipmap(*normals);
}

void MaterialComponent::createMetalRoughTexture() {
    if (metalrough) glDeleteTextures(1, metalrough.get());
    metalrough = std::make_shared<unsigned int>();

    auto metalRoughnessValue = glm::vec4(metallic, roughness, 0.0f, 1.0f);
    glCreateTextures(GL_TEXTURE_2D, 1, metalrough.get());
    glTextureStorage2D(*metalrough, 1, GL_RGBA16F, 1, 1);
    glTextureSubImage2D(*metalrough, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(metalRoughnessValue));
    glTextureParameteri(*metalrough, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(*metalrough, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(*metalrough, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(*metalrough, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(*metalrough, GL_TEXTURE_WRAP_R, GL_REPEAT);
}

void MaterialComponent::createMetalRoughTexture(const Stb::Image& image) {
    if (metalrough) glDeleteTextures(1, metalrough.get());
    metalrough = std::make_shared<unsigned int>();

    glCreateTextures(GL_TEXTURE_2D, 1, metalrough.get());
    auto mipmapLevels = static_cast<GLsizei>(1 + std::floor(std::log2(std::max(image.w, image.h))));
    glTextureStorage2D(*metalrough, mipmapLevels, GL_RGBA8, image.w, image.h);
    glTextureSubImage2D(*metalrough, 0, 0, 0, image.w, image.h, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);
    glTextureParameteri(*metalrough, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(*metalrough, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenerateTextureMipmap(*metalrough);
}

void MaterialComponent::uploadRenderData() {
    createAlbedoTexture();
    createNormalTexture();
    createMetalRoughTexture();
}

/////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::uploadRenderData(const std::unordered_map<std::string, Stb::Image>& images) {
    auto albedoEntry = images.find(albedoFile);
    albedo = std::make_shared<unsigned int>();

    auto normalsEntry = images.find(normalFile);
    normals = std::make_shared<unsigned int>();
    
    auto metalroughEntry = images.find(mrFile);
    metalrough = std::make_shared<unsigned int>();
    
    if (albedoEntry != images.end() && !albedoEntry->first.empty()) {
        createAlbedoTexture(albedoEntry->second);
    } else createAlbedoTexture();
    
    if (normalsEntry != images.end() && !normalsEntry->first.empty()) {
        createNormalTexture(normalsEntry->second);
    } else createNormalTexture();

    if (metalroughEntry != images.end() && !metalroughEntry->first.empty()) {
        createMetalRoughTexture(metalroughEntry->second);
    } else createMetalRoughTexture();
}

/////////////////////////////////////////////////////////////////////////////////////////

template<>
void clone<TransformComponent>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& component = reg.get<TransformComponent>(from);
    reg.emplace<TransformComponent>(to, component);
}

/////////////////////////////////////////////////////////////////////////////////////////

template<>
void clone<NodeComponent>(entt::registry& reg, entt::entity from, entt::entity to) {
    auto& component = reg.get<NodeComponent>(from);
    reg.emplace<NodeComponent>(to, component);
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
    to_component.uploadRenderData();
}

/////////////////////////////////////////////////////////////////////////////////////////

cloner::cloner() {
    clone_functions[entt::type_info<NameComponent>::id()] = &clone<NameComponent>;
    clone_functions[entt::type_info<NodeComponent>::id()] = &clone<NodeComponent>;
    clone_functions[entt::type_info<MeshComponent>::id()] = &clone<MeshComponent>;
    clone_functions[entt::type_info<MaterialComponent>::id()] = &clone<MaterialComponent>;
    clone_functions[entt::type_info<TransformComponent>::id()] = &clone<TransformComponent>;
}

/////////////////////////////////////////////////////////////////////////////////////////

cloner::clone_fn_type* cloner::getFunction(entt::id_type id_type) { return clone_functions[id_type]; }

/////////////////////////////////////////////////////////////////////////////////////////

cloner* cloner::getSingleton() {
    static std::unique_ptr<cloner> cl = std::make_unique<cloner>();
    return cl.get();
}

} // ECS
} // raekor