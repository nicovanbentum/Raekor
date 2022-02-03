#include "pch.h"
#include "scene.h"

#include "timer.h"
#include "serial.h"
#include "systems.h"
#include "async.h"

namespace Raekor
{

Scene::~Scene() {
    clear();
}



entt::entity Scene::createObject(const std::string& name) {
    auto entity = create();
    emplace<Name>(entity, name);
    emplace<Node>(entity);
    emplace<Transform>(entity);
    return entity;
}



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



void Scene::destroyObject(entt::entity entity) {
    if (all_of<Node>(entity)) {
        auto tree = NodeSystem::getFlatHierarchy(*this, get<Node>(entity));
        
        for (auto member : tree) {
            NodeSystem::remove(*this, get<Node>(member));
            destroy(member);
        }
    
        NodeSystem::remove(*this, get<Node>(entity));
    }

    destroy(entity);
}



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
    auto transforms = view<Node, Transform>();

    for (auto& [entity, node, transform] : transforms.each()) {

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



void Scene::loadMaterialTextures(Assets& assets, const std::vector<entt::entity>& materials) {
    Timer timer;
    timer.start();

    for (const auto& entity : materials) {
        Async::dispatch([&]() {
            auto& material = this->get<Material>(entity);
            assets.get<TextureAsset>(material.albedoFile);
            assets.get<TextureAsset>(material.normalFile);
            assets.get<TextureAsset>(material.metalroughFile);
        });
    }

    Async::wait();

    timer.stop();
    std::cout << "Async texture time " << timer.elapsedMs() << '\n';

    timer.start();
    for (auto entity : materials) {
        auto& material = get<Material>(entity);

        if (m_UploadMaterialCallback) {
            m_UploadMaterialCallback(material, assets);
        }
    }

    timer.stop();
    std::cout << "Upload texture time " << timer.elapsedMs() << '\n';
}



void Scene::saveToFile(Assets& assets, const std::string& file) {
    std::stringstream bufferstream;
    
    {
        cereal::BinaryOutputArchive output(bufferstream);
        entt::snapshot{ *this }.entities(output).component<
            Name, Node, Transform,
            Mesh, Material, PointLight,
            DirectionalLight>(output);

        output(assets);
    }

    auto buffer = bufferstream.str();

    const auto bound = LZ4_compressBound(int(buffer.size()));

    std::vector<char> compressed(bound);
    Timer timer;
    timer.start();

    auto compressedSize = LZ4_compress_default(buffer.c_str(), compressed.data(), int(buffer.size()), bound);

    timer.stop();
    std::cout << "Compression time: " << timer.elapsedMs() << '\n';

    std::ofstream filestream(file, std::ios::binary);
    filestream.write(compressed.data(), compressedSize);
}



void Scene::openFromFile(Assets& assets, const std::string& file) {
    if (!fs::is_regular_file(file)) {
        std::clog << "silent return in Scene::openFromFile : filepath " << file << " does not exist.";
        return;
    }
    std::ifstream storage(file, std::ios::binary);

    //Read file into buffer
    std::string buffer;
    storage.seekg(0, std::ios::end);
    buffer.resize(storage.tellg());
    storage.seekg(0, std::ios::beg);
    storage.read(&buffer[0], buffer.size());

    auto decompressed = std::string();
    decompressed.resize(buffer.size() * 11); // TODO: store the uncompressed size somewhere
    const int decompressed_size = LZ4_decompress_safe(buffer.data(), decompressed.data(), int(buffer.size()), int(decompressed.size()));

    clear();

    Timer timer;
    timer.start();

    // stringstream to serialise from char data back to scene/asset representation
    {
        auto bufferstream = std::stringstream(decompressed);
        cereal::BinaryInputArchive input(bufferstream);
    
        // use cereal -> entt snapshot loader to load scene
        entt::snapshot_loader{ *this }.entities(input).component <
            Name, Node, Transform,
            Mesh, Material, PointLight,
            DirectionalLight >(input);

        // load assets
        input(assets);
    }

    timer.stop();
    std::cout << "Archive time " << timer.elapsedMs() << '\n';


    // init material render data
    auto materials = view<Material>();
    auto materialEntities = std::vector<entt::entity>();
    materialEntities.assign(materials.data(), materials.data() + materials.size());
    loadMaterialTextures(assets, materialEntities);

    timer.start();

    // init mesh render data
    for (auto& [entity, mesh] : view<Mesh>().each()) {
        mesh.generateAABB();
        if (m_UploadMeshCallback) {
            m_UploadMeshCallback(mesh);
        }
    }

    timer.stop();
    std::cout << "Mesh time " << timer.elapsedMs() << "\n\n";
}

} // Raekor