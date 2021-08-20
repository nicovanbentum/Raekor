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
  /*  on_destroy<Mesh>().connect<entt::invoke<&Mesh::destroy>>();
    on_destroy<Material>().connect<entt::invoke<&Material::destroy>>();*/
    on_destroy<Skeleton>().connect<entt::invoke<&Skeleton::destroy>>();
}

/////////////////////////////////////////////////////////////////////////////////////////

Scene::~Scene() {
    clear();
}

/////////////////////////////////////////////////////////////////////////////////////////

entt::entity Scene::createObject(const std::string& name) {
    auto entity = create();
    emplace<Name>(entity, name);
    emplace<Node>(entity);
    emplace<Transform>(entity);
    return entity;
}

/////////////////////////////////////////////////////////////////////////////////////////

entt::entity Scene::pickObject(Math::Ray& ray) {
    entt::entity pickedEntity = entt::null;
    std::map<float, entt::entity> boxesHit;

    auto entities = view<Mesh, Transform>();
    for (auto entity : entities) {
        auto& mesh = entities.get<Mesh>(entity);
        auto& transform = entities.get<Transform>(entity);

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
        auto& mesh = entities.get<Mesh>(pair.second);
        auto& transform = entities.get<Transform>(pair.second);

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

void Scene::bindScript(entt::entity entity, NativeScript& script) {
    auto address = GetProcAddress(script.asset->getModule(), script.procAddress.c_str());
    if (address) {
        auto function = reinterpret_cast<INativeScript::FactoryType>(address);
        script.script = function();
        script.script->bind(entity, *this);
    } else {
        std::clog << "Failed to "
            "bind script " << script.file <<
            " to entity " << entt::to_integral(entity) <<
            " from class " << script.procAddress << '\n';
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::destroyObject(entt::entity entity) {
    if (has<Node>(entity)) {
        auto tree = NodeSystem::getFlatHierarchy(*this, get<Node>(entity));
        
        for (auto member : tree) {
            NodeSystem::remove(*this, get<Node>(member));
            destroy(member);
        }
    
        NodeSystem::remove(*this, get<Node>(entity));
    }

    destroy(entity);
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::updateNode(entt::entity node, entt::entity parent) {
    auto& transform = get<Transform>(node);
    
    if (parent == entt::null) {
        transform.worldTransform = transform.localTransform;
    } else {
        auto& parentTransform = get<Transform>(parent);
        transform.worldTransform = parentTransform.worldTransform * transform.localTransform;
    }

    auto& comp = get<Node>(node);

    auto curr = comp.firstChild;
    while (curr != entt::null) {
        updateNode(curr, node);
        curr = get<Node>(curr).nextSibling;
    }
}

void Scene::updateTransforms() {
    auto nodeView = view<Node, Transform>();

    for (auto entity : nodeView) {
        auto& [node, transform] = nodeView.get<Node, Transform>(entity);

        if (node.parent == entt::null) {
            transform.compose();
            updateNode(entity, node.parent);
        }
    }
}

void Scene::updateLights() {
    auto dirLights = view<DirectionalLight, Transform>();

    for (auto entity : dirLights) {
        auto& light = dirLights.get<DirectionalLight>(entity);
        auto& transform = dirLights.get<Transform>(entity);
        light.direction = glm::vec4(static_cast<glm::quat>(transform.rotation) * glm::vec3(0, -1, 0), 1.0);
    }

    auto pointLights = view<PointLight, Transform>();

    for (auto entity : pointLights) {
        auto& light = pointLights.get<PointLight>(entity);
        auto& transform = pointLights.get<Transform>(entity);
        light.position = glm::vec4(transform.position, 1.0f);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::loadMaterialTextures(Async& async, Assets& assets, const std::vector<entt::entity>& materials) {
    Timer timer;
    timer.start();

    for (const auto& entity : materials) {
        async.dispatch([&]() {
            auto& material = this->get<Material>(entity);
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
        auto& material = get<Material>(entity);

       /* material.createAlbedoTexture(assets.get<TextureAsset>(material.albedoFile));
        material.createNormalTexture(assets.get<TextureAsset>(material.normalFile));
        material.createMetalRoughTexture(assets.get<TextureAsset>(material.mrFile));*/
    }

    timer.stop();
    std::cout << "Upload texture time " << timer.elapsedMs() << std::endl;
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::saveToFile(const std::string& file) {
    std::ofstream outstream(file, std::ios::binary);
    cereal::BinaryOutputArchive output(outstream);
    entt::snapshot{ *this }.entities(output).component <
        Name, Node, Transform,
        Mesh, Material, PointLight,
        DirectionalLight >(output);
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
        Name, Node, Transform,
        Mesh, Material, PointLight,
        DirectionalLight >(input);

    timer.stop();
    std::cout << "Archive time " << timer.elapsedMs() << std::endl;


    // init material render data
    auto materials = view<Material>();
    auto materialEntities = std::vector<entt::entity>();
    materialEntities.assign(materials.data(), materials.data() + materials.size());
    loadMaterialTextures(async, assets, materialEntities);

    timer.start();

    // init mesh render data
    auto entities = view<Mesh>();
    for (auto entity : entities) {
        auto& mesh = entities.get<Mesh>(entity);
        mesh.generateAABB();
        mesh.uploadVertices();
        mesh.uploadIndices();
    }

    timer.stop();
    std::cout << "Mesh time " << timer.elapsedMs() << std::endl << std::endl;
}

} // Raekor