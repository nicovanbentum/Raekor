#include "PCH.h"
#include "Scene.h"
#include "Math.h"
#include "Iter.h"
#include "JSON.h"
#include "Undo.h"
#include "Member.h"
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
	if (const DirectionalLight* sun_light = GetSunLight())
		return Vec3(sun_light->GetDirection());

	return glm::normalize(Quat(Vec3(glm::radians(15.0f), 0.0f, 0.0f)) * DirectionalLight::cDefaultDirection);
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
	PROFILE_FUNCTION_CPU();

	for (auto [entity, light, transform] : Each<DirectionalLight, Transform>())
		light.direction = Vec4(glm::normalize(transform.GetRotationWorldSpace() * DirectionalLight::cDefaultDirection), 0.0f);

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
	PROFILE_FUNCTION_CPU();

	for (const auto& [entity, transform, camera] : Each<Transform, Camera>())
	{
		Quat q = transform.rotation;
		camera.SetPosition(transform.GetPositionWorldSpace());
        camera.SetDirection(transform.GetRotationWorldSpace() * Camera::cForward);
	}
}


void Scene::UpdateTriggers(const Vec3& inDefaultActivator)
{
	PROFILE_FUNCTION_CPU();

	for (const auto& [entity, trigger, transform] : Each<TriggerVolume, Transform>())
	{
		Vec3 activator_position = inDefaultActivator;

		if (trigger.activator != Entity::Null)
		{
			const Transform* activator = GetPtr<Transform>(trigger.activator);

			if (!activator)
			{
				trigger.entered = false;
				trigger.exited = trigger.inside;
				trigger.inside = false;
				continue;
			}

			activator_position = activator->GetPositionWorldSpace();
		}

		const bool inside = trigger.Contains(transform, activator_position);

		trigger.entered = inside && !trigger.inside && !( trigger.once && trigger.triggered );
		trigger.exited = !inside && trigger.inside;
		trigger.triggered |= trigger.entered;
		trigger.inside = inside;
	}
}


void Scene::ResetTriggers()
{
	for (const auto& [entity, trigger] : Each<TriggerVolume>())
		trigger.Reset();
}


void Scene::RebuildTransformOrder(const ComponentStorage<Transform>& inTransforms)
{
	m_TransformOrder.clear();
	m_TransformOrder.reserve(inTransforms.Length());

	TraverseFunction Traverse = [](void* inContext, Scene& inScene, Entity inEntity)
	{
		const ComponentStorage<Transform>& transforms = *static_cast<const ComponentStorage<Transform>*>( inContext );

		const int transform_index = transforms.GetPackedIndex(inEntity);

		if (transform_index < 0)
			return;

		const Entity parent = inScene.GetParent(inEntity);
		const int parent_index = parent != inScene.GetRootEntity() ? transforms.GetPackedIndex(parent) : -1;

		inScene.m_TransformOrder.push_back(TransformNode
		{
			.mTransform = uint32_t(transform_index),
			.mParent = parent_index < 0 ? UINT32_MAX : uint32_t(parent_index)
		});
	};

	TraverseDepthFirst(m_RootEntity, Traverse, (void*)&inTransforms);

	m_TransformOrderStorage = &inTransforms;
	m_TransformOrderVersion = inTransforms.GetVersion();
	m_TransformOrderHierarchyVersion = m_HierarchyVersion;
}


#pragma optimize("gt", on)
static void sMultiplyTransforms(const Mat4x4& inParent, const Mat4x4& inLocal, Mat4x4& outWorld)
{
	const float* parent = &inParent[0][0];
	const float* local = &inLocal[0][0];
	float* world = &outWorld[0][0];

	for (int column = 0; column < 4; column++)
	{
		for (int row = 0; row < 4; row++)
		{
			world[column * 4 + row] =
				parent[0 * 4 + row] * local[column * 4 + 0] +
				parent[1 * 4 + row] * local[column * 4 + 1] +
				parent[2 * 4 + row] * local[column * 4 + 2] +
				parent[3 * 4 + row] * local[column * 4 + 3];
		}
	}
}


void Scene::UpdateTransforms()
{
	PROFILE_FUNCTION_CPU();

	ComponentStorage<Transform>* storage = GetComponentStorage<Transform>();

	if (storage != m_TransformOrderStorage || storage->GetVersion() != m_TransformOrderVersion || m_HierarchyVersion != m_TransformOrderHierarchyVersion)
		RebuildTransformOrder(*storage);

	Transform* transforms = storage->m_Components.data();

	for (const TransformNode& node : m_TransformOrder)
	{
		Transform& transform = transforms[node.mTransform];

		Mat4x4 local_transform = transform.localTransform;

		if (const Animation* animation = transform.animation != Entity::Null ? GetPtr<Animation>(transform.animation) : nullptr)
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

		if (node.mParent == UINT32_MAX)
			transform.worldTransform = local_transform;
		else
			sMultiplyTransforms(transforms[node.mParent].worldTransform, local_transform, transform.worldTransform);
	}

	for (const auto& [entity, mesh_instanced, transform] : Each<MeshInstanced, Transform>())
	{
		for (MeshInstanced::Instance& instance : mesh_instanced.instances)
		{
			const Mat4x4 prev_world_transform = instance.worldTransform;
			sMultiplyTransforms(transform.worldTransform, instance.transform, instance.worldTransform);

			instance.prevWorldTransform = instance.worldVisible ? prev_world_transform : instance.worldTransform;
			instance.worldVisible = instance.visible;
		}
	}
}
#pragma optimize("", on)


void Scene::UpdateStreaming()
{

}


void Scene::UpdateAnimations(float inDeltaTime)
{
	PROFILE_FUNCTION_CPU();

	for (auto [entity, animation] : Each<Animation>())
		animation.OnUpdate(inDeltaTime);

	ComponentStorage<Skeleton>* skeletons = GetComponentStorage<Skeleton>();

	g_JobSystem.ParallelFor(uint32_t(skeletons->Length()), 1, [&](uint32_t inIndex)
	{
		Skeleton& skeleton = skeletons->m_Components[inIndex];

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
}


void Scene::UpdateNativeScripts(float inDeltaTime, Application* inApp)
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
				gLogError("Script", "{}", e.what());
			}
		}
        else if (!script.type.empty() && inApp)
        {
            if (g_RTTIFactory.GetRTTI(script.type.c_str()))
                BindScriptToEntity(entity, script, inApp);
        }
	}
}


void Scene::UpdatePausedNativeScripts(float inDeltaTime)
{
	PROFILE_FUNCTION_CPU();

	for (auto [entity, script] : Each<NativeScript>())
	{
		if (!script.script)
			continue;

		try
		{
			script.script->OnPausedUpdate(inDeltaTime);
		}
		catch (const std::exception& e)
		{
			gLogError("Script", "{}", e.what());
		}
	}
}


void Scene::RenderDebugShapes(Entity inEntity, float inOpacity) const
{
	// render bounding box for meshes
	if (Has<Mesh>(inEntity))
	{
		const auto& [mesh, transform] = Get<Mesh, Transform>(inEntity);
		const BBox3D bounds = mesh.GetBounds();
		g_DebugRenderer.AddLineCube(bounds.GetMin(), bounds.GetMax(), transform.worldTransform);

		Vec4 debug_mesh_color = Vec4(0.65, 1.0, 0.8, inOpacity);
#if 0

		for (int i = 0; i < mesh.indices.size(); i += 3)
		{
			Vec3 v0 = mesh.positions[mesh.indices[i + 0]];
			Vec3 v1 = mesh.positions[mesh.indices[i + 1]];
			Vec3 v2 = mesh.positions[mesh.indices[i + 2]];

			Vec4 v0_ws = transform.worldTransform * Vec4(v0, 1.0);
			Vec4 v1_ws = transform.worldTransform * Vec4(v1, 1.0);
			Vec4 v2_ws = transform.worldTransform * Vec4(v2, 1.0);

			g_DebugRenderer.AddTriangle(Vec3(v0_ws), Vec3(v1_ws), Vec3(v2_ws), debug_mesh_color);
		}
#endif
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

	if (Has<TriggerVolume, Transform>(inEntity))
	{
		const auto& [trigger, transform] = Get<TriggerVolume, Transform>(inEntity);
		g_DebugRenderer.AddLineCube(-trigger.size * 0.5f, trigger.size * 0.5f, transform.worldTransform, Vec4(1.0f, 0.6f, 0.1f, 1.0f));
	}

	if (Has<SoundSource, Transform>(inEntity))
	{
		const auto& [source, transform] = Get<SoundSource, Transform>(inEntity);

		if (source.spatial)
		{
			const Vec3 position = transform.GetPositionWorldSpace();
			g_DebugRenderer.AddLineSphere(position, source.minDistance, Vec4(0.3f, 0.8f, 1.0f, 1.0f));
			g_DebugRenderer.AddLineSphere(position, source.maxDistance, Vec4(0.3f, 0.8f, 1.0f, 0.35f));
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

	for (const auto& [hash, components] : m_Components)
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
		Skeleton* skeleton = GetPtr<Skeleton>(copy);

		if (skeleton)
		{
			skeleton->boneIndexBuffer = 0;
			skeleton->boneWeightBuffer = 0;
			skeleton->boneTransformsBuffer = 0;
			skeleton->skinnedVertexBuffer = 0;
			skeleton->skinnedBottomLevelAS = 0;
			skeleton->blasScratchBuffer = 0;
			skeleton->gpuBuffersUploaded = false;
		}

		if (m_Renderer && mesh.asset)
		{
			m_Renderer->UploadMeshBuffers(*mesh.asset);

			if (skeleton)
				m_Renderer->UploadSkeletonBuffers(copy, *skeleton, mesh);
		}
	}

	if (MeshInstanced* mesh_instanced = GetPtr<MeshInstanced>(copy))
	{
		if (m_Renderer && mesh_instanced->asset)
			m_Renderer->UploadMeshBuffers(*mesh_instanced->asset);
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
				if (Skeleton* skeleton = inScene.GetPtr<Skeleton>(inEntity))
					inScene.m_Renderer->DestroySkeletonBuffers(inEntity, *skeleton);

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
	LoadMaterialTextures(inAssets, GetEntities<Material>());
}


void Scene::LoadMaterialTextures(Assets& inAssets, Slice<const Entity> inMaterials)
{
	Timer timer;

	g_JobSystem.ParallelFor(uint32_t(inMaterials.size()), 1, [&](uint32_t inIndex)
	{
		const Material& material = Get<Material>(inMaterials[inIndex]);
		inAssets.GetAsset<TextureAsset>(material.albedoFile);
		inAssets.GetAsset<TextureAsset>(material.normalFile);
		inAssets.GetAsset<TextureAsset>(material.emissiveFile);
		inAssets.GetAsset<TextureAsset>(material.metallicFile);
		inAssets.GetAsset<TextureAsset>(material.roughnessFile);
	});

	gLogInfo("Scene", "Load textures to RAM took {:.3f} seconds.", timer.Restart());

	if (m_Renderer == nullptr)
		return;

	for (Entity entity : inMaterials)
		m_Renderer->UploadMaterialTextures(entity, Get<Material>(entity), inAssets);

	gLogInfo("Scene", "Upload textures to GPU took {:.3f} seconds.", timer.GetElapsedTime());
}


void Scene::LoadMeshAssets(Assets& ioAssets)
{
	Timer timer;

	Array<String> sources;
	uint32_t missing_source_count = 0;

	auto AddSource = [&](const String& inSource)
	{
		if (inSource.empty())
			missing_source_count++;
		else if (std::find(sources.begin(), sources.end(), inSource) == sources.end())
			sources.push_back(inSource);
	};

	for (const Mesh& mesh : GetStorage<Mesh>())
		AddSource(mesh.source);

	for (const MeshInstanced& mesh_instanced : GetStorage<MeshInstanced>())
		AddSource(mesh_instanced.source);

	if (missing_source_count > 0)
		gLogWarning("Scene", "{} meshes in {} have no source file and can't be loaded, regenerate the scene from its source file", missing_source_count, m_ActiveSceneFilePath.string());

	Array<MeshAsset::Ptr> assets(sources.size());

	g_JobSystem.ParallelFor(uint32_t(sources.size()), 1, [&](uint32_t inIndex)
	{
		assets[inIndex] = MeshAsset::Find(ioAssets, sources[inIndex]);
	});

	for (const auto& [entity, mesh] : Each<Mesh>())
	{
		const auto source = std::find(sources.begin(), sources.end(), mesh.source);
		mesh.asset = source != sources.end() ? assets[source - sources.begin()] : nullptr;

		if (mesh.asset && mesh.index >= mesh.asset->GetMeshCount())
		{
			gLogWarning("Scene", "Mesh {} of entity {} is out of range, {} only contains {} meshes", mesh.index, uint32_t(entity), mesh.source, mesh.asset->GetMeshCount());
			mesh.asset = nullptr;
		}
	}

	for (const auto& [entity, mesh_instanced] : Each<MeshInstanced>())
	{
		const auto source = std::find(sources.begin(), sources.end(), mesh_instanced.source);
		mesh_instanced.asset = source != sources.end() ? assets[source - sources.begin()] : nullptr;

		if (mesh_instanced.asset && mesh_instanced.index >= mesh_instanced.asset->GetMeshCount())
		{
			gLogWarning("Scene", "Mesh {} of entity {} is out of range, {} only contains {} meshes", mesh_instanced.index, uint32_t(entity), mesh_instanced.source, mesh_instanced.asset->GetMeshCount());
			mesh_instanced.asset = nullptr;
		}
	}

	gLogInfo("Scene", "Load {} mesh assets took {:.3f} seconds.", sources.size(), timer.GetElapsedTime());
}


void Scene::SaveToFile(const String& inFile, Assets& ioAssets, Application* inApp)
{
	File file = File(inFile, std::ios::binary | std::ios::out | std::ios::trunc);

	if (!file.is_open())
	{
		gLogError("Scene", "Failed to open {} for writing", inFile);
		return;
	}

	SceneHeader header = {};
	header.Version = SceneHeader::sVersion;
	header.MagicNumber = SceneHeader::sMagicNumber;

	WriteFileBinary(file, header);

	WriteFileBinary(file, m_Entities);

	Array<EntityHierarchy::Pair> pairs;
	pairs.reserve(m_Hierarchy.count());

	for (const EntityHierarchy::Pair& pair : m_Hierarchy)
		pairs.push_back(pair);

	WriteFileBinary(file, pairs);

	Array<SceneComponentTable> tables;
	tables.reserve(m_Components.size());

	for (const auto& [hash, storage] : m_Components)
	{
		if (storage->IsEmpty())
			continue;

		SceneComponentTable table = {};
		table.mHash = hash;
		table.mStart = uint64_t(file.tellp());

		storage->WriteTable(file);

		table.mSize = uint64_t(file.tellp()) - table.mStart;
		tables.push_back(table);
	}

	header.IndexTableStart = uint64_t(file.tellp());
	header.IndexTableCount = tables.size();

	WriteFileBinary(file, tables);

	file.seekp(0);
	WriteFileBinary(file, header);

	if (!file)
		gLogError("Scene", "Failed to write {}", inFile);
}


bool Scene::ReadSceneFile(const String& inFilePath)
{
	File file = File(inFilePath, std::ios::binary | std::ios::in);

	if (!file.is_open())
	{
		gLogError("Scene", "Failed to open {}", inFilePath);
		return false;
	}

	SceneHeader header = {};
	ReadFileBinary(file, header);

	if (header.MagicNumber != SceneHeader::sMagicNumber)
	{
		gLogError("Scene", "Magic number mismatch in {}", inFilePath);
		return false;
	}

	if (header.Version != SceneHeader::sVersion)
	{
		gLogError("Scene", "Unsupported scene format version {} in {}, regenerate it from its source file", header.Version, inFilePath);
		return false;
	}

	Clear();
	m_Hierarchy.clear();
	MarkHierarchyChanged();

	ReadFileBinary(file, m_Entities);
	RebuildEntityIndices();

	Array<EntityHierarchy::Pair> pairs;
	ReadFileBinary(file, pairs);
	m_Hierarchy.insert(pairs);

	file.seekg(header.IndexTableStart);

	Array<SceneComponentTable> tables;
	ReadFileBinary(file, tables);

	Array<Pair<IComponentStorage*, SceneComponentTable>> table_reads;
	table_reads.reserve(tables.size());

	for (const SceneComponentTable& table : tables)
	{
		if (IComponentStorage* storage = GetComponentStorage(table.mHash))
			table_reads.emplace_back(storage, table);
		else
			gLogWarning("Scene", "Skipped unknown component table with hash {:#x}", table.mHash);
	}

	g_JobSystem.ParallelFor(uint32_t(table_reads.size()), 1, [&](uint32_t inIndex)
	{
		const auto& [storage, table] = table_reads[inIndex];

		File table_file = File(inFilePath, std::ios::binary | std::ios::in);
		table_file.seekg(table.mStart);

		storage->ReadTable(table_file);
	});

	return true;
}


void Scene::BindScripts(Application* inApp)
{
	if (inApp == nullptr)
		return;

	for (const auto& [entity, script] : Each<NativeScript>())
	{
		if (script.script == nullptr && g_RTTIFactory.GetRTTI(script.type.c_str()))
			BindScriptToEntity(entity, script, inApp);
	}
}


void Scene::UnbindScripts()
{
	StoreScriptVariables();

	for (const auto& [entity, script] : Each<NativeScript>())
	{
		delete script.script;
		script.script = nullptr;
	}
}


void Scene::StoreScriptVariables()
{
	for (const auto& [entity, script] : Each<NativeScript>())
	{
		if (script.script == nullptr)
			continue;

		JSON::JSONWriter writer;
		writer.Write("{");

		bool is_first_member = true;

		for (const auto& member : script.script->GetRTTI())
		{
			if (!is_first_member)
				writer.Write(", ");

			writer.Write(std::format("\"{}\": ", member->GetCustomName()));
			member->ToJSON(writer, script.script);

			is_first_member = false;
		}

		writer.Write("}");

		script.variables = writer.GetString();
	}
}


void Scene::OpenFromFile(const String& inFilePath, Assets& ioAssets, Application* inApp)
{
	PROFILE_FUNCTION_CPU();

	m_ActiveSceneFilePath = inFilePath;

	if (inApp)
	{
		String filename = m_ActiveSceneFilePath.filename().string();
		inApp->GetDiscordRPC().SetActivityDetails(filename.c_str());

		if (inApp->GetUndo())
			inApp->GetUndo()->Clear();
	}

	if (!LoadFromFile(inFilePath, ioAssets))
		return;

	Timer timer;

	UploadMeshes();

	BindScripts(inApp);

	gLogInfo("Scene", "Upload mesh data to GPU took {:.3f} seconds.", timer.GetElapsedTime());
}


bool Scene::LoadFromFile(const String& inFilePath, Assets& ioAssets)
{
	PROFILE_FUNCTION_CPU();

	Timer timer;

	if (!ReadSceneFile(inFilePath))
		return false;

	m_ActiveSceneFilePath = inFilePath;

	gLogInfo("Scene", "Load ECStorage data took {:.3f} seconds.", timer.Restart());

	LoadMeshAssets(ioAssets);

	LoadMaterialTextures(ioAssets);

	UploadDirectionalLightCubeMaps(ioAssets);

	return true;
}


void Scene::UploadDirectionalLightCubeMaps(Assets& ioAssets)
{
	if (m_Renderer == nullptr)
		return;

	for (const auto& [entity, light] : Each<DirectionalLight>())
	{
		if (light.cubeMapFile.empty())
			continue;

		if (TextureAsset::Ptr asset = ioAssets.GetAsset<TextureAsset>(light.cubeMapFile))
			light.cubeMap = m_Renderer->UploadTextureFromAsset(asset);
	}
}


void Scene::UploadMeshes()
{
	UploadMeshes(GetEntities<Mesh>());
	UploadMeshes(GetEntities<MeshInstanced>());
}


void Scene::UploadMeshes(Slice<const Entity> inEntities)
{
	if (m_Renderer == nullptr)
		return;

	Array<MeshAsset*> assets;

	for (Entity entity : inEntities)
	{
		if (const Mesh* mesh = GetPtr<Mesh>(entity))
		{
			if (mesh->asset && std::find(assets.begin(), assets.end(), mesh->asset.get()) == assets.end())
				assets.push_back(mesh->asset.get());
		}

		if (const MeshInstanced* mesh_instanced = GetPtr<MeshInstanced>(entity))
		{
			if (mesh_instanced->asset && std::find(assets.begin(), assets.end(), mesh_instanced->asset.get()) == assets.end())
				assets.push_back(mesh_instanced->asset.get());
		}
	}

	g_JobSystem.ParallelFor(uint32_t(assets.size()), 1, [&](uint32_t inIndex)
	{
		m_Renderer->UploadMeshBuffers(*assets[inIndex]);
	});

	for (Entity entity : inEntities)
	{
		if (const Mesh* mesh = GetPtr<Mesh>(entity))
		{
			if (Skeleton* skeleton = GetPtr<Skeleton>(entity))
				m_Renderer->UploadSkeletonBuffers(entity, *skeleton, *mesh);
		}
	}
}


void Scene::ReleaseResources()
{
	for (const auto& [entity, script] : Each<NativeScript>())
	{
		delete script.script;
		script.script = nullptr;
	}

	if (m_Renderer == nullptr)
		return;

	for (const auto& [entity, mesh] : Each<Mesh>())
	{
		if (mesh.asset)
			m_Renderer->DestroyMeshBuffers(*mesh.asset);

		if (Skeleton* skeleton = GetPtr<Skeleton>(entity))
			m_Renderer->DestroySkeletonBuffers(entity, *skeleton);
	}

	for (const auto& [entity, mesh_instanced] : Each<MeshInstanced>())
	{
		if (mesh_instanced.asset)
			m_Renderer->DestroyMeshBuffers(*mesh_instanced.asset);
	}
}


void Scene::Swap(Scene& ioOther)
{
	ECStorage::Swap(ioOther);

	std::swap(m_Hierarchy, ioOther.m_Hierarchy);
	MarkHierarchyChanged();
	ioOther.MarkHierarchyChanged();
	std::swap(m_RootEntity, ioOther.m_RootEntity);
	std::swap(m_ActiveSceneFilePath, ioOther.m_ActiveSceneFilePath);
}


void Scene::CopyFrom(Scene& inOther)
{
	ECStorage::CopyFrom(inOther);

	Array<EntityHierarchy::Pair> pairs;
	pairs.reserve(inOther.m_Hierarchy.count());

	for (const EntityHierarchy::Pair& pair : inOther.m_Hierarchy)
		pairs.push_back(pair);

	m_Hierarchy.clear();
	m_Hierarchy.insert(pairs);
	MarkHierarchyChanged();

	m_RootEntity = inOther.m_RootEntity;
	m_ActiveSceneFilePath = inOther.m_ActiveSceneFilePath;
}


Array<Entity> Scene::Merge(Scene& ioOther)
{
	HashMap<Entity, Entity> mapping;

	Array<Entity> new_entities;
	new_entities.reserve(ioOther.m_Entities.size());

	for (Entity entity : ioOther.m_Entities)
	{
		if (entity == ioOther.m_RootEntity)
			continue;

		const Entity new_entity = Create();
		mapping[entity] = new_entity;
		new_entities.push_back(new_entity);
	}

	for (const auto& [hash, other_storage] : ioOther.m_Components)
	{
		IComponentStorage* storage = GetComponentStorage(hash);

		if (storage == nullptr)
		{
			gLogWarning("Scene", "Skipped merging unknown component storage with hash {:#x}", hash);
			continue;
		}

		for (Entity entity : ioOther.m_Entities)
		{
			if (entity != ioOther.m_RootEntity && other_storage->Contains(entity))
				storage->Move(*other_storage, entity, mapping[entity]);
		}
	}

	auto RemapEntity = [&mapping](Entity& ioEntity)
	{
		const auto mapped = mapping.find(ioEntity);
		ioEntity = mapped != mapping.end() ? mapped->second : Entity::Null;
	};

	for (Entity entity : new_entities)
	{
		if (Mesh* mesh = GetPtr<Mesh>(entity))
			RemapEntity(mesh->material);

		if (MeshInstanced* mesh_instanced = GetPtr<MeshInstanced>(entity))
		{
			RemapEntity(mesh_instanced->material);

			for (MeshInstanced::Instance& instance : mesh_instanced->instances)
				RemapEntity(instance.material);
		}

		if (Skeleton* skeleton = GetPtr<Skeleton>(entity))
			RemapEntity(skeleton->animation);

		if (Transform* transform = GetPtr<Transform>(entity))
			RemapEntity(transform->animation);
	}

	Array<Entity> parents = { ioOther.m_RootEntity };

	for (size_t index = 0; index < parents.size(); index++)
	{
		const Entity parent = parents[index];

		for (Entity child : ioOther.GetChildren(parent))
		{
			ParentTo(mapping[child], parent == ioOther.m_RootEntity ? m_RootEntity : mapping[parent]);
			parents.push_back(child);
		}
	}

	ioOther.Clear();
	ioOther.m_Hierarchy.clear();
	ioOther.MarkHierarchyChanged();
	ioOther.m_RootEntity = ioOther.Create();

	return new_entities;
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

		if (!inScript.variables.empty())
		{
			const RTTI& rtti = inScript.script->GetRTTI();
			JSON::JSONData json = JSON::JSONData::sFromText(inScript.variables);

			if (json.HasRootObject())
			{
				uint32_t token_index = 1;

				while (token_index < json.GetTokenCount())
				{
					const String& member_name = json.GetString(token_index++);

					if (Member* member = rtti.GetMember(member_name.c_str()))
						token_index = member->FromJSON(json, token_index, inScript.script);
					else
						token_index = json.SkipToken(token_index);
				}
			}
		}

		inScript.script->OnBind();

		gLogInfo("Scene", "Attached {} to entity {}", inScript.script->GetRTTI().GetTypeName(), uint32_t(inEntity));
	}
	else
		gLogError("Scene", "Failed to bind script {} to entity {}", inScript.file, uint32_t(inEntity));
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


void Importer::CreateMeshAsset(const String& inFile)
{
	m_MeshAsset = MeshAsset::Create(MeshAsset::GetSourcePath(inFile));
}


Mesh& Importer::AddMesh(Entity inEntity, uint32_t inIndex, Entity inMaterial)
{
	Mesh& mesh = m_Scene.Add<Mesh>(inEntity);
	mesh.SetAsset(m_MeshAsset, inIndex);
	mesh.material = inMaterial;
	return mesh;
}


void Importer::FinishMeshAsset(Assets* inAssets)
{
	if (m_MeshAsset == nullptr)
		return;

	m_MeshAsset->Save(m_MeshAsset->GetPath().string());

	if (inAssets != nullptr)
	{
		inAssets->ReleaseAsset(m_MeshAsset->GetPath().string());
		inAssets->AddAsset(m_MeshAsset->GetPath().string(), m_MeshAsset);
	}

	if (m_Renderer == nullptr)
		return;

	m_Renderer->UploadMeshBuffers(*m_MeshAsset);

	for (const auto& [entity, mesh, skeleton] : m_Scene.Each<Mesh, Skeleton>())
	{
		if (mesh.asset == m_MeshAsset)
			m_Renderer->UploadSkeletonBuffers(entity, skeleton, mesh);
	}
}


bool SceneImporter::LoadFromFile(const String& inFile, Assets* inAssets)
{
	Timer timer;

	if (inAssets == nullptr || !m_ImportedScene.LoadFromFile(inFile, *inAssets))
	{
		gLogError("Scene", "Error loading {}", inFile);
		return false;
	}

	gLogInfo("Scene Import", "File load took {:.2f} ms", Timer::sToMilliseconds(timer.Restart()));

	const Entity root_entity = m_Scene.CreateSpatialEntity(Path(inFile).filename().string());

	const Array<Entity> new_entities = m_Scene.Merge(m_ImportedScene);

	Array<Entity> new_materials;

	for (Entity entity : new_entities)
	{
		if (m_Scene.GetParent(entity) == m_Scene.GetRootEntity())
			m_Scene.ParentTo(entity, root_entity);

		if (m_Scene.Has<Material>(entity))
			new_materials.push_back(entity);
	}

	gLogInfo("Scene Import", "Merging took {:.2f} ms", Timer::sToMilliseconds(timer.Restart()));

	if (m_Renderer)
		m_Scene.UploadMeshes(new_entities);

	m_Scene.LoadMaterialTextures(*inAssets, new_materials);

	return true;
}

} // RK