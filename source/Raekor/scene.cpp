#include "pch.h"
#include "scene.h"

#include "components.h"
#include "timer.h"
#include "async.h"
#include "rmath.h"
#include "script.h"
#include "systems.h"
#include "application.h"

namespace Raekor {

Entity Scene::PickSpatialEntity(const Ray& inRay) {
    auto picked_entity = NULL_ENTITY;
    auto boxes_hit = std::map<float, Entity>{};

    for (auto [entity, transform, mesh] : Each<Transform, Mesh>()) {
        // convert AABB from local to world space
        auto world_aabb = std::array<glm::vec3, 2>
        {
            transform.worldTransform * glm::vec4(mesh.aabb[0], 1.0),
            transform.worldTransform * glm::vec4(mesh.aabb[1], 1.0)
        };

        // check for ray hit
        const auto bounding_box = BBox3D(mesh.aabb[0], mesh.aabb[1]);
        const auto hit_result = inRay.HitsOBB(bounding_box, transform.worldTransform);

        if (hit_result.has_value())
            boxes_hit[hit_result.value()] = entity;
    }

    for (auto& pair : boxes_hit) {
        const auto& mesh = Get<Mesh>(pair.second);
        const auto& transform = Get<Transform>(pair.second);

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


Entity Scene::CreateSpatialEntity(const std::string& inName) {
    auto entity = Create();
    auto& name = Add<Name>(entity);
    name.name = inName;

    Add<Node>(entity);
    Add<Transform>(entity);
    return entity;
}


void Scene::DestroySpatialEntity(Entity inEntity) {
    if (Has<Node>(inEntity)) {
        auto tree = NodeSystem::sGetFlatHierarchy(*this, inEntity);

        for (auto member : tree) {
            NodeSystem::sRemove(*this, Get<Node>(member));
            Destroy(member);
        }

        NodeSystem::sRemove(*this, Get<Node>(inEntity));
    }

    Destroy(inEntity);
}


Vec3 Scene::GetSunLightDirection() const {
    auto lookDirection = Vec3(0.25f, -0.9f, 0.0f);

    if (Count<DirectionalLight>() == 1) {
        auto dir_light_entity = GetEntities<DirectionalLight>()[0];
        auto& transform = Get<Transform>(dir_light_entity);
        lookDirection = static_cast<Quat>(transform.rotation) * lookDirection;
    }
    else {
        // we rotate default light a little or else we get nan values in our view matrix
        lookDirection = static_cast<Quat>(Vec3(glm::radians(15.0f), 0, 0)) * lookDirection;
    }

    return glm::clamp(lookDirection, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });
}


const DirectionalLight* Scene::GetSunLight() const {
    if (Count<DirectionalLight>() == 1)
        return &GetStorage<DirectionalLight>()[0];
    else
        return nullptr;
}


void Scene::UpdateLights() {
    for (auto [entity, light, transform] : Each<DirectionalLight, Transform>())
        light.direction = Vec4(static_cast<glm::quat>(transform.rotation) * glm::vec3(0, -1, 0), 1.0);

    for (auto [entity, light, transform] : Each<PointLight, Transform>())
        light.position = Vec4(transform.position, 1.0f);
}


void Scene::UpdateTransforms() {
    for (const auto& [entity, node, transform] : Each<Node, Transform>()) {
        if (node.parent != NULL_ENTITY)
            continue;

        if (node.parent == NULL_ENTITY) {
            m_Nodes.push(entity);

            while (!m_Nodes.empty()) {
                const auto& [current_node, current_transform] = Get<Node, Transform>(m_Nodes.top());
                m_Nodes.pop();

                if (current_node.parent == NULL_ENTITY) {
                    current_transform.worldTransform = current_transform.localTransform;
                } else {
                    const auto& [parent_node, parent_transform] = Get<Node, Transform>(current_node.parent);
                    current_transform.worldTransform = parent_transform.worldTransform * current_transform.localTransform;
                }

                auto child = current_node.firstChild;
                while (child != NULL_ENTITY) {
                    m_Nodes.push(child);
                    child = Get<Node>(child).nextSibling;
                }

            }
        }
    }

    assert(m_Nodes.empty());
}


void Scene::UpdateAnimations(float inDeltaTime) {
    auto skeleton_count = Count<Skeleton>();
    if (!skeleton_count)
        return;

    auto skeleton_job_ptrs = std::vector<Job::Ptr>();
    skeleton_job_ptrs.reserve(skeleton_count);

    for (auto [entity, skeleton] : Each<Skeleton>()) {
        auto job_ptr = g_ThreadPool.QueueJob([&]() {
            skeleton.UpdateFromAnimation(skeleton.animations[0], inDeltaTime);
        });

        skeleton_job_ptrs.push_back(job_ptr);
    }

    for (const auto& job_ptr : skeleton_job_ptrs)
        job_ptr->WaitCPU();
}


void Scene::UpdateNativeScripts(float inDeltaTime) {
    for (auto [entity, script] : Each<NativeScript>()) {
        if (script.script) {
            try {
                script.script->OnUpdate(inDeltaTime);
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
            }
        }
    }
}



Entity Scene::Clone(Entity inEntity) {
    auto copy = Create();
    Visit(inEntity, [&](uint32_t inTypeHash) {
        gForEachTupleElement(Components, [&](auto component) {
            using ComponentType = decltype(component)::type;

            if (gGetRTTI<ComponentType>().GetHash() == inTypeHash)
                clone<ComponentType>(*this, inEntity, copy);
            });
        }
    );

    if (Has<Mesh>(copy))
        m_Renderer->UploadMeshBuffers(Get<Mesh>(copy));

    return copy;
}


void Scene::LoadMaterialTextures(Assets& assets, const Slice<Entity>& materials) {
    Timer timer;

    for (const auto& entity : materials) {
        g_ThreadPool.QueueJob([&]() {
            auto& material = Get<Material>(entity);
            assets.GetAsset<TextureAsset>(material.albedoFile);
            assets.GetAsset<TextureAsset>(material.normalFile);
            assets.GetAsset<TextureAsset>(material.metalroughFile);
        });
    }

    g_ThreadPool.WaitForJobs();

    std::cout << std::format("[Scene] Load textures to RAM took {:.3f} seconds.\n", timer.Restart());

    for (auto entity : materials) {
        auto& material = Get<Material>(entity);

        if (m_Renderer)
            m_Renderer->UploadMaterialTextures(material, assets);
    }

    std::cout << std::format("[Scene] Upload textures to GPU took {:.3f} seconds.\n", timer.GetElapsedTime());
}


void Scene::SaveToFile(Assets& assets, const std::string& file) {
    auto archive = BinaryWriteArchive(file);
    WriteFileBinary(archive.GetFile(), m_Entities);

    gForEachTupleElement(Components, [&](auto component) {
        using ComponentType = decltype(component)::type;
        const auto type_hash = gGetTypeHash<ComponentType>();

        GetSparseSet<ComponentType>()->Write(archive);
    });
}


void Scene::OpenFromFile(Assets& assets, const std::string& file) {
    if (!fs::is_regular_file(file)) {
        std::cout << "Scene::openFromFile : filepath " << file << " does not exist.";
        return;
    }

    Clear();

    Timer timer;

    auto archive = BinaryReadArchive(file);
    ReadFileBinary(archive.GetFile(), m_Entities);

    gForEachTupleElement(Components, [&](auto inVar) {
        using Component = decltype(inVar)::type;
        GetSparseSet<Component>()->Read(archive);
    });

    std::cout << std::format("[Scene] Load ECS data took {:.3f} seconds.\n", timer.GetElapsedTime());

    // init material render data
    LoadMaterialTextures(assets, GetEntities<Material>());

    timer.Restart();

    // init mesh render data
    for (const auto& [entity, mesh] : Each<Mesh>()) {
        mesh.CalculateAABB();
     
        if (m_Renderer)
            m_Renderer->UploadMeshBuffers(mesh);
    }

    std::cout << std::format("[Scene] Upload mesh data to GPU took {:.3f} seconds.\n", timer.GetElapsedTime());
}


void Scene::BindScriptToEntity(Entity entity, NativeScript& script) {
    auto address = GetProcAddress(script.asset->GetModule(), script.procAddress.c_str());
    if (address) {
        auto function = reinterpret_cast<INativeScript::FactoryType>(address);
        script.script = function();
        script.script->Bind(entity, this);
    }
    else {
        std::clog << "Failed to bind script" << script.file <<
                     " to entity " << uint32_t(entity) <<
                     " from class " << script.procAddress << '\n';
    }
}


void Scene::Optimize() {
    for (const auto& [material_entity, material] : Each<Material>()) {
        Mesh merged_mesh;
        merged_mesh.material = material_entity;

        for (const auto& [mesh_entity, mesh] : Each<Mesh>()) {
            if (mesh.material != material_entity)
                continue;


        }

    }
}


bool SceneImporter::LoadFromFile(const std::string& inFile, Assets* inAssets) {
    /*
    * LOAD GLTF FROM DISK
    */
    Timer timer;

    // TODO: make passing in assets optional
    m_ImportedScene.OpenFromFile(*inAssets, inFile);

    if (!m_ImportedScene.Count<Mesh>() || !m_ImportedScene.Count<Material>()) {
        std::cout << std::format("[Scene] Error loading {} \n", inFile);
        return false;
    }

    std::cout << "[Scene Import] File load took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

    /*
    * PARSE MATERIALS
    */
    for (const auto& [entity, material] : m_ImportedScene.Each<Material>()) {
        auto new_entity = m_Scene.Create();
        
        auto& name = m_Scene.Add<Name>(new_entity, m_ImportedScene.Get<Name>(entity));
        
        ConvertMaterial(new_entity, m_ImportedScene.Get<Material>(entity));

        m_MaterialMapping[entity] = new_entity;
    }

    std::cout << "[Scene Import] Materials took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

    /*
    * PARSE NODES & MESHES
    */
    for (const auto& [entity, node] : m_ImportedScene.Each<Node>()) {
            if (node.IsRoot())
                ParseNode(entity, NULL_ENTITY);
    }

    const auto root_entity = m_Scene.CreateSpatialEntity(Path(inFile).filename().string());
    auto& root_node = m_Scene.Get<Node>(root_entity);

    for (const auto& entity : m_CreatedNodeEntities) {
        auto& node = m_Scene.Get<Node>(entity);

        if (node.IsRoot())
            NodeSystem::sAppend(m_Scene, root_entity, root_node, entity, node);
    }

    std::cout << "[Scene Import] Meshes & nodes took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

    // Load the converted textures from disk and upload them to the GPU
    if (inAssets != nullptr) {
        auto materials = std::vector<Entity>();
        materials.reserve(m_MaterialMapping.size());

        for (const auto& [imported_entity, output_entity] : m_MaterialMapping)
            materials.push_back(output_entity);

        m_Scene.LoadMaterialTextures(*inAssets, Slice(materials.data(), materials.size()));
    }

    return true;
}


void SceneImporter::ParseNode(Entity inEntity, Entity inParent) {
    // Create new entity
    auto new_entity = m_CreatedNodeEntities.emplace_back(m_Scene.CreateSpatialEntity());

    // Copy over transform
    if (m_ImportedScene.Has<Transform>(inEntity))
        m_Scene.Add<Transform>(new_entity, m_ImportedScene.Get<Transform>(inEntity));
    else
        m_Scene.Add<Transform>(new_entity);

    // Copy over name
    if (m_ImportedScene.Has<Name>(inEntity))
        m_Scene.Add<Name>(new_entity, m_ImportedScene.Get<Name>(inEntity));
    else 
        m_Scene.Add<Name>(new_entity);

    // set the new entity's parent
    if (inParent != NULL_ENTITY)
        NodeSystem::sAppend(m_Scene, inParent, m_Scene.Get<Node>(inParent), new_entity, m_Scene.Get<Node>(new_entity));

    // Copy over mesh
    if (m_ImportedScene.Has<Mesh>(inEntity))
        ConvertMesh(new_entity, m_ImportedScene.Get<Mesh>(inEntity));

    // Copy over skeleton
    if (m_ImportedScene.Has<Skeleton>(inEntity))
        ConvertBones(new_entity, m_ImportedScene.Get<Skeleton>(inEntity));

    // recurse into children
    auto child = m_ImportedScene.Get<Node>(inEntity).firstChild;
    while (child != NULL_ENTITY) {
        ParseNode(child, inParent);
        child = m_ImportedScene.Get<Node>(child).nextSibling;
    }
}


void SceneImporter::ConvertMesh(Entity inEntity, const Mesh& inMesh) {
    auto& mesh = m_Scene.Add<Mesh>(inEntity, inMesh);

    mesh.material = m_MaterialMapping[mesh.material];

    if (m_Renderer)
        m_Renderer->UploadMeshBuffers(mesh);
}


void SceneImporter::ConvertBones(Entity inEntity, const Skeleton& inSkeleton) {
    auto& mesh = m_Scene.Get<Mesh>(inEntity);
    auto& skeleton = m_Scene.Add<Skeleton>(inEntity, inSkeleton);

    if (m_Renderer)
        m_Renderer->UploadSkeletonBuffers(skeleton, mesh);
}


void SceneImporter::ConvertMaterial(Entity inEntity, const Material& inMaterial) {
    auto& material = m_Scene.Add<Material>(inEntity, inMaterial);

    material.gpuAlbedoMap = 0;
    material.gpuNormalMap = 0;
    material.gpuMetallicRoughnessMap = 0;
}



} // Raekor