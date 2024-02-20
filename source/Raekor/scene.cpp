#include "pch.h"
#include "scene.h"

#include "iter.h"
#include "input.h"
#include "timer.h"
#include "async.h"
#include "debug.h"
#include "rmath.h"
#include "script.h"
#include "systems.h"
#include "components.h"
#include "application.h"

namespace Raekor {

Scene::Scene(IRenderInterface* inRenderer) : m_Renderer(inRenderer) 
{
	gForEachTupleElement(Components, [&](auto component)
	{
		using ComponentType = decltype( component )::type;

		if (!m_Components.contains(gGetTypeHash<ComponentType>()))
			m_Components[gGetRTTI<ComponentType>().mHash] = new ComponentStorage<ComponentType>();
	});
}



Entity Scene::PickSpatialEntity(const Ray& inRay) const
{
	auto picked_entity = Entity::Null;
	auto boxes_hit = std::map<float, Entity> {};

	for (auto [entity, transform, mesh] : Each<Transform, Mesh>())
	{
		auto oobb = BBox3D(mesh.aabb[0], mesh.aabb[1]);
		const auto hit_result = inRay.HitsOBB(oobb, transform.worldTransform);

		if (hit_result.has_value())
			boxes_hit[hit_result.value()] = entity;
	}

	for (auto& pair : boxes_hit)
	{
		const auto& mesh = Get<Mesh>(pair.second);
		const auto& transform = Get<Transform>(pair.second);

		for (auto i = 0u; i < mesh.indices.size(); i += 3)
		{
			const auto v0 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i]], 1.0));
			const auto v1 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 1]], 1.0));
			const auto v2 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 2]], 1.0));

			Vec2 barycentrics;
			const auto hit_result = inRay.HitsTriangle(v0, v1, v2, barycentrics);

			if (hit_result.has_value())
			{
				picked_entity = pair.second;
				return picked_entity;
			}
		}
	}

	return picked_entity;
}


Entity Scene::CreateSpatialEntity(const std::string& inName)
{
	auto entity = Create();
	auto& name = Add<Name>(entity);
	name.name = inName;

	Add<Node>(entity);
	Add<Transform>(entity);
	return entity;
}


void Scene::DestroySpatialEntity(Entity inEntity)
{
	if (Has<Node>(inEntity))
	{
		auto tree = NodeSystem::sGetFlatHierarchy(*this, inEntity);

		for (auto member : tree)
		{
			NodeSystem::sRemove(*this, Get<Node>(member));
			Destroy(member);
		}

		NodeSystem::sRemove(*this, Get<Node>(inEntity));
	}

	Destroy(inEntity);
}


Vec3 Scene::GetSunLightDirection() const
{
	auto lookDirection = Vec3(0.25f, -0.9f, 0.0f);

	if (Count<DirectionalLight>() == 1)
	{
		auto dir_light_entity = GetEntities<DirectionalLight>()[0];
		auto& transform = Get<Transform>(dir_light_entity);
		lookDirection = static_cast<Quat>( transform.rotation ) * lookDirection;
	}
	else
	{
		// we rotate default light a little or else we get nan values in our view matrix
		lookDirection = static_cast<Quat>( Vec3(glm::radians(15.0f), 0, 0) ) * lookDirection;
	}

	return glm::clamp(lookDirection, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });
}


const DirectionalLight* Scene::GetSunLight() const
{
	if (Count<DirectionalLight>() == 1)
		return &GetStorage<DirectionalLight>()[0];
	else
		return nullptr;
}


void Scene::UpdateLights()
{
	for (auto [entity, light, transform] : Each<DirectionalLight, Transform>())
		light.direction = Vec4(static_cast<glm::quat>( transform.GetRotationWorldSpace() ) * glm::vec3(0, -1, 0), 1.0);

	for (auto [entity, light, transform] : Each<Light, Transform>())
	{
		light.direction = glm::toMat3(transform.GetRotationWorldSpace()) * Vec3(0.0f, 0.0f, -1.0f);
		light.position = Vec4(transform.GetPositionWorldSpace(), 1.0f);
	}
}


void Scene::UpdateTransforms()
{
	for (const auto& [entity, node, transform] : Each<Node, Transform>())
	{
		if (node.parent != Entity::Null)
			continue;

		if (node.parent == Entity::Null)
		{
			m_Nodes.push(entity);

			while (!m_Nodes.empty())
			{
				const auto& [current_node, current_transform] = Get<Node, Transform>(m_Nodes.top());
				m_Nodes.pop();

				if (current_node.parent == Entity::Null)
				{
					current_transform.worldTransform = current_transform.localTransform;
				}
				else
				{
					const auto& parent_transform = Get<Transform>(current_node.parent);
					current_transform.worldTransform = parent_transform.worldTransform * current_transform.localTransform;
				}

				auto child = current_node.firstChild;
				while (child != Entity::Null)
				{
					m_Nodes.push(child);
					child = Get<Node>(child).nextSibling;
				}
			}
		}
	}

	assert(m_Nodes.empty());
}


void Scene::UpdateAnimations(float inDeltaTime)
{
	for (auto [entity, animation] : Each<Animation>())
		animation.OnUpdate(inDeltaTime);

	auto skeleton_count = Count<Skeleton>();
	if (!skeleton_count)
		return;

	auto skeleton_job_ptrs = std::vector<Job::Ptr>();
	skeleton_job_ptrs.reserve(skeleton_count);

	for (auto [entity, skeleton] : Each<Skeleton>())
	{
		auto job_ptr = g_ThreadPool.QueueJob([&]()
		{
			if (Exists(skeleton.animation) && Has<Animation>(skeleton.animation))
			{
				skeleton.UpdateFromAnimation(Get<Animation>(skeleton.animation));
			}
			else
			{
				Animation animation;
				skeleton.UpdateFromAnimation(animation);
			}
		});

		skeleton_job_ptrs.push_back(job_ptr);
	}

	for (const auto& job_ptr : skeleton_job_ptrs)
		job_ptr->WaitCPU();
}


void Scene::UpdateNativeScripts(float inDeltaTime)
{
	for (auto [entity, script] : Each<NativeScript>())
	{
		if (script.script)
		{
			try
			{
				script.script->OnUpdate(inDeltaTime);
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << '\n';
			}
		}
	}
}


void Scene::RenderDebugShapes(Entity inEntity) const
{
	// render bounding box for meshes
	if (Has<Mesh>(inEntity))
	{
		const auto& [mesh, transform] = Get<Mesh, Transform>(inEntity);
		g_DebugRenderer.AddLineCube(mesh.aabb[0], mesh.aabb[1], transform.worldTransform);
	}
	// render debug shape for lights
	if (Has<Light>(inEntity))
	{
		const Light& light = Get<Light>(inEntity);

		if (light.type == LIGHT_TYPE_SPOT)
		{
			g_DebugRenderer.AddLineCone(light.position, light.direction, light.attributes.x, light.attributes.z);

		}
		else if (light.type == LIGHT_TYPE_POINT)
		{
			g_DebugRenderer.AddLineSphere(light.position, light.attributes.x);
		}
	}
	// render debug shape for directional light
	if (Has<DirectionalLight>(inEntity))
	{
		const DirectionalLight& light = Get<DirectionalLight>(inEntity);

		if (Has<Transform>(inEntity))
		{
			const Transform& transform = Get<Transform>(inEntity);

			static constexpr float extent = 0.1f;
			static constexpr float length = 0.5f;
			g_DebugRenderer.AddLineArrow(transform.GetPositionWorldSpace(), light.GetDirection(), 0.1f, 0.5f);
		}
	}
}


Entity Scene::Clone(Entity inEntity)
{
	Entity copy = Create();

	for (const auto& [type_hash, components] : m_Components)
	{
		if (components->Contains(inEntity))
			components->Copy(inEntity, copy);
	}

	if (Has<Name>(copy))
	{
		Name& name = Get<Name>(copy);
		name.name = name.name + " (Copy)";
	}

	if (Has<Mesh>(copy))
	{
		Mesh& mesh = Get<Mesh>(copy);
		m_Renderer->UploadMeshBuffers(copy, Get<Mesh>(copy));

		if (Has<Skeleton>(copy))
			m_Renderer->UploadSkeletonBuffers(copy, Get<Skeleton>(copy), mesh);
	}
	
	if (Has<Node>(copy))
	{
		Node& to_node = Get<Node>(copy);
		Node& from_node = Get<Node>(inEntity);

		if (from_node.parent != Entity::Null)
			NodeSystem::sAppend(*this, from_node.parent, Get<Node>(from_node.parent), copy, to_node);

		for (Entity it = from_node.firstChild; it != Entity::Null; it = Get<Node>(it).nextSibling)
			Clone(it);
	}

	if (Has<BoxCollider>(copy))
	{
		BoxCollider& collider = Get<BoxCollider>(copy);
		collider.bodyID = JPH::BodyID();
	}

	return copy;
}


void Scene::LoadMaterialTextures(Assets& assets, const Slice<Entity>& inMaterialEntities)
{
	Timer timer;

	for (Entity entity : inMaterialEntities)
	{
		g_ThreadPool.QueueJob([&]()
		{
			Material& material = Get<Material>(entity);
			assets.GetAsset<TextureAsset>(material.albedoFile);
			assets.GetAsset<TextureAsset>(material.normalFile);
			assets.GetAsset<TextureAsset>(material.emissiveFile);
			assets.GetAsset<TextureAsset>(material.metallicFile);
			assets.GetAsset<TextureAsset>(material.roughnessFile);
		});
	}

	g_ThreadPool.WaitForJobs();

	std::cout << std::format("[Scene] Load textures to RAM took {:.3f} seconds.\n", timer.Restart());

	for (Entity entity : inMaterialEntities)
	{
		Material& material = Get<Material>(entity);

		if (m_Renderer)
			m_Renderer->UploadMaterialTextures(entity, material, assets);
	}

	std::cout << std::format("[Scene] Upload textures to GPU took {:.3f} seconds.\n", timer.GetElapsedTime());
}


void Scene::SaveToFile(const std::string& inFile, Assets& ioAssets, Application* inApp)
{
	BinaryWriteArchive archive(inFile);
	WriteFileBinary(archive.GetFile(), m_Entities);

	gForEachTupleElement(Components, [&](auto inVar)
	{
		using Component = decltype( inVar )::type;
		GetComponentStorage<Component>()->Write(archive);
	});
}


void Scene::OpenFromFile(const std::string& inFilePath, Assets& ioAssets, Application* inApp)
{
	// set file path properties
	m_ActiveSceneFilePath = inFilePath;
	assert(fs::is_regular_file(inFilePath));

	// clear the current scene
	Clear();

	// read in the component data
	Timer timer;
	BinaryReadArchive archive(inFilePath);
	ReadFileBinary(archive.GetFile(), m_Entities);

	gForEachTupleElement(Components, [&](auto inVar)
	{
		using Component = decltype( inVar )::type;
		GetComponentStorage<Component>()->Read(archive);
	});

	std::cout << std::format("[Scene] Load ECStorage data took {:.3f} seconds.\n", timer.GetElapsedTime());

	// load material texture data to vram
	LoadMaterialTextures(ioAssets, GetEntities<Material>());
	timer.Restart();

	// load mesh data from vram to gpu memory
	for (const auto& [entity, mesh] : Each<Mesh>())
	{
		mesh.CalculateAABB();

		if (m_Renderer)
			m_Renderer->UploadMeshBuffers(entity, mesh);

		if (Skeleton* skeleton = GetPtr<Skeleton>(entity))
			m_Renderer->UploadSkeletonBuffers(entity, *skeleton, mesh);
	}

	for (const auto& [entity, script] : Each<NativeScript>())
	{
		if (auto asset = ioAssets.GetAsset<ScriptAsset>(script.file))
		{
			for (const auto& type : asset->GetRegisteredTypes())
			{
				script.types.push_back(type);
			}

			if (inApp)
				BindScriptToEntity(entity, script, inApp);
		}
	}

	std::cout << std::format("[Scene] Upload mesh data to GPU took {:.3f} seconds.\n", timer.GetElapsedTime());
}


void Scene::BindScriptToEntity(Entity inEntity, NativeScript& inScript, Application* inApp)
{
	if (inScript.script)
	{
        delete inScript.script;
		inScript.script = nullptr;
	}

	if (inScript.script = static_cast<INativeScript*>(g_RTTIFactory.Construct(inScript.type.c_str())))
	{
		inScript.script->m_App = inApp;
		inScript.script->m_Scene = this;
		inScript.script->m_Input = g_Input;
		inScript.script->m_Entity = inEntity;
		inScript.script->m_Camera = &inApp->GetViewport().GetCamera();
		inScript.script->m_DebugRenderer = &g_DebugRenderer;

		inScript.script->OnBind();

		std::clog << std::format("Attached {} to entity {} \n", inScript.script->GetRTTI().GetTypeName(), uint32_t(inEntity));
	}
	else
		std::clog << std::format("Failed to bind script {} to entity {} \n", inScript.file, uint32_t(inEntity)) << '\n';
}


void Scene::Optimize()
{
	for (const auto& [material_entity, material] : Each<Material>())
	{
		Mesh merged_mesh;
		merged_mesh.material = material_entity;

		for (const auto& [mesh_entity, mesh] : Each<Mesh>())
		{
			if (mesh.material != material_entity)
				continue;

			/* TODO */
		}
	}
}


bool SceneImporter::LoadFromFile(const std::string& inFile, Assets* inAssets)
{
	/*
	* LOAD GLTF FROM DISK
	*/
	Timer timer;

	// TODO: make passing in ioAssets optional
	m_ImportedScene.OpenFromFile(inFile, *inAssets);

	if (!m_ImportedScene.Count<Mesh>() || !m_ImportedScene.Count<Material>())
	{
		std::cout << std::format("[Scene] Error loading {} \n", inFile);
		return false;
	}

	std::cout << "[Scene Import] File load took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	/*
	* PARSE MATERIALS
	*/
	for (const auto& [entity, material] : m_ImportedScene.Each<Material>())
	{
		auto new_entity = m_Scene.Create();

		auto& name = m_Scene.Add<Name>(new_entity, m_ImportedScene.Get<Name>(entity));

		ConvertMaterial(new_entity, m_ImportedScene.Get<Material>(entity));

		m_MaterialMapping[entity] = new_entity;
	}

	std::cout << "[Scene Import] Materials took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	/*
	* PARSE NODES & MESHES
	*/
	for (const auto& [entity, node] : m_ImportedScene.Each<Node>())
	{
		if (node.IsRoot())
			ParseNode(entity, Entity::Null);
	}

	const auto root_entity = m_Scene.CreateSpatialEntity(Path(inFile).filename().string());
	auto& root_node = m_Scene.Get<Node>(root_entity);

	for (const auto& entity : m_CreatedNodeEntities)
	{
		auto& node = m_Scene.Get<Node>(entity);

		if (node.IsRoot())
			NodeSystem::sAppend(m_Scene, root_entity, root_node, entity, node);
	}

	std::cout << "[Scene Import] Meshes & nodes took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	// Load the converted textures from disk and upload them to the GPU
	if (inAssets != nullptr)
	{
		auto materials = std::vector<Entity>();
		materials.reserve(m_MaterialMapping.size());

		for (const auto& [imported_entity, output_entity] : m_MaterialMapping)
			materials.push_back(output_entity);

		m_Scene.LoadMaterialTextures(*inAssets, Slice(materials.data(), materials.size()));
	}

	return true;
}


void SceneImporter::ParseNode(Entity inEntity, Entity inParent)
{
	// Create new inEntity
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

	// set the new inEntity's parent
	if (inParent != Entity::Null)
		NodeSystem::sAppend(m_Scene, inParent, m_Scene.Get<Node>(inParent), new_entity, m_Scene.Get<Node>(new_entity));

	// Copy over mesh
	if (m_ImportedScene.Has<Mesh>(inEntity))
		ConvertMesh(new_entity, m_ImportedScene.Get<Mesh>(inEntity));

	// Copy over skeleton
	if (m_ImportedScene.Has<Skeleton>(inEntity))
		ConvertBones(new_entity, m_ImportedScene.Get<Skeleton>(inEntity));

	// recurse into children
	auto child = m_ImportedScene.Get<Node>(inEntity).firstChild;
	while (child != Entity::Null)
	{
		ParseNode(child, new_entity);
		child = m_ImportedScene.Get<Node>(child).nextSibling;
	}
}


void SceneImporter::ConvertMesh(Entity inEntity, const Mesh& inMesh)
{
	auto& mesh = m_Scene.Add<Mesh>(inEntity, inMesh);

	mesh.material = m_MaterialMapping[mesh.material];

	if (m_Renderer)
		m_Renderer->UploadMeshBuffers(inEntity, mesh);
}


void SceneImporter::ConvertBones(Entity inEntity, const Skeleton& inSkeleton)
{
	auto& mesh = m_Scene.Get<Mesh>(inEntity);
	auto& skeleton = m_Scene.Add<Skeleton>(inEntity, inSkeleton);

	if (m_Renderer)
		m_Renderer->UploadSkeletonBuffers(inEntity, skeleton, mesh);
}


void SceneImporter::ConvertMaterial(Entity inEntity, const Material& inMaterial)
{
	auto& material = m_Scene.Add<Material>(inEntity, inMaterial);

	material.gpuAlbedoMap = 0;
	material.gpuNormalMap = 0;
	material.gpuMetallicMap = 0;
	material.gpuRoughnessMap = 0;
}



} // Raekor