#pragma once

#include "mesh.h"
#include "camera.h"
#include "texture.h"

#include "ecs.h"
#include "components.h"
#include "async.h"

namespace Raekor {

static entt::entity createEmpty(entt::registry& registry, const std::string& name) {
    auto entity = registry.create();
    auto comp = registry.emplace<ECS::NameComponent>(entity, name);
    registry.emplace<ECS::NodeComponent>(entity);
    registry.emplace<ECS::TransformComponent>(entity);
    return entity;
}


class AssimpImporter {
public:
    AssimpImporter() {}
    void loadFromDisk(entt::registry& scene, const std::string& file, AsyncDispatcher& dispatcher);

private:
    void processAiNode(entt::registry& scene, const aiScene* aiscene, aiNode* node, entt::entity root);

    // TODO: every mesh in the file is created as an Entity that has 1 name, 1 mesh and 1 material component
    // we might want to incorporate meshrenderers and seperate entities for materials
    void loadMesh(const aiScene* aiscene, entt::registry& scene, aiMesh* assimpMesh, aiMaterial* assimpMaterial, aiMatrix4x4 localTransform, entt::entity root);

private:
    std::unordered_map<std::string, Stb::Image> images;
};

void updateTransforms(entt::registry& scene);

entt::entity pickObject(entt::registry& scene, Math::Ray& ray);
void destroyNode(entt::registry& scene, entt::entity entity);
void loadAssetsFromDisk(entt::registry& scene, AsyncDispatcher& dispatcher, const std::string& directory = "");


} // Namespace Raekor