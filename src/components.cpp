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

void MaterialComponent::uploadRenderData() {
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

/////////////////////////////////////////////////////////////////////////////////////////

void MaterialComponent::uploadRenderData(const std::unordered_map<std::string, Stb::Image>& images) {
    auto albedoEntry = images.find(albedoFile);
    albedo = std::make_unique<glTexture2D>();
    albedo->bind();

    if (albedoEntry != images.end() && !albedoEntry->first.empty()) {
        const Stb::Image& image = albedoEntry->second;
        albedo->init(image.w, image.h, Format::SRGBA_U8, image.pixels);
        albedo->setFilter(Sampling::Filter::Trilinear);
        albedo->genMipMaps();
    }
    else {
        albedo->init(1, 1, { GL_SRGB_ALPHA, GL_RGBA, GL_FLOAT }, glm::value_ptr(baseColour));
        albedo->setFilter(Sampling::Filter::None);
        albedo->setWrap(Sampling::Wrap::Repeat);
    }

    auto normalsEntry = images.find(normalFile);
    normals = std::make_unique<glTexture2D>();
    normals->bind();

    if (normalsEntry != images.end() && !normalsEntry->first.empty()) {
        const Stb::Image& image = normalsEntry->second;
        normals->init(image.w, image.h, Format::RGBA_U8, image.pixels);
        normals->setFilter(Sampling::Filter::Trilinear);
        normals->genMipMaps();
    }
    else {
        constexpr auto tbnAxis = glm::vec<4, float>(0.5f, 0.5f, 1.0f, 1.0f);
        normals->init(1, 1, { GL_RGBA16F, GL_RGBA, GL_FLOAT }, glm::value_ptr(tbnAxis));
        normals->setFilter(Sampling::Filter::None);
        normals->setWrap(Sampling::Wrap::Repeat);
    }

    auto metalroughEntry = images.find(mrFile);
    metalrough = std::make_unique<glTexture2D>();

    if (metalroughEntry != images.end() && !metalroughEntry->first.empty()) {
        const Stb::Image& image = metalroughEntry->second;
        metalrough->bind();
        metalrough->init(image.w, image.h, { GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE }, image.pixels);
        metalrough->setFilter(Sampling::Filter::None);
        normals->genMipMaps();
    }
    else {
        auto metalRoughnessValue = glm::vec4(metallic, roughness, 0.0f, 1.0f);
        metalrough->bind();
        metalrough->init(1, 1, { GL_RGBA16F, GL_RGBA, GL_FLOAT }, glm::value_ptr(metalRoughnessValue));
        metalrough->setFilter(Sampling::Filter::None);
        metalrough->setWrap(Sampling::Wrap::Repeat);
    }
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