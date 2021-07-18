#include "pch.h"
#include "scene.h"

#include "mesh.h"
#include "timer.h"
#include "serial.h"
#include "systems.h"
#include "async.h"

namespace Raekor
{

Scene::Scene() {
    on_destroy<ecs::MeshComponent>().connect<entt::invoke<&ecs::MeshComponent::destroy>>();
    on_destroy<ecs::MaterialComponent>().connect<entt::invoke<&ecs::MaterialComponent::destroy>>();
    on_destroy<ecs::AnimationComponent>().connect<entt::invoke<&ecs::AnimationComponent::destroy>>();
}

/////////////////////////////////////////////////////////////////////////////////////////

Scene::~Scene() {
    clear();
}

/////////////////////////////////////////////////////////////////////////////////////////

entt::entity Scene::createObject(const std::string& name) {
    auto entity = create();
    emplace<ecs::NameComponent>(entity, name);
    emplace<ecs::NodeComponent>(entity);
    emplace<ecs::TransformComponent>(entity);
    return entity;
}

/////////////////////////////////////////////////////////////////////////////////////////

entt::entity Scene::pickObject(Math::Ray& ray) {
    entt::entity pickedEntity = entt::null;
    std::map<float, entt::entity> boxesHit;

    auto entities = view<ecs::MeshComponent, ecs::TransformComponent>();
    for (auto entity : entities) {
        auto& mesh = entities.get<ecs::MeshComponent>(entity);
        auto& transform = entities.get<ecs::TransformComponent>(entity);

        // convert AABB from local to world space
        std::array<glm::vec3, 2> worldAABB =
        {
            transform.worldTransform * glm::vec4(mesh.aabb[0], 1.0),
            transform.worldTransform * glm::vec4(mesh.aabb[1], 1.0)
        };

        // check for ray hit
        auto scaledMin = mesh.aabb[0] * transform.scale;
        auto scaledMax = mesh.aabb[1] * transform.scale;
        auto hitResult = ray.hitsOBB(scaledMin, scaledMax, transform.worldTransform);

        if (hitResult.has_value()) {
            boxesHit[hitResult.value()] = entity;
        }
    }

    for (auto& pair : boxesHit) {
        auto& mesh = entities.get<ecs::MeshComponent>(pair.second);
        auto& transform = entities.get<ecs::TransformComponent>(pair.second);

        for (unsigned int i = 0; i < mesh.indices.size(); i += 3) {
            auto v0 = glm::vec3(transform.worldTransform * glm::vec4(mesh.positions[mesh.indices[i]], 1.0));
            auto v1 = glm::vec3(transform.worldTransform * glm::vec4(mesh.positions[mesh.indices[i + 1]], 1.0));
            auto v2 = glm::vec3(transform.worldTransform * glm::vec4(mesh.positions[mesh.indices[i + 2]], 1.0));

            auto triangleHitResult = ray.hitsTriangle(v0, v1, v2);
            if (triangleHitResult.has_value()) {
                pickedEntity = pair.second;
                return pickedEntity;
            }
        }
    }

    return pickedEntity;
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::destroyObject(entt::entity entity) {
    if (has<ecs::NodeComponent>(entity)) {
        auto tree = NodeSystem::getFlatHierarchy(*this, get<ecs::NodeComponent>(entity));
        for (auto member : tree) {
            NodeSystem::remove(*this, get<ecs::NodeComponent>(member));
            destroy(member);
        }
    }

    destroy(entity);
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::updateNode(entt::entity node, entt::entity parent) {
    auto& transform = get<ecs::TransformComponent>(node);
    
    if (parent == entt::null) {
        transform.worldTransform = transform.localTransform;
    } else {
        auto& parentTransform = get<ecs::TransformComponent>(parent);
        transform.worldTransform = parentTransform.worldTransform * transform.localTransform;
    }

    auto& comp = get<ecs::NodeComponent>(node);

    auto curr = comp.firstChild;
    while (curr != entt::null) {
        updateNode(curr, node);
        curr = get<ecs::NodeComponent>(curr).nextSibling;
    }
}

void Scene::updateTransforms() {
    auto nodeView = view<ecs::NodeComponent, ecs::TransformComponent>();

    for (auto entity : nodeView) {
        auto& [node, transform] = nodeView.get<ecs::NodeComponent, ecs::TransformComponent>(entity);

        if (node.parent == entt::null) {
            updateNode(entity, node.parent);
        }

    }
}

void Scene::updateLights() {
    auto dirLights = view<ecs::DirectionalLightComponent, ecs::TransformComponent>();

    for (auto entity : dirLights) {
        auto& light = dirLights.get<ecs::DirectionalLightComponent>(entity);
        auto& transform = dirLights.get<ecs::TransformComponent>(entity);
        light.direction = glm::vec4(static_cast<glm::quat>(transform.rotation) * glm::vec3(0, -1, 0), 1.0);
    }

    auto pointLights = view<ecs::PointLightComponent, ecs::TransformComponent>();

    for (auto entity : pointLights) {
        auto& light = pointLights.get<ecs::PointLightComponent>(entity);
        auto& transform = pointLights.get<ecs::TransformComponent>(entity);
        light.position = glm::vec4(transform.position, 1.0f);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::loadMaterialTextures(Async& async, Assets& assets, const std::vector<entt::entity>& materials) {
    Timer timer;
    timer.start();

    for (const auto& entity : materials) {
        async.dispatch([&]() {
            auto& material = this->get<ecs::MaterialComponent>(entity);
            assets.get<TextureAsset>(material.albedoFile);
            assets.get<TextureAsset>(material.normalFile);
            assets.get<TextureAsset>(material.mrFile);
        });
    }

    async.wait();

    timer.stop();
    std::cout << "Async texture time " << timer.elapsedMs() << std::endl;

    timer.start();
    for (auto entity : materials) {
        auto& material = get<ecs::MaterialComponent>(entity);

        material.createAlbedoTexture(assets.get<TextureAsset>(material.albedoFile));
        material.createNormalTexture(assets.get<TextureAsset>(material.normalFile));

        auto mrTexture = assets.get<TextureAsset>(material.mrFile);
        if (mrTexture) {
            material.createMetalRoughTexture(mrTexture);
        } else {
            material.createMetalRoughTexture();
        }
    }

    timer.stop();
    std::cout << "Upload texture time " << timer.elapsedMs() << std::endl;
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::saveToFile(const std::string& file) {
    std::ofstream outstream(file, std::ios::binary);
    cereal::BinaryOutputArchive output(outstream);
    entt::snapshot{ *this }.entities(output).component <
        ecs::NameComponent, ecs::NodeComponent, ecs::TransformComponent,
        ecs::MeshComponent, ecs::MaterialComponent, ecs::PointLightComponent,
        ecs::DirectionalLightComponent >(output);
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::openFromFile(Async& async, Assets& assets, const std::string& file) {
    if (!std::filesystem::is_regular_file(file)) {
        return;
    }
    std::ifstream storage(file, std::ios::binary);

    // TODO: lz4 compression
    ////Read file into buffer
    //std::string buffer;
    //storage.seekg(0, std::ios::end);
    //buffer.resize(storage.tellg());
    //storage.seekg(0, std::ios::beg);
    //storage.read(&buffer[0], buffer.size());

    //char* const regen_buffer = (char*)malloc(buffer.size());
    //const int decompressed_size = LZ4_decompress_safe(buffer.data(), regen_buffer, buffer.size(), bound_size);

    clear();

    Timer timer;
    timer.start();

    cereal::BinaryInputArchive input(storage);
    entt::snapshot_loader{ *this }.entities(input).component <
        ecs::NameComponent, ecs::NodeComponent, ecs::TransformComponent,
        ecs::MeshComponent, ecs::MaterialComponent, ecs::PointLightComponent,
        ecs::DirectionalLightComponent >(input);

    timer.stop();
    std::cout << "Archive time " << timer.elapsedMs() << std::endl;


    // init material render data
    auto materials = view<ecs::MaterialComponent>();
    auto materialEntities = std::vector<entt::entity>();
    materialEntities.assign(materials.data(), materials.data() + materials.size());
    loadMaterialTextures(async, assets, materialEntities);

    timer.start();

    // init mesh render data
    auto entities = view<ecs::MeshComponent>();
    for (auto entity : entities) {
        auto& mesh = entities.get<ecs::MeshComponent>(entity);
        mesh.generateAABB();
        mesh.uploadVertices();
        mesh.uploadIndices();
    }

    timer.stop();
    std::cout << "Mesh time " << timer.elapsedMs() << std::endl << std::endl;
}

} // Raekor