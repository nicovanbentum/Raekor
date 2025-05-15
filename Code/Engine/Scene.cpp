#include "PCH.h"
#include "Scene.h"
#include "Math.h"
#include "Iter.h"
#include "Undo.h"
#include "Input.h"
#include "Timer.h"
#include "Script.h"
#include "Physics.h"
#include "Profiler.h"
#include "Threading.h"
#include "Components.h"
#include "Application.h"
#include "DebugRenderer.h"

namespace RK {

Scene::Scene(IRenderInterface* inRenderer) : m_Renderer(inRenderer), m_RootEntity(Create())
{
	EnsureExists<Name>();
	EnsureExists<Mesh>();
	EnsureExists<Light>();
	EnsureExists<Camera>();
	EnsureExists<Material>();
	EnsureExists<Skeleton>();
	EnsureExists<SoftBody>();
	EnsureExists<RigidBody>();
	EnsureExists<Transform>();
	EnsureExists<Animation>();
	EnsureExists<NativeScript>();
	EnsureExists<DirectionalLight>();
	EnsureExists<DDGISceneSettings>();
}


Entity Scene::CreateSpatialEntity(StringView inName)
{
	Entity entity = Create();
	ParentTo(entity, m_RootEntity);
	
	Add<Name>(entity).name = inName;
	Add<Transform>(entity);
	
	return entity;
}


void Scene::DestroySpatialEntity(Entity inEntity)
{
	TraverseFunction Traverse = [](void* inContext, Scene& inScene, Entity inEntity) 
	{
		inScene.Destroy(inEntity);
		inScene.Unparent(inEntity);
	};

	TraverseBreadthFirst(inEntity, Traverse, nullptr);
}


Vec3 Scene::GetSunLightDirection() const
{
	Vec3 sun_direction = Vec3(0.25f, -0.9f, 0.0f);

	if (Count<DirectionalLight>() == 1)
	{
		const Entity sunlight_entity = GetEntities<DirectionalLight>()[0];
		const Transform& sunlight_transform = Get<Transform>(sunlight_entity);
		sun_direction = sunlight_transform.GetRotationWorldSpace() * sun_direction;
	}
	else
	{
		// we rotate default light a little or else we get nan values in our view matrix
		sun_direction = static_cast<Quat>( Vec3(glm::radians(15.0f), 0, 0) ) * sun_direction;
	}

	return glm::clamp(sun_direction, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });
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
		light.direction = Vec4(transform.GetRotationWorldSpace() * Vec3(0, -1, 0), 1.0);

	for (auto [entity, light, transform] : Each<Light, Transform>())
	{
		light.direction = transform.GetRotationWorldSpace() * Vec3(0.0f, 0.0f, -1.0f);
		light.position = Vec4(transform.GetPositionWorldSpace(), 1.0f);
	}
}


void Scene::TraverseBreadthFirst(Entity inEntity, TraverseFunction inFunction, void* inContext)
{
	m_BFS.push(inEntity);

	while (!m_BFS.empty())
	{
		Entity entity = m_BFS.front();
		m_BFS.pop();

		if (entity != m_RootEntity)
			inFunction(inContext, *this, entity);

		if (auto children = m_Hierarchy.findRight(entity))
		{
			for (Entity child : *children)
				m_BFS.push(child);
		}
	}

	assert(m_BFS.empty());
}


void Scene::TraverseDepthFirst(Entity inEntity, TraverseFunction inFunction, void* inContext)
{
	m_DFS.push(inEntity);

	while (!m_DFS.empty())
	{
		Entity entity = m_DFS.top();
		m_DFS.pop();

		if (entity != m_RootEntity)
			inFunction(inContext, *this, entity);

		if (auto children = m_Hierarchy.findRight(entity))
		{
			for (Entity child : *children)
				m_DFS.push(child);
		}
	}

	assert(m_DFS.empty());
}


void Scene::UpdateCameras()
{
	for (const auto& [entity, transform, camera] : Each<Transform, Camera>())
	{
		Quat q = transform.rotation;
		camera.SetPosition(transform.GetPositionWorldSpace());
        camera.SetDirection(transform.GetRotationWorldSpace() * Camera::cForward);
	}
}


void Scene::UpdateTransforms()
{
	PROFILE_FUNCTION_CPU();

	TraverseFunction Traverse = [](void* inContext, Scene& inScene, Entity inEntity) 
	{
		Entity parent = inScene.GetParent(inEntity);
		Transform& transform = inScene.Get<Transform>(inEntity);

		Mat4x4 local_transform = transform.localTransform;

		if (const Animation* animation = inScene.GetPtr<Animation>(transform.animation))
		{
			if (animation->HasKeyFrames(transform.animationChannel))
			{
				const KeyFrames& keyframes = animation->GetKeyFrames(transform.animationChannel);

				Vec3 scale = transform.scale;
				Vec3 position = transform.position;
				Quat rotation = transform.rotation;

				if (keyframes.CanInterpolateScale())
					scale = keyframes.GetInterpolatedScale(animation->GetRunningTime());

				if (keyframes.CanInterpolatePosition())
					position = keyframes.GetInterpolatedPosition(animation->GetRunningTime());

				if (keyframes.CanInterpolateRotation())
					rotation = keyframes.GetInterpolatedRotation(animation->GetRunningTime());

				local_transform = glm::translate(Mat4x4(1.0f), position);
				local_transform = local_transform * glm::toMat4(rotation);
				local_transform = glm::scale(local_transform, scale);
			}
		}

        transform.prevWorldTransform = transform.worldTransform;

		if (parent == Entity::Null || parent == inScene.GetRootEntity())
		{
			transform.worldTransform = local_transform;
		}
		else
		{
			const Transform& parent_transform = inScene.Get<Transform>(parent);
			transform.worldTransform = parent_transform.worldTransform * local_transform;
		}
	};

	TraverseDepthFirst(m_RootEntity, Traverse, nullptr);
}


void Scene::UpdateStreaming()
{

}


void Scene::UpdateAnimations(float inDeltaTime)
{
	PROFILE_FUNCTION_CPU();

	for (auto [entity, animation] : Each<Animation>())
		animation.OnUpdate(inDeltaTime);

	uint32_t skeleton_count = Count<Skeleton>();
	if (!skeleton_count)
		return;

	Array<Job::Ptr> skeleton_job_ptrs;
	skeleton_job_ptrs.reserve(skeleton_count);

	for (auto [entity, skeleton] : Each<Skeleton>())
	{
		Job::Ptr job_ptr = g_ThreadPool.QueueJob([&]()
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

	for (const Job::Ptr& job_ptr : skeleton_job_ptrs)
		job_ptr->WaitCPU();
}


void Scene::UpdateNativeScripts(float inDeltaTime)
{
	PROFILE_FUNCTION_CPU();

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
        else if (!script.type.empty())
        {
            if (RTTI* rtti = g_RTTIFactory.GetRTTI(script.type.c_str()))
            {
                script.script = (INativeScript*)g_RTTIFactory.Construct(script.type.c_str());
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
		g_DebugRenderer.AddLineCube(mesh.bbox.GetMin(), mesh.bbox.GetMax(), transform.worldTransform);
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


Entity Scene::Clone(Entity inEntity, Entity inParent)
{
	Entity copy = Create();

	for (IComponentStorage* components : std::views::values(m_Components))
	{
		if (components->Contains(inEntity))
			components->Copy(inEntity, copy);
	}

	if (Has<Name>(copy))
	{
		Name& cloned_name = Get<Name>(copy);

		std::smatch match;
		std::regex pattern("\\((\\d+)\\)$");

		// keep track of lowest number and next lowest number,
		// if they're different we can increment the lowest to fill gaps
		// e.g. if "Light (4)" and "Light (6)" exist, we create "Light (5)"
		int highest_number = -1;
		int lowest_number = INT_MAX;
		int next_lowest_number = INT_MAX;

		// check all the other entities in the scene for similar names
		for (const auto& [entity, name] : Each<Name>())
		{
			if (name.name.starts_with(cloned_name.name))
			{
				if (std::regex_search(name.name, match, pattern))
				{
					// name is of format "Light (x)" , extract x
					int number = std::stoi(match[1].str());
					if (number < lowest_number)
						next_lowest_number = number;

					lowest_number = glm::min(number, lowest_number);
					highest_number = glm::max(number, highest_number);
				}
			}
		}

		int new_number = highest_number + 1;

		// fill gaps
		if (lowest_number < next_lowest_number)
			new_number = lowest_number + 1;

		// new indices start at 1 at least
		new_number = glm::max(new_number, 1);

		// if the current name is of format "Light (x)" update the number
		if (std::regex_search(cloned_name.name, match, pattern))
		{
			cloned_name.name = cloned_name.name.substr(0, match.position()) + std::format("({})", new_number);
		}
		else // bad format, just append the new number
		{
			cloned_name.name += std::format(" ({})", new_number);
		}
	}

	if (Has<Mesh>(copy))
	{
		Mesh& mesh = Get<Mesh>(copy);

		if (m_Renderer)
		{
			m_Renderer->UploadMeshBuffers(copy, Get<Mesh>(copy));

			if (Has<Skeleton>(copy))
				m_Renderer->UploadSkeletonBuffers(copy, Get<Skeleton>(copy), mesh);
		}
	}
	
	if (Has<RigidBody>(copy))
	{
		RigidBody& collider = Get<RigidBody>(copy);
		collider.bodyID = JPH::BodyID();
	}

	if (inParent != Entity::Null)
		ParentTo(copy, inParent);

	for (Entity child : GetChildren(inEntity))
		Clone(child, copy);

	return copy;
}



void Scene::Destroy(Entity inEntity)
{
	if (Exists(inEntity) && inEntity != m_RootEntity)
	{
		Scene::TraverseFunction Traverse = [](void* inContext, Scene& inScene, Entity inEntity)
		{
			if (inScene.m_Renderer != nullptr)
			{
				if (inScene.Has<Mesh>(inEntity))
					inScene.m_Renderer->DestroyMeshBuffers(inEntity, inScene.Get<Mesh>(inEntity));

				//if (inScene.Has<Material>(inEntity))
					//inScene.m_Renderer->DestroyMaterialTextures(inEntity, inScene.Get<Material>(inEntity));
			}

			ECStorage* storage = (ECStorage*)inContext;
			storage->Destroy(inEntity);
			inScene.Unparent(inEntity);
		};

		TraverseBreadthFirst(inEntity, Traverse, this);
	}
}


void Scene::LoadMaterialTextures(Assets& inAssets)
{
	Timer timer;

    for (const auto& [entity, material] : Each<Material>())
	{
		g_ThreadPool.QueueJob([&, entity]()
		{
			Material& material = Get<Material>(entity);
			inAssets.GetAsset<TextureAsset>(material.albedoFile);
			inAssets.GetAsset<TextureAsset>(material.normalFile);
			inAssets.GetAsset<TextureAsset>(material.emissiveFile);
			inAssets.GetAsset<TextureAsset>(material.metallicFile);
			inAssets.GetAsset<TextureAsset>(material.roughnessFile);

            //if (m_Renderer)
              //  m_Renderer->UploadMaterialTextures(entity, material, inAssets);
		}
        );
	}

    g_ThreadPool.WaitForJobs();

	std::cout << std::format("[Scene] Load textures to RAM took {:.3f} seconds.\n", timer.Restart());

    for (const auto& [entity, material] : Each<Material>())
	{
            Material& material = Get<Material>(entity);

		    if (m_Renderer)
			    m_Renderer->UploadMaterialTextures(entity, material, inAssets);
	}

    g_ThreadPool.WaitForJobs();

	std::cout << std::format("[Scene] Upload textures to GPU took {:.3f} seconds.\n", timer.GetElapsedTime());
}


void Scene::SaveToFile(const String& inFile, Assets& ioAssets, Application* inApp)
{
	BinaryWriteArchive archive(inFile);
	File& file = archive.GetFile();
	
	SceneHeader header;
	header.Version = SceneHeader::sVersion;
	header.MagicNumber = SceneHeader::sMagicNumber;

	// jump over the header
	file.seekg(sizeof(SceneHeader));

	// write Entity's
	WriteFileBinary(file, m_Entities);

	// write hierarchy
	Array<EntityHierarchy::Pair> pairs;
	pairs.reserve(m_Hierarchy.count());

	for (const EntityHierarchy::Pair& pair : m_Hierarchy)
		pairs.push_back(pair);

	WriteFileBinary(file, pairs);
	
	// write and track tables
	Array<SceneTable> tables;
	tables.reserve(m_Components.size());

	for (const auto& [hash, components] : m_Components)
	{
		SceneTable table;
		table.Hash = hash;
		table.Start = file.tellg();

		components->Write(archive);

		table.Size = uint64_t(file.tellg()) - table.Start;
		tables.push_back(table);
	}

	// update header
	header.IndexTableStart = file.tellg();
	header.IndexTableCount = tables.size();

	// write tables
	WriteFileBinary(file, tables);

	// write header (start of the file)
	file.seekg(0);
	WriteFileBinary(file, header);
}


void Scene::OpenFromFile(const String& inFilePath, Assets& ioAssets, Application* inApp)
{
	PROFILE_FUNCTION_CPU();

	// set file path properties
	m_ActiveSceneFilePath = inFilePath;
	assert(fs::is_regular_file(inFilePath));

	if (inApp)
	{
		// update Discord status
		String filename = m_ActiveSceneFilePath.filename().string();
		inApp->GetDiscordRPC().SetActivityDetails(filename.c_str());

		// clear undo system
        if (inApp->GetUndo())
		    inApp->GetUndo()->Clear();
	}

	// open archive
	BinaryReadArchive archive(inFilePath);
	File& file = archive.GetFile();

	// read header
	SceneHeader header;
	ReadFileBinary(file, header);

	// check for errors
	if (header.MagicNumber != SceneHeader::sMagicNumber)
	{
		if (inApp) 
			inApp->LogMessage(std::format("[Scene] Magic number mismatch!"));
		return;
	}
	
	if (header.Version != SceneHeader::sVersion)
	{
		if (inApp)
			inApp->LogMessage(std::format("[Scene] Format version mismatch!!"));
		return;
	}

	// clear the current scene
	Clear();
	m_Hierarchy.clear();

	// read in Entity's
	ReadFileBinary(file, m_Entities);

	Timer timer;
	
	// read in hierarchy
	Array<EntityHierarchy::Pair> pairs;
	ReadFileBinary(file, pairs);
	m_Hierarchy.insert(pairs);

	std::cout << std::format("[Scene] Load Hierarchy data took {:.3f} seconds.\n", timer.GetElapsedTime());

	// read in tables
	Array<SceneTable> tables;
	file.seekg(header.IndexTableStart);
	ReadFileBinary(file, tables);

	// read in components
	for (const SceneTable& table : tables)
	{
		file.seekg(table.Start);
		m_Components[table.Hash]->Read(archive);
	}

	std::cout << std::format("[Scene] Load ECStorage data took {:.3f} seconds.\n", timer.GetElapsedTime());

	// load material texture data to vram
	LoadMaterialTextures(ioAssets);

	for (const auto& [entity, light] : Each<DirectionalLight>())
	{
		if (light.cubeMapFile.empty())
			continue;

		if (TextureAsset::Ptr asset = ioAssets.GetAsset<TextureAsset>(light.cubeMapFile))
		{
			light.cubeMap = m_Renderer->UploadTextureFromAsset(asset);
			m_Renderer->OnResize(inApp->GetViewport());
		}
	}

	timer.Restart();

	// load mesh data to vram
	for (const auto& [entity, mesh] : Each<Mesh>())
	{
        g_ThreadPool.QueueJob([this, entity, &mesh]() 
        {
		    if (m_Renderer)
			    m_Renderer->UploadMeshBuffers(entity, mesh);

		    if (Skeleton* skeleton = GetPtr<Skeleton>(entity))
			    m_Renderer->UploadSkeletonBuffers(entity, *skeleton, mesh);
        });
	}

    g_ThreadPool.WaitForJobs();

	for (const auto& [entity, script] : Each<NativeScript>())
	{
		if (ScriptAsset::Ptr asset = ioAssets.GetAsset<ScriptAsset>(script.file))
		{
			for (const String& type_str : asset->GetRegisteredTypes())
			{
				script.types.push_back(type_str);
			}

            if (inApp) 
            {
				BindScriptToEntity(entity, script, inApp);
                continue;
            }
		}
        
        if (!script.type.empty())
        {
            if (inApp)
                BindScriptToEntity(entity, script, inApp);
        }
	}

	std::cout << std::format("[Scene] Upload mesh data to GPU took {:.3f} seconds.\n", timer.GetElapsedTime());
}


void Scene::OpenFromFileAsync(const String& inFilePath, Assets& ioAssets, Application* inApp)
{
	PROFILE_FUNCTION_CPU();

	// set file path properties
	m_ActiveSceneFilePath = inFilePath;
	assert(fs::is_regular_file(inFilePath));

	// open archive
	BinaryReadArchive archive(inFilePath);
	File& file = archive.GetFile();

	// read header
	SceneHeader header;
	ReadFileBinary(file, header);

	// check for errors
	if (header.MagicNumber != SceneHeader::sMagicNumber)
	{
		if (inApp)
			inApp->LogMessage(std::format("[Scene] Magic number mismatch!"));
		return;
	}

	if (header.Version != SceneHeader::sVersion)
	{
		if (inApp)
			inApp->LogMessage(std::format("[Scene] Format version mismatch!!"));
		return;
	}

	// clear the current scene
	Clear();
	m_Hierarchy.clear();

	// read in Entity's
	ReadFileBinary(file, m_Entities);

	Timer timer;

	// read in hierarchy
	Array<EntityHierarchy::Pair> pairs;
	ReadFileBinary(file, pairs);
	m_Hierarchy.insert(pairs);

	std::cout << std::format("[Scene] Load Hierarchy data took {:.3f} seconds.\n", timer.GetElapsedTime());

	// read in tables
	Array<SceneTable> tables;
	file.seekg(header.IndexTableStart);
	ReadFileBinary(file, tables);

	// read in components
	for (const SceneTable& table : tables)
	{
		file.seekg(table.Start);
		m_Components[table.Hash]->Read(archive);
	}

	std::cout << std::format("[Scene] Load ECStorage data took {:.3f} seconds.\n", timer.GetElapsedTime());

    for (const auto& [entity, script] : Each<NativeScript>())
    {
        if (ScriptAsset::Ptr asset = ioAssets.GetAsset<ScriptAsset>(script.file))
        {
            for (const String& type_str : asset->GetRegisteredTypes())
            {
                script.types.push_back(type_str);
            }

            if (inApp)
            {
                BindScriptToEntity(entity, script, inApp);
                continue;
            }
        }

        if (!script.type.empty())
        {
            if (inApp)
                BindScriptToEntity(entity, script, inApp);
        }
    }

	timer.Restart();

	// load mesh data to RAM and schedule VRAM uploads
	g_ThreadPool.QueueJob([this]() 
	{
		for (const auto& [entity, mesh] : Each<Mesh>())
		{
			if (m_Renderer)
				m_Renderer->UploadMeshBuffers(entity, mesh);

			if (Skeleton* skeleton = GetPtr<Skeleton>(entity))
				m_Renderer->UploadSkeletonBuffers(entity, *skeleton, mesh);
		}
	});

	Job::Barrier barrier = Job::Barrier(Count<Material>());

	// load textures data to RAM
	for (const auto& [entity, material] : Each<Material>())
	{
		barrier.AddJob(g_ThreadPool.QueueJob([&]()
		{
			ioAssets.GetAsset<TextureAsset>(material.albedoFile);
			ioAssets.GetAsset<TextureAsset>(material.normalFile);
			ioAssets.GetAsset<TextureAsset>(material.emissiveFile);
			ioAssets.GetAsset<TextureAsset>(material.metallicFile);
			ioAssets.GetAsset<TextureAsset>(material.roughnessFile);
		}));
	}

	if (m_Renderer)
	{
		g_ThreadPool.QueueJob([this, &ioAssets, barrier]() 
		{
			barrier.Wait();

			for (const auto& [entity, material] : Each<Material>())
			{
				m_Renderer->UploadMaterialTextures(entity, material, ioAssets);
			}
		});
	}
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
		inScript.script->m_DebugRenderer = &g_DebugRenderer;

		inScript.script->OnBind();

		std::clog << std::format("[Scene] Attached {} to entity {} \n", inScript.script->GetRTTI().GetTypeName(), uint32_t(inEntity));
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


bool SceneImporter::LoadFromFile(const String& inFile, Assets* inAssets)
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
		Entity new_entity = m_Scene.Create();

		Name& name = m_Scene.Add<Name>(new_entity, m_ImportedScene.Get<Name>(entity));

		ConvertMaterial(new_entity, m_ImportedScene.Get<Material>(entity));

		m_MaterialMapping[entity] = new_entity;
	}

	std::cout << "[Scene Import] Materials took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	/*
	* PARSE NODES & MESHES
	*/
	Scene::TraverseFunction Traverse = [](void* inContext, Scene& inScene, Entity inEntity) 
	{
		SceneImporter* importer = (SceneImporter*)inContext;

		if (inScene.GetParent(inEntity) == inScene.GetRootEntity())
			importer->ParseNode(inEntity, Entity::Null);
	};
	
	m_ImportedScene.TraverseDepthFirst(m_ImportedScene.GetRootEntity(), Traverse, this);

	const Entity root_entity = m_Scene.CreateSpatialEntity(Path(inFile).filename().string());

	for (Entity entity : m_CreatedNodeEntities)
	{
		if (!m_Scene.HasParent(entity) || m_Scene.GetParent(entity) == m_Scene.GetRootEntity())
			 m_Scene.ParentTo(entity, root_entity);
	}

	std::cout << "[Scene Import] Meshes & nodes took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	// Load the converted textures from disk and upload them to the GPU
	if (inAssets != nullptr)
	{
		Array<Entity> materials;
		materials.reserve(m_MaterialMapping.size());

		for (const auto& [imported_entity, output_entity] : m_MaterialMapping)
			materials.push_back(output_entity);

		m_Scene.LoadMaterialTextures(*inAssets);
	}

	return true;
}


void SceneImporter::ParseNode(Entity inEntity, Entity inParent)
{
	// Create new inEntity
	Entity new_entity = m_CreatedNodeEntities.emplace_back(m_Scene.CreateSpatialEntity());

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
		m_Scene.ParentTo(new_entity, inParent);

	// Copy over mesh
	if (m_ImportedScene.Has<Mesh>(inEntity))
		ConvertMesh(new_entity, m_ImportedScene.Get<Mesh>(inEntity));

	// Copy over skeleton
	if (m_ImportedScene.Has<Skeleton>(inEntity))
		ConvertBones(new_entity, m_ImportedScene.Get<Skeleton>(inEntity));

	// Copy over animations
	if (m_ImportedScene.Has<Animation>(inEntity))
		m_Scene.Add<Animation>(new_entity, m_Scene.Get<Animation>(inEntity));

	// recurse into children
	for (Entity child : m_ImportedScene.GetChildren(inEntity))
		ParseNode(child, new_entity);
}


void SceneImporter::ConvertMesh(Entity inEntity, const Mesh& inMesh)
{
	Mesh& mesh = m_Scene.Add<Mesh>(inEntity, inMesh);

	mesh.material = m_MaterialMapping[mesh.material];

	if (m_Renderer)
		m_Renderer->UploadMeshBuffers(inEntity, mesh);
}


void SceneImporter::ConvertBones(Entity inEntity, const Skeleton& inSkeleton)
{
	Mesh& mesh = m_Scene.Get<Mesh>(inEntity);
	Skeleton& skeleton = m_Scene.Add<Skeleton>(inEntity, inSkeleton);

	if (m_Renderer)
		m_Renderer->UploadSkeletonBuffers(inEntity, skeleton, mesh);
}


void SceneImporter::ConvertMaterial(Entity inEntity, const Material& inMaterial)
{
	Material& material = m_Scene.Add<Material>(inEntity, inMaterial);

	material.gpuAlbedoMap = 0;
	material.gpuNormalMap = 0;
	material.gpuMetallicMap = 0;
	material.gpuRoughnessMap = 0;
}

} // RK