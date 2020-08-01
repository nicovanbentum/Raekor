#pragma once

#include "mesh.h"
#include "camera.h"
#include "texture.h"

#include "ecs.h"
#include "components.h"
#include "async.h"

namespace Raekor {

static entt::entity createEmpty(entt::registry& registry, const std::string& name = "Empty") {
    auto entity = registry.create();
    auto comp = registry.emplace<ecs::NameComponent>(entity, name);
    registry.emplace<ecs::NodeComponent>(entity);
    registry.emplace<ecs::TransformComponent>(entity);
    return entity;
}

class AssimpImporter {
public:
    AssimpImporter() {}

    bool loadFile(entt::registry& scene, AsyncDispatcher& dispatcher, const std::string& file);

private:
    std::vector<entt::entity>   loadMaterials(entt::registry& scene, const aiScene* aiscene, const std::string& directory);
    entt::entity                loadMesh(entt::registry& scene, aiMesh* assimpMesh);
    void                        loadBones(entt::registry& scene, const aiScene* aiscene, aiMesh* assimpMesh, entt::entity entity);
};

// TODO: move this stuff into classes
void updateTransforms(entt::registry& scene);
entt::entity pickObject(entt::registry& scene, Math::Ray& ray);
void destroyNode(entt::registry& scene, entt::entity entity);
void loadMaterialTextures(const ecs::view<ecs::MaterialComponent>& materials, AsyncDispatcher& dispatcher);

} // Namespace Raekor