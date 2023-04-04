#include "pch.h"
#include "scene.h"

#include "timer.h"
#include "systems.h"

namespace Raekor {

entt::entity Scene::PickSpatialEntity(Ray& inRay) {
    auto picked_entity = entt::entity(entt::null);
    auto boxes_hit = std::map<float, entt::entity>{};

    auto entities = view<Mesh, Transform>();
    for (auto entity : entities) {
        auto& mesh = entities.get<Mesh>(entity);
        auto& transform = entities.get<Transform>(entity);

        // convert AABB from local to world space
        auto world_aabb = std::array<glm::vec3, 2>
        {
            transform.worldTransform * glm::vec4(mesh.aabb[0], 1.0),
            transform.worldTransform * glm::vec4(mesh.aabb[1], 1.0)
        };

        // check for ray hit
        const auto bounding_box = BBox3D(mesh.aabb[0] * transform.scale, mesh.aabb[1] * transform.scale);
        const auto hit_result = inRay.HitsOBB(bounding_box, transform.worldTransform);

        if (hit_result.has_value())
            boxes_hit[hit_result.value()] = entity;
    }

    for (auto& pair : boxes_hit) {
        auto& mesh = entities.get<Mesh>(pair.second);
        auto& transform = entities.get<Transform>(pair.second);

        for (auto i = 0u; i < mesh.indices.size(); i += 3) {
            const auto v0 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i]], 1.0));
            const auto v1 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 1]], 1.0));
            const auto v2 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 2]], 1.0));

            const auto hit_result = inRay.HitsTriangle(v0, v1, v2);

            if (hit_result.has_value()) {
                picked_entity = pair.second;
                return picked_entity;
            }
        }
    }

    return picked_entity;
}


entt::entity Scene::CreateSpatialEntity(const std::string& inName) {
    auto entity = create();
    auto& name = emplace<Name>(entity);
    name.name = inName;

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


glm::vec3 Scene::GetSunLightDirection() const {
    auto lightView = view<const DirectionalLight, const Transform>();
    auto lookDirection = glm::vec3(0.25f, -0.9f, 0.0f);

    if (lightView.begin() != lightView.end()) {
        const auto& lightTransform = lightView.get<const Transform>(lightView.front());
        lookDirection = static_cast<glm::quat>(lightTransform.rotation) * lookDirection;
    }
    else {
        // we rotate default light a little or else we get nan values in our view matrix
        lookDirection = static_cast<glm::quat>(glm::vec3(glm::radians(15.0f), 0, 0)) * lookDirection;
    }

    return glm::clamp(lookDirection, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });
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


void Scene::UpdateTransforms() {
    for (auto& [entity, node, transform] : view<Node, Transform>().each()) {
        transform.Compose();

        if (node.parent == entt::null)
            UpdateTransformsRecursively(entity, node.parent);
    }
}


void Scene::UpdateTransformsRecursively(entt::entity node, entt::entity parent) {
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
        UpdateTransformsRecursively(curr, node);
        curr = get<Node>(curr).nextSibling;
    }
}


Entity Scene::Clone(Entity inEntity) {
    auto copy = create();
    visit(inEntity, [&](const entt::type_info info) {
        gForEachTupleElement(Components, [&](auto component) {
            using ComponentType = decltype(component)::type;

            if (info.seq() == entt::type_seq<ComponentType>())
                clone<ComponentType>(*this, inEntity, copy);
            });
        }
    );
    return copy;
}


void Scene::LoadMaterialTextures(Assets& assets, const Slice<Entity>& materials) {
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

    auto compressed =std::vector<char>(bound);
    Timer timer;

    auto compressedSize = LZ4_compress_default(buffer.c_str(), compressed.data(), int(buffer.size()), bound);

    std::cout << "Compression time: " << Timer::sToMilliseconds(timer.GetElapsedTime()) << '\n';

    auto filestream = std::ofstream(file, std::ios::binary);
    filestream.write(compressed.data(), compressedSize);
}


void Scene::OpenFromFile(Assets& assets, const std::string& file) {
    if (!FileSystem::is_regular_file(file)) {
        std::cout << "Scene::openFromFile : filepath " << file << " does not exist.";
        return;
    }

    //Read file into buffer
    std::ifstream storage(file, std::ios::binary);
    std::string buffer;
    buffer.resize(FileSystem::file_size(file));
    storage.read(&buffer[0], buffer.size());

    auto decompressed = std::string();
    decompressed.resize(glm::min(buffer.size() * 11, size_t(INT_MAX) - 1)); // TODO: store the uncompressed size somewhere
    const auto decompressed_size = LZ4_decompress_safe(buffer.data(), decompressed.data(), int(buffer.size()), int(decompressed.size()));

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

    // temp fix for paths not being serialized
    for (auto& [path, asset] : assets)
        if (asset->GetPath() != path)
            asset->SetPath(path);

    // init material render data
    auto materials = view<Material>();
    LoadMaterialTextures(assets, Slice(materials.data(), materials.size()));

    timer.Restart();

    // init mesh render data
    for (auto& [entity, mesh] : view<Mesh>().each()) {
        mesh.CalculateAABB();
        if (m_UploadMeshCallback)
            m_UploadMeshCallback(mesh);
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
        std::clog << "Failed to bind script" << script.file <<
                     " to entity " << entt::to_integral(entity) <<
                     " from class " << script.procAddress << '\n';
    }
}

} // Raekor