#include "pch.h"
#include "scene.h"

#include "timer.h"
#include "serial.h"
#include "systems.h"
#include "async.h"

namespace Raekor {

entt::entity Scene::PickSpatialEntity(Math::Ray& ray) {
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
        auto hitResult = ray.HitsOBB(scaledMin, scaledMax, transform.worldTransform);

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

            auto triangleHitResult = ray.HitsTriangle(v0, v1, v2);
            if (triangleHitResult.has_value()) {
                pickedEntity = pair.second;
                return pickedEntity;
            }
        }
    }

    return pickedEntity;
}


entt::entity Scene::CreateSpatialEntity(const std::string& name) {
    auto entity = create();
    emplace<Name>(entity, name);
    emplace<Node>(entity);
    emplace<Transform>(entity);
    return entity;
}


void Scene::DestroySpatialEntity(entt::entity entity) {
    if (all_of<Node>(entity)) {
        auto tree = NodeSystem::sGetFlatHierarchy(*this, get<Node>(entity));

        for (auto member : tree) {
            NodeSystem::sRemove(*this, get<Node>(member));
            destroy(member);
        }

        NodeSystem::sRemove(*this, get<Node>(entity));
    }

    destroy(entity);
}


void Scene::UpdateSelfAndChildNodes(entt::entity node, entt::entity parent) {
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
        UpdateSelfAndChildNodes(curr, node);
        curr = get<Node>(curr).nextSibling;
    }
}


void Scene::UpdateTransforms() {
    for (auto& [entity, node, transform] : view<Node,Transform>().each()) {
        transform.Compose();
        
        if (node.parent == entt::null) {
            UpdateSelfAndChildNodes(entity, node.parent);
        }
    }
}


void Scene::UpdateLights() {
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


void Scene::LoadMaterialTextures(Assets& assets, const std::vector<entt::entity>& materials) {
    Timer timer;

    for (const auto& entity : materials) {
        Async::sQueueJob([&]() {
            auto& material = this->get<Material>(entity);
            assets.Get<TextureAsset>(material.albedoFile);
            assets.Get<TextureAsset>(material.normalFile);
            assets.Get<TextureAsset>(material.metalroughFile);
        });
    }

    Async::sWait();

    std::cout << "Async texture time " << Timer::sToMilliseconds(timer.Restart()) << '\n';

    for (auto entity : materials) {
        auto& material = get<Material>(entity);

        if (m_UploadMaterialCallback)
            m_UploadMaterialCallback(material, assets);
    }

    std::cout << "Upload texture time " << Timer::sToMilliseconds(timer.Restart()) << '\n';
}


void Scene::SaveToFile(Assets& assets, const std::string& file) {
    std::stringstream bufferstream;
    
    {
        cereal::BinaryOutputArchive output(bufferstream);
        entt::snapshot{ *this }.entities(output).component<
            Name, Node, Transform,
            Mesh, Material, PointLight,
            DirectionalLight, BoxCollider>(output);

        output(assets);
    }

    auto buffer = bufferstream.str();

    const auto bound = LZ4_compressBound(int(buffer.size()));

    std::vector<char> compressed(bound);
    Timer timer;

    auto compressedSize = LZ4_compress_default(buffer.c_str(), compressed.data(), int(buffer.size()), bound);

    std::cout << "Compression time: " << Timer::sToMilliseconds(timer.GetElapsedTime()) << '\n';

    std::ofstream filestream(file, std::ios::binary);
    filestream.write(compressed.data(), compressedSize);
}


void Scene::OpenFromFile(Assets& assets, const std::string& file) {
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
    
    decompressed.resize(glm::min(buffer.size() * 11, size_t(INT_MAX) - 1)); // TODO: store the uncompressed size somewhere
    const int decompressed_size = LZ4_decompress_safe(buffer.data(), decompressed.data(), int(buffer.size()), int(decompressed.size()));

    clear();

    Timer timer;

    // stringstream to serialise from char data back to scene/asset representation
    {
        decompressed[decompressed.size() - 1] = '\0';
        auto bufferstream = std::istringstream(decompressed);
        cereal::BinaryInputArchive input(bufferstream);
    
        // use cereal -> entt snapshot loader to load scene
        entt::snapshot_loader{ *this }.entities(input).component <
            Name, Node, Transform,
            Mesh, Material, PointLight,
            DirectionalLight, BoxCollider >(input);

        // load assets
        input(assets);
    }

    std::cout << "Archive time " << Timer::sToMilliseconds(timer.GetElapsedTime()) << '\n';

    // init material render data
    auto materials = view<Material>();
    auto materialEntities = std::vector<entt::entity>();
    materialEntities.assign(materials.data(), materials.data() + materials.size());
    LoadMaterialTextures(assets, materialEntities);

    timer.Restart();

    // init mesh render data
    for (auto& [entity, mesh] : view<Mesh>().each()) {
        //for (auto& pos : mesh.positions)
        //    pos *= 0.05;

        //for (auto& normal : mesh.normals)
        //    normal *= 0.05;

        //for (auto& tangent : mesh.tangents)
        //    tangent *= 0.05;

        mesh.CalculateAABB();
        if (m_UploadMeshCallback) {
            m_UploadMeshCallback(mesh);
        }
    }

    std::cout << "Mesh time " << Timer::sToMilliseconds(timer.GetElapsedTime()) << "\n\n";
}


void Scene::BindScriptToEntity(entt::entity entity, NativeScript& script) {
    auto address = GetProcAddress(script.asset->GetModule(), script.procAddress.c_str());
    if (address) {
        auto function = reinterpret_cast<INativeScript::FactoryType>(address);
        script.script = function();
        script.script->Bind(entity, *this);
    }
    else {
        std::clog << "Failed to "
            "bind script " << script.file <<
            " to entity " << entt::to_integral(entity) <<
            " from class " << script.procAddress << '\n';
    }
}

} // Raekor