#include "PCH.h"
#include "ECS.h"
#include "OS.h"
#include "JSON.h"
#include "GLTF.h"
#include "Scene.h"
#include "Script.h"
#include "Assets.h"
#include "Timer.h"
#include "Member.h"
#include "Archive.h"
#include "Threading.h"
#include "Components.h"

#include "miniaudio/miniaudio.h"

namespace RK {

static int sFailures = 0;

static void sCheck(bool inCondition, const char* inExpression, std::source_location inLocation = std::source_location::current())
{
	if (inCondition)
		return;

	sFailures++;
	gLogError("Tests", "{}({}): check failed: {}", inLocation.file_name(), inLocation.line(), inExpression);
}

#define CHECK(expression) sCheck(bool(expression), #expression)


struct TestComponentV1
{
	RTTI_DECLARE_TYPE(TestComponentV1);

	int mRemoved = 0;
	float mKept = 0.0f;
	String mName;
	float mChangedType = 0.0f;
};

RTTI_DEFINE_TYPE(TestComponentV1)
{
	RTTI_DEFINE_MEMBER(TestComponentV1, SERIALIZE_ALL, "Removed", mRemoved);
	RTTI_DEFINE_MEMBER(TestComponentV1, SERIALIZE_ALL, "Kept", mKept);
	RTTI_DEFINE_MEMBER(TestComponentV1, SERIALIZE_ALL, "Name", mName);
	RTTI_DEFINE_MEMBER(TestComponentV1, SERIALIZE_ALL, "Changed Type", mChangedType);
}


struct TestComponentV2
{
	RTTI_DECLARE_TYPE(TestComponentV2);

	String mName = "default";
	float mKept = 0.0f;
	int mAdded = 42;
	Array<int> mChangedType = { 7 };
};

RTTI_DEFINE_TYPE(TestComponentV2)
{
	RTTI_DEFINE_MEMBER(TestComponentV2, SERIALIZE_ALL, "Name", mName);
	RTTI_DEFINE_MEMBER(TestComponentV2, SERIALIZE_ALL, "Kept", mKept);
	RTTI_DEFINE_MEMBER(TestComponentV2, SERIALIZE_ALL, "Added", mAdded);
	RTTI_DEFINE_MEMBER(TestComponentV2, SERIALIZE_ALL, "Changed Type", mChangedType);
}


struct TestJSON
{
	RTTI_DECLARE_TYPE(TestJSON);

	bool mFalse = true;
	bool mTrue = false;
	float mSmall = 0.0f;
	Vec3 mVector = Vec3(0.0f);
	String mQuoted;
	int mSkipped = 5;
};

RTTI_DEFINE_TYPE(TestJSON)
{
	RTTI_DEFINE_MEMBER(TestJSON, SERIALIZE_ALL, "False", mFalse);
	RTTI_DEFINE_MEMBER(TestJSON, SERIALIZE_ALL, "True", mTrue);
	RTTI_DEFINE_MEMBER(TestJSON, SERIALIZE_ALL, "Small", mSmall);
	RTTI_DEFINE_MEMBER(TestJSON, SERIALIZE_ALL, "Vector", mVector);
	RTTI_DEFINE_MEMBER(TestJSON, SERIALIZE_ALL, "Quoted", mQuoted);
	RTTI_DEFINE_MEMBER(TestJSON, SERIALIZE_BINARY, "Skipped", mSkipped);
}


struct TestLegacyComponent
{
	RTTI_DECLARE_TYPE(TestLegacyComponent);

	int mFirst = 0;
	String mName;
	int mAdded = 9;
	float mLast = 0.0f;
};

RTTI_DEFINE_TYPE(TestLegacyComponent)
{
	RTTI_DEFINE_MEMBER(TestLegacyComponent, SERIALIZE_ALL, "First", mFirst);
	RTTI_DEFINE_MEMBER(TestLegacyComponent, SERIALIZE_ALL, "Name", mName);
	RTTI_DEFINE_MEMBER(TestLegacyComponent, ESerializeType(SERIALIZE_ALL | SERIALIZE_NO_LEGACY_BINARY), "Added", mAdded);
	RTTI_DEFINE_MEMBER(TestLegacyComponent, SERIALIZE_ALL, "Last", mLast);
}


static void sTestLegacyMemberSkipping(const Path& inDirectory)
{
	const Path legacy_path = inDirectory / "legacy_component.bin";

	{
		File file = File(legacy_path, std::ios::binary | std::ios::out | std::ios::trunc);
		WriteFileBinary(file, String(RTTI_OF<TestLegacyComponent>().GetTypeName()));
		WriteFileBinary(file, 17);
		WriteFileBinary(file, String("legacy"));
		WriteFileBinary(file, 2.5f);
	}

	TestLegacyComponent component;

	{
		BinaryReadArchive archive(legacy_path);
		archive >> component;
	}

	CHECK(component.mFirst == 17);
	CHECK(component.mName == "legacy");
	CHECK(component.mAdded == 9);
	CHECK(component.mLast == 2.5f);
}


static void sTestComponentTableVersioning(const Path& inDirectory)
{
	const Path table_path = inDirectory / "component_table.bin";

	{
		ComponentStorage<TestComponentV1> storage;

		for (uint32_t index = 0; index < 100; index++)
		{
			storage.Insert(Entity(index * 3), TestComponentV1
			{
				.mRemoved = int(index),
				.mKept = float(index) * 0.5f,
				.mName = std::format("Entity {}", index),
				.mChangedType = 1.0f
			});
		}

		File file = File(table_path, std::ios::binary | std::ios::out | std::ios::trunc);
		storage.WriteTable(file);
	}

	ComponentStorage<TestComponentV2> storage;

	{
		File file = File(table_path, std::ios::binary | std::ios::in);
		CHECK(!storage.ReadTable(file));
	}

	CHECK(storage.Length() == 100);

	for (uint32_t index = 0; index < 100; index++)
	{
		CHECK(storage.Contains(Entity(index * 3)));

		const TestComponentV2& component = storage.Get(Entity(index * 3));
		CHECK(component.mKept == float(index) * 0.5f);
		CHECK(component.mName == std::format("Entity {}", index));
		CHECK(component.mAdded == 42);
		CHECK(component.mChangedType.size() == 1 && component.mChangedType[0] == 7);
	}
}


static void sTestSceneRoundTrip(const Path& inDirectory)
{
	const String scene_path = ( inDirectory / "round_trip.scene" ).string();

	Assets assets;
	Scene scene = Scene(nullptr);

	Array<Entity> entities;

	for (uint32_t index = 0; index < 32; index++)
	{
		const Entity entity = entities.emplace_back(scene.CreateSpatialEntity(std::format("Node {}", index)));

		Transform& transform = scene.Get<Transform>(entity);
		transform.position = Vec3(float(index), 2.0f, -float(index));
		transform.Compose();

		const Entity material_entity = scene.CreateSpatialEntity(std::format("Material {}", index));
		Material& material = scene.Add<Material>(material_entity);
		material.albedo = Vec4(0.1f * index, 0.2f, 0.3f, 1.0f);
		material.roughness = 0.123456789f;
		material.blendMode = EMaterialBlendMode(index % MATERIAL_BLEND_MODE_COUNT);
		material.alphaCutoff = 0.01f * index;
		material.samplerFilter = EMaterialSamplerFilter(index % MATERIAL_SAMPLER_FILTER_COUNT);
		material.samplerAddressMode = EMaterialSamplerAddressMode(index % MATERIAL_SAMPLER_ADDRESS_MODE_COUNT);

		constexpr StaticArray builtin_meshes = { MeshAsset::cBuiltinCube, MeshAsset::cBuiltinSphere, MeshAsset::cBuiltinPlane };

		Mesh& mesh = scene.Add<Mesh>(entity);
		mesh.SetAsset(MeshAsset::Find(assets, builtin_meshes[index % builtin_meshes.size()]), 0);
		mesh.material = material_entity;

		if (index % 4 == 0)
		{
			Light& light = scene.Add<Light>(entity);
			light.type = LIGHT_TYPE_POINT;
			light.color = Vec4(1.0f, 0.5f, 0.25f, float(index));
		}

		if (index % 5 == 0)
		{
			SoundSource& source = scene.Add<SoundSource>(entity);
			source.file = std::format("Assets/Sound{}.ogg", index);
			source.category = ESoundCategory(index % SOUND_CATEGORY_COUNT);
			source.volume = 0.25f * index;
			source.loop = index % 2 == 0;
			source.playOnTriggerEnter = true;
			source.maxDistance = 10.0f + index;
			source.voice = 42;

			TriggerVolume& trigger = scene.Add<TriggerVolume>(entity);
			trigger.size = Vec3(1.0f, 2.0f, float(index));
			trigger.activator = material_entity;
			trigger.once = true;
		}
	}

	scene.SaveToFile(scene_path, assets);

	Scene loaded = Scene(nullptr);
	loaded.OpenFromFile(scene_path, assets);

	CHECK(loaded.GetEntities().size() == scene.GetEntities().size());
	CHECK(loaded.Count<Mesh>() == scene.Count<Mesh>());
	CHECK(loaded.Count<Light>() == scene.Count<Light>());
	CHECK(loaded.Count<Material>() == scene.Count<Material>());

	for (Entity entity : entities)
	{
		CHECK(loaded.Has<Name>(entity) && loaded.Get<Name>(entity).name == scene.Get<Name>(entity).name);
		CHECK(loaded.Has<Transform>(entity) && loaded.Get<Transform>(entity).position == scene.Get<Transform>(entity).position);
		CHECK(loaded.GetParent(entity) == scene.GetParent(entity));

		const Mesh& original_mesh = scene.Get<Mesh>(entity);
		const Mesh& loaded_mesh = loaded.Get<Mesh>(entity);

		CHECK(loaded_mesh.source == original_mesh.source);
		CHECK(loaded_mesh.index == original_mesh.index);
		CHECK(loaded_mesh.GetGeometry() && loaded_mesh.GetGeometry() == original_mesh.GetGeometry());
		CHECK(loaded_mesh.material == original_mesh.material);

		const Material& original_material = scene.Get<Material>(original_mesh.material);
		const Material& loaded_material = loaded.Get<Material>(loaded_mesh.material);
		CHECK(loaded_material.albedo == original_material.albedo);
		CHECK(loaded_material.roughness == original_material.roughness);
		CHECK(loaded_material.blendMode == original_material.blendMode);
		CHECK(loaded_material.alphaCutoff == original_material.alphaCutoff);
		CHECK(loaded_material.samplerFilter == original_material.samplerFilter);
		CHECK(loaded_material.samplerAddressMode == original_material.samplerAddressMode);

		CHECK(loaded.Has<Light>(entity) == scene.Has<Light>(entity));

		if (scene.Has<Light>(entity))
			CHECK(loaded.Get<Light>(entity).color == scene.Get<Light>(entity).color);

		CHECK(loaded.Has<SoundSource>(entity) == scene.Has<SoundSource>(entity));
		CHECK(loaded.Has<TriggerVolume>(entity) == scene.Has<TriggerVolume>(entity));

		if (scene.Has<SoundSource>(entity))
		{
			const SoundSource& original_source = scene.Get<SoundSource>(entity);
			const SoundSource& loaded_source = loaded.Get<SoundSource>(entity);

			CHECK(loaded_source.file == original_source.file);
			CHECK(loaded_source.category == original_source.category);
			CHECK(loaded_source.volume == original_source.volume);
			CHECK(loaded_source.loop == original_source.loop);
			CHECK(loaded_source.playOnTriggerEnter == original_source.playOnTriggerEnter);
			CHECK(loaded_source.maxDistance == original_source.maxDistance);
			CHECK(loaded_source.voice == 0);

			const TriggerVolume& original_trigger = scene.Get<TriggerVolume>(entity);
			const TriggerVolume& loaded_trigger = loaded.Get<TriggerVolume>(entity);

			CHECK(loaded_trigger.size == original_trigger.size);
			CHECK(loaded_trigger.activator == original_trigger.activator);
			CHECK(loaded_trigger.once == original_trigger.once);
		}
	}
}


static void sTestTriggerVolumes()
{
	Scene scene = Scene(nullptr);

	const Entity trigger_entity = scene.CreateSpatialEntity("Trigger");
	const Entity activator_entity = scene.CreateSpatialEntity("Activator");

	Transform& transform = scene.Get<Transform>(trigger_entity);
	transform.position = Vec3(10.0f, 0.0f, 0.0f);
	transform.rotation = glm::angleAxis(glm::radians(90.0f), Vec3(0.0f, 1.0f, 0.0f));
	transform.scale = Vec3(2.0f);
	transform.Compose();

	TriggerVolume& trigger = scene.Add<TriggerVolume>(trigger_entity);
	trigger.size = Vec3(1.0f, 1.0f, 4.0f);

	scene.UpdateTransforms();

	CHECK(trigger.Contains(transform, Vec3(13.5f, 0.0f, 0.0f)));
	CHECK(!trigger.Contains(transform, Vec3(14.5f, 0.0f, 0.0f)));
	CHECK(!trigger.Contains(transform, Vec3(10.0f, 0.0f, 1.5f)));

	scene.UpdateTriggers(Vec3(0.0f));
	CHECK(!trigger.inside && !trigger.entered && !trigger.exited);

	scene.UpdateTriggers(Vec3(10.0f, 0.0f, 0.0f));
	CHECK(trigger.inside && trigger.entered && !trigger.exited);

	scene.UpdateTriggers(Vec3(11.0f, 0.0f, 0.0f));
	CHECK(trigger.inside && !trigger.entered && !trigger.exited);

	scene.UpdateTriggers(Vec3(0.0f));
	CHECK(!trigger.inside && !trigger.entered && trigger.exited);

	trigger.once = true;

	scene.UpdateTriggers(Vec3(10.0f, 0.0f, 0.0f));
	CHECK(trigger.inside && !trigger.entered);

	trigger.Reset();
	trigger.activator = activator_entity;

	Transform& activator = scene.Get<Transform>(activator_entity);
	activator.position = Vec3(10.0f, 0.0f, 0.0f);
	activator.Compose();
	scene.UpdateTransforms();

	scene.UpdateTriggers(Vec3(0.0f));
	CHECK(trigger.inside && trigger.entered);

	scene.Destroy(activator_entity);
	scene.UpdateTriggers(Vec3(10.0f, 0.0f, 0.0f));
	CHECK(!trigger.inside && trigger.exited);
}


static void sTestLoopingSoundLengths()
{
	const StaticArray<Pair<const char*, uint64_t>, 7> loops =
	{
		Pair<const char*, uint64_t> { "Assets/RailRush/Audio/Music.ogg", 30 * 44100 },
		Pair<const char*, uint64_t> { "Assets/RailRush/Audio/Ambience.ogg", 24 * 44100 },
		Pair<const char*, uint64_t> { "Assets/RailRush/Audio/TrainRumble.ogg", 4 * 52920 },
		Pair<const char*, uint64_t> { "Assets/RailRush/Audio/SkateboardLoop.ogg", 8 * 16317 },
		Pair<const char*, uint64_t> { "Assets/DuskSwarm/Audio/Music.ogg", 40 * 44100 },
		Pair<const char*, uint64_t> { "Assets/DuskSwarm/Audio/BossMusic.ogg", 32 * 44100 },
		Pair<const char*, uint64_t> { "Assets/DuskSwarm/Audio/Ambience.ogg", 30 * 44100 },
	};

	for (const auto& [file, expected_frames] : loops)
	{
		ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
		ma_decoder decoder;

		if (ma_decoder_init_file(file, &config, &decoder) != MA_SUCCESS)
		{
			CHECK(!"failed to open looping sound");
			continue;
		}

		Array<float> frames(4096 * decoder.outputChannels);
		uint64_t frame_count = 0;

		for (;;)
		{
			ma_uint64 frames_read = 0;
			ma_decoder_read_pcm_frames(&decoder, frames.data(), 4096, &frames_read);
			frame_count += frames_read;

			if (frames_read < 4096)
				break;
		}

		ma_decoder_uninit(&decoder);

		if (frame_count != expected_frames)
			gLogError("Tests", "{} decoded to {} frames, expected {}", file, frame_count, expected_frames);

		CHECK(frame_count == expected_frames);
	}
}


class TestScript : public INativeScript
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(TestScript);

	void OnUpdate(float inDeltaTime) override {}
	void OnEvent(const SDL_Event& inEvent) override {}

	int mCount = 1;
	float mSpeed = 2.0f;
	bool mEnabled = false;
	Entity mTarget = Entity::Null;
};

RTTI_DEFINE_TYPE(TestScript)
{
	RTTI_DEFINE_TYPE_INHERITANCE(TestScript, INativeScript);

	RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, "Count", mCount);
	RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, "Speed", mSpeed);
	RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, "Enabled", mEnabled);
	RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, "Target", mTarget);
}


static void sTestScriptVariables(const Path& inDirectory)
{
	const String scene_path = ( inDirectory / "scripts.scene" ).string();

	Assets assets;
	Scene scene = Scene(nullptr);

	const Entity target = scene.CreateSpatialEntity("Target");
	const Entity entity = scene.CreateSpatialEntity("Scripted");

	NativeScript& script = scene.Add<NativeScript>(entity);
	script.type = RTTI_OF<TestScript>().GetTypeName();

	scene.BindScriptToEntity(entity, script, nullptr);
	CHECK(script.script != nullptr);

	if (script.script == nullptr)
		return;

	TestScript* instance = static_cast<TestScript*>( script.script );
	instance->mCount = 42;
	instance->mSpeed = 0.125f;
	instance->mEnabled = true;
	instance->mTarget = target;

	scene.UnbindScripts();
	CHECK(script.script == nullptr);
	CHECK(!script.variables.empty());

	scene.BindScriptToEntity(entity, script, nullptr);
	instance = static_cast<TestScript*>( script.script );

	CHECK(instance && instance->mCount == 42 && instance->mSpeed == 0.125f && instance->mEnabled && instance->mTarget == target);

	scene.StoreScriptVariables();
	scene.SaveToFile(scene_path, assets);
	scene.UnbindScripts();

	Scene loaded = Scene(nullptr);
	loaded.OpenFromFile(scene_path, assets);

	NativeScript& loaded_script = loaded.Get<NativeScript>(entity);
	CHECK(loaded_script.type == script.type);

	loaded.BindScriptToEntity(entity, loaded_script, nullptr);
	const TestScript* loaded_instance = static_cast<const TestScript*>( loaded_script.script );

	CHECK(loaded_instance && loaded_instance->mCount == 42 && loaded_instance->mSpeed == 0.125f && loaded_instance->mEnabled && loaded_instance->mTarget == target);

	loaded.UnbindScripts();

	loaded_script.variables = "{\"Removed\": [1, 2, 3], \"Count\": 7}";
	loaded.BindScriptToEntity(entity, loaded_script, nullptr);

	const TestScript* migrated_instance = static_cast<const TestScript*>( loaded_script.script );
	CHECK(migrated_instance && migrated_instance->mCount == 7 && migrated_instance->mSpeed == 2.0f);

	loaded.UnbindScripts();
}


static void sTestGltfBlendModes(const Path& inGltfFile)
{
	if (!fs::exists(inGltfFile))
	{
		gLogWarning("Tests", "Skipped the glTF blend mode test, {} does not exist", inGltfFile.string());
		return;
	}

	Scene scene = Scene(nullptr);

	GltfImporter importer = GltfImporter(scene, nullptr);
	CHECK(importer.LoadFromFile(inGltfFile.string(), nullptr));

	auto FindMaterial = [&](StringView inName) -> const Material*
	{
		for (const auto& [entity, material] : scene.Each<Material>())
		{
			if (const Name* name = scene.GetPtr<Name>(entity); name && name->name == inName)
				return &material;
		}

		return nullptr;
	};

	const Material* opaque = FindMaterial("MatOpaque");
	const Material* blend = FindMaterial("MatBlend");
	const Material* cutoff_25 = FindMaterial("MatCutoff25");
	const Material* cutoff_75 = FindMaterial("MatCutoff75");
	const Material* cutoff_default = FindMaterial("MatCutoffDefault");

	CHECK(opaque && opaque->blendMode == MATERIAL_BLEND_MODE_OPAQUE);
	CHECK(blend && blend->blendMode == MATERIAL_BLEND_MODE_BLENDED);
	CHECK(cutoff_25 && cutoff_25->blendMode == MATERIAL_BLEND_MODE_MASKED && cutoff_25->alphaCutoff == 0.25f);
	CHECK(cutoff_75 && cutoff_75->blendMode == MATERIAL_BLEND_MODE_MASKED && cutoff_75->alphaCutoff == 0.75f);
	CHECK(cutoff_default && cutoff_default->blendMode == MATERIAL_BLEND_MODE_MASKED && cutoff_default->alphaCutoff == 0.5f);
}


static void sTestMeshPack(const Path& inDirectory, const Path& inGltfFile)
{
	MeshAsset::Ptr pack = MeshAsset::Create("Assets/Tests/Pack.gltf");
	MeshGeometry::CreateCube(pack->GetMeshes().emplace_back(), 2.0f);
	MeshGeometry::CreateSphere(pack->GetMeshes().emplace_back(), 0.5f, 8, 8);
	pack->GetMeshes()[0].name = "Cube";
	pack->GetMeshes()[1].name = "Sphere";

	const String pack_path = ( inDirectory / "Pack.meshes" ).string();
	CHECK(pack->Save(pack_path));

	MeshAsset loaded = MeshAsset(pack_path);
	CHECK(loaded.Load());
	CHECK(loaded.GetSource() == "Assets/Tests/Pack.gltf");
	CHECK(loaded.GetMeshCount() == 2);

	for (uint32_t index = 0; index < loaded.GetMeshCount() && index < pack->GetMeshCount(); index++)
	{
		const MeshGeometry& original = *pack->GetMesh(index);
		const MeshGeometry& geometry = *loaded.GetMesh(index);

		CHECK(geometry.name == original.name);
		CHECK(geometry.positions == original.positions);
		CHECK(geometry.uvs == original.uvs);
		CHECK(geometry.normals == original.normals);
		CHECK(geometry.tangents == original.tangents);
		CHECK(geometry.indices == original.indices);
		CHECK(geometry.vertices == original.vertices);
		CHECK(geometry.bbox.GetMin() == original.bbox.GetMin() && geometry.bbox.GetMax() == original.bbox.GetMax());
	}

	CHECK(MeshAsset::CreateBuiltin("Builtin/Unknown") == nullptr);

	if (!fs::exists(inGltfFile))
	{
		gLogWarning("Tests", "Skipped the glTF mesh pack test, {} does not exist", inGltfFile.string());
		return;
	}

	const String source = MeshAsset::GetSourcePath(inGltfFile);
	const String cached_path = MeshAsset::GetCachedPath(source);

	std::error_code error_code;
	fs::remove(cached_path, error_code);

	Scene scene = Scene(nullptr);
	GltfImporter importer = GltfImporter(scene, nullptr);
	CHECK(importer.LoadFromFile(inGltfFile.string(), nullptr));

	const MeshAsset::Ptr imported = importer.GetMeshAsset();
	CHECK(imported && imported->GetMeshCount() > 0);
	CHECK(fs::exists(cached_path));
	CHECK(scene.Count<Mesh>() > 0);

	for (const auto& [entity, mesh] : scene.Each<Mesh>())
	{
		CHECK(mesh.asset == imported);
		CHECK(mesh.source == source);
		CHECK(mesh.GetGeometry() && !mesh.GetGeometry()->IsEmpty());
	}

	Assets assets;
	const MeshAsset::Ptr cached = MeshAsset::Find(assets, source);
	CHECK(cached && cached != imported && cached->GetMeshCount() == imported->GetMeshCount());

	for (uint32_t index = 0; cached && index < cached->GetMeshCount(); index++)
		CHECK(cached->GetMesh(index)->positions == imported->GetMesh(index)->positions);

	CHECK(MeshAsset::Find(assets, source) == cached);

	fs::remove(cached_path, error_code);

	Assets regenerated_assets;
	const MeshAsset::Ptr regenerated = MeshAsset::Find(regenerated_assets, source);
	CHECK(regenerated && regenerated->GetMeshCount() == imported->GetMeshCount());
	CHECK(fs::exists(cached_path));
}


static void sTestMultiComponentEach()
{
	ECStorage ecs;
	ecs.EnsureExists<Name>();
	ecs.EnsureExists<Mesh>();
	ecs.EnsureExists<Transform>();

	Array<Entity> entities;

	for (int index = 0; index < 64; index++)
	{
		const Entity entity = entities.emplace_back(ecs.Create());

		if (index % 2 == 0)
			ecs.Add<Name>(entity).name = std::to_string(index);

		if (index % 3 == 0)
			ecs.Add<Transform>(entity).position.x = float(index);

		if (index % 5 != 0)
			ecs.Add<Mesh>(entity).index = uint32_t(index);
	}

	auto Expected = [&]<typename ...Components>()
	{
		Array<Entity> expected;

		for (Entity entity : entities)
		{
			if (ecs.Exists(entity) && ecs.Has<Components...>(entity))
				expected.push_back(entity);
		}

		return expected;
	};

	auto Visited = [&](auto&& inView)
	{
		Array<Entity> visited;

		for (const auto& entry : inView)
			visited.push_back(entry.mEntity);

		std::sort(visited.begin(), visited.end());
		return visited;
	};

	auto CheckViews = [&]()
	{
		const ECStorage& const_ecs = ecs;

		const Array<Entity> name_transform = Expected.template operator()<Name, Transform>();
		const Array<Entity> mesh_transform = Expected.template operator()<Mesh, Transform>();
		const Array<Entity> name_transform_mesh = Expected.template operator()<Name, Transform, Mesh>();

		const bool name_transform_matches = Visited(ecs.Each<Name, Transform>()) == name_transform;
		const bool transform_name_matches = Visited(ecs.Each<Transform, Name>()) == name_transform;
		const bool mesh_transform_matches = Visited(ecs.Each<Mesh, Transform>()) == mesh_transform;
		const bool name_transform_mesh_matches = Visited(ecs.Each<Name, Transform, Mesh>()) == name_transform_mesh;
		const bool const_transform_mesh_matches = Visited(const_ecs.Each<Transform, Mesh>()) == mesh_transform;

		CHECK(!name_transform.empty() && !mesh_transform.empty() && !name_transform_mesh.empty());
		CHECK(name_transform_matches);
		CHECK(transform_name_matches);
		CHECK(mesh_transform_matches);
		CHECK(name_transform_mesh_matches);
		CHECK(const_transform_mesh_matches);

		const bool name_transform_count_matches = const_ecs.Count<Name, Transform>() == name_transform.size();
		const bool transform_name_count_matches = const_ecs.Count<Transform, Name>() == name_transform.size();
		const bool mesh_transform_count_matches = const_ecs.Count<Mesh, Transform>() == mesh_transform.size();
		const bool name_transform_mesh_count_matches = const_ecs.Count<Name, Transform, Mesh>() == name_transform_mesh.size();
		const bool mesh_count_matches = const_ecs.Count<Mesh>() == Expected.template operator()<Mesh>().size();

		CHECK(name_transform_count_matches);
		CHECK(transform_name_count_matches);
		CHECK(mesh_transform_count_matches);
		CHECK(name_transform_mesh_count_matches);
		CHECK(mesh_count_matches);

		for (const auto& [entity, name, transform, mesh] : ecs.Each<Name, Transform, Mesh>())
		{
			CHECK(&name == &ecs.Get<Name>(entity));
			CHECK(&transform == &ecs.Get<Transform>(entity));
			CHECK(mesh.index == uint32_t(transform.position.x));
		}
	};

	CheckViews();

	ecs.Remove<Name>(entities[6]);
	ecs.Remove<Transform>(entities[12]);
	ecs.Destroy(entities[18]);
	ecs.Remove<Mesh>(entities[0]);

	CheckViews();

	const bool is_empty = ecs.Each<Name, Transform>().IsEmpty();
	const bool expected_empty = Expected.template operator()<Name, Transform>().empty();
	CHECK(is_empty == expected_empty);
}


static void sTestTransformHierarchy()
{
	Scene scene = Scene(nullptr);

	auto CreateNode = [&](Vec3 inPosition, Entity inParent)
	{
		const Entity entity = scene.CreateSpatialEntity("Node");

		if (inParent != Entity::Null)
		{
			scene.Unparent(entity);
			scene.ParentTo(entity, inParent);
		}

		Transform& transform = scene.Get<Transform>(entity);
		transform.position = inPosition;
		transform.Compose();
		return entity;
	};

	auto WorldPosition = [&](Entity inEntity) { return Vec3(scene.Get<Transform>(inEntity).worldTransform[3]); };

	const Entity a = CreateNode(Vec3(1.0f, 0.0f, 0.0f), Entity::Null);
	const Entity b = CreateNode(Vec3(0.0f, 2.0f, 0.0f), a);
	const Entity c = CreateNode(Vec3(0.0f, 0.0f, 3.0f), b);
	const Entity d = CreateNode(Vec3(4.0f, 0.0f, 0.0f), Entity::Null);

	scene.UpdateTransforms();

	CHECK(WorldPosition(a) == Vec3(1.0f, 0.0f, 0.0f));
	CHECK(WorldPosition(b) == Vec3(1.0f, 2.0f, 0.0f));
	CHECK(WorldPosition(c) == Vec3(1.0f, 2.0f, 3.0f));
	CHECK(WorldPosition(d) == Vec3(4.0f, 0.0f, 0.0f));

	scene.Unparent(b);
	scene.ParentTo(b, d);
	scene.UpdateTransforms();

	CHECK(WorldPosition(b) == Vec3(4.0f, 2.0f, 0.0f));
	CHECK(WorldPosition(c) == Vec3(4.0f, 2.0f, 3.0f));

	scene.Remove<Transform>(a);
	const Entity e = CreateNode(Vec3(0.0f, 5.0f, 0.0f), c);
	scene.UpdateTransforms();

	CHECK(WorldPosition(c) == Vec3(4.0f, 2.0f, 3.0f));
	CHECK(WorldPosition(e) == Vec3(4.0f, 7.0f, 3.0f));

	scene.Destroy(b);
	scene.Get<Transform>(d).position = Vec3(6.0f, 0.0f, 0.0f);
	scene.Get<Transform>(d).Compose();
	scene.UpdateTransforms();

	CHECK(!scene.Exists(c) && !scene.Exists(e));
	CHECK(WorldPosition(d) == Vec3(6.0f, 0.0f, 0.0f));

	const Entity rotated = CreateNode(Vec3(1.0f, 2.0f, 3.0f), d);
	Transform& rotated_transform = scene.Get<Transform>(rotated);
	rotated_transform.rotation = glm::angleAxis(0.7f, glm::normalize(Vec3(1.0f, 2.0f, 0.5f)));
	rotated_transform.scale = Vec3(0.5f, 2.0f, 1.5f);
	rotated_transform.Compose();

	Transform& parent_transform = scene.Get<Transform>(d);
	parent_transform.rotation = glm::angleAxis(-1.3f, glm::normalize(Vec3(0.2f, 1.0f, -0.4f)));
	parent_transform.scale = Vec3(3.0f, 1.0f, 0.25f);
	parent_transform.Compose();

	scene.UpdateTransforms();

	const Mat4x4 expected = parent_transform.localTransform * rotated_transform.localTransform;
	const Mat4x4& world = scene.Get<Transform>(rotated).worldTransform;

	for (int column = 0; column < 4; column++)
	{
		for (int row = 0; row < 4; row++)
			CHECK(glm::abs(world[column][row] - expected[column][row]) < 1e-5f);
	}

	const Entity scaled_parent = CreateNode(Vec3(0.0f), Entity::Null);
	Transform& scaled_parent_transform = scene.Get<Transform>(scaled_parent);
	scaled_parent_transform.rotation = glm::angleAxis(0.9f, glm::normalize(Vec3(0.3f, 1.0f, 0.2f)));
	scaled_parent_transform.scale = Vec3(2.5f);
	scaled_parent_transform.Compose();

	const Entity scaled_child = CreateNode(Vec3(1.0f, 0.0f, 0.0f), scaled_parent);
	Transform& scaled_child_transform = scene.Get<Transform>(scaled_child);
	scaled_child_transform.rotation = glm::angleAxis(-0.4f, glm::normalize(Vec3(1.0f, 0.0f, 0.6f)));
	scaled_child_transform.scale = Vec3(0.5f, 1.0f, 3.0f);
	scaled_child_transform.Compose();

	scene.UpdateTransforms();

	const Quat expected_rotation = scaled_parent_transform.rotation * scaled_child_transform.rotation;
	const Quat world_rotation = scene.Get<Transform>(scaled_child).GetRotationWorldSpace();

	for (const Vec3& axis : { Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f) })
		CHECK(glm::length(world_rotation * axis - expected_rotation * axis) < 1e-4f);
}


static void sTestSceneMergeAndSwap()
{
	Scene source = Scene(nullptr);

	const Entity material_entity = source.Create();
	source.Add<Name>(material_entity).name = "Material";
	source.Add<Material>(material_entity).albedo = Vec4(0.25f, 0.5f, 0.75f, 1.0f);

	const Entity parent = source.CreateSpatialEntity("Parent");
	const Entity child = source.CreateSpatialEntity("Child");
	source.Unparent(child);
	source.ParentTo(child, parent);

	Mesh& mesh = source.Add<Mesh>(child);
	mesh.SetAsset(MeshAsset::CreateBuiltin(MeshAsset::cBuiltinCube), 0);
	mesh.material = material_entity;

	Scene snapshot = Scene(nullptr);
	snapshot.CopyFrom(source);

	CHECK(snapshot.GetEntities().size() == source.GetEntities().size());
	CHECK(snapshot.GetParent(child) == parent);
	CHECK(snapshot.Get<Mesh>(child).GetGeometry() == source.Get<Mesh>(child).GetGeometry());

	Scene destination = Scene(nullptr);
	const Entity existing = destination.CreateSpatialEntity("Existing");

	const Array<Entity> merged = destination.Merge(source);

	CHECK(merged.size() == 3);
	CHECK(source.GetEntities().size() == 1);
	CHECK(source.Count<Mesh>() == 0);
	CHECK(destination.Has<Name>(existing) && destination.Get<Name>(existing).name == "Existing");
	CHECK(destination.Count<Mesh>() == 1);

	const Entity merged_child = destination.GetEntities<Mesh>()[0];
	const Entity merged_parent = destination.GetParent(merged_child);

	CHECK(destination.Has<Name>(merged_child) && destination.Get<Name>(merged_child).name == "Child");
	CHECK(destination.Has<Name>(merged_parent) && destination.Get<Name>(merged_parent).name == "Parent");
	CHECK(destination.GetParent(merged_parent) == destination.GetRootEntity());
	CHECK(destination.Get<Mesh>(merged_child).GetGeometry() == snapshot.Get<Mesh>(child).GetGeometry());

	const Entity merged_material = destination.Get<Mesh>(merged_child).material;
	CHECK(destination.Has<Material>(merged_material) && destination.Get<Material>(merged_material).albedo == Vec4(0.25f, 0.5f, 0.75f, 1.0f));

	Scene swapped = Scene(nullptr);
	swapped.Swap(destination);

	CHECK(destination.GetEntities().size() == 1);
	CHECK(swapped.Count<Mesh>() == 1);
	CHECK(swapped.GetParent(merged_child) == merged_parent);
}


static void sTestEntityLifetime(const Path& inDirectory)
{
	Scene scene = Scene(nullptr);
	const size_t initial_count = scene.GetEntities().size();

	const Entity parent = scene.CreateSpatialEntity("Parent");
	const Entity child = scene.CreateSpatialEntity("Child");
	scene.ParentTo(child, parent);

	Array<Entity> spawned;
	for (int index = 0; index < 64; index++)
		spawned.push_back(scene.CreateSpatialEntity("Spawned"));

	CHECK(scene.GetEntities().size() == initial_count + 66);

	for (Entity entity : spawned)
		scene.Destroy(entity);

	scene.Destroy(parent);

	CHECK(scene.GetEntities().size() == initial_count);
	CHECK(!scene.Exists(parent) && !scene.Exists(child) && !scene.Exists(spawned.front()));
	CHECK(!scene.Has<Name>(child));

	const Entity survivor = scene.CreateSpatialEntity("Survivor");
	CHECK(scene.Exists(survivor));
	CHECK(std::find(spawned.begin(), spawned.end(), survivor) == spawned.end() && survivor != parent && survivor != child);

	Assets assets;
	const String scene_path = ( inDirectory / "lifetime.scene" ).string();
	scene.SaveToFile(scene_path, assets);

	Scene loaded = Scene(nullptr);
	loaded.OpenFromFile(scene_path, assets);

	CHECK(loaded.GetEntities().size() == scene.GetEntities().size());
	CHECK(loaded.Exists(survivor) && loaded.Get<Name>(survivor).name == "Survivor");

	const Entity created_after_load = loaded.Create();
	CHECK(created_after_load > survivor && loaded.Exists(created_after_load));
}


static void sTestJSON(const Path& inDirectory)
{
	const Path json_path = inDirectory / "test.json";

	{
		TestJSON value = TestJSON
		{
			.mFalse = false,
			.mTrue = true,
			.mSmall = 0.000001234f,
			.mVector = Vec3(1234.5678f, -0.1f, 1e-7f),
			.mQuoted = "a \"quoted\" C:\\path"
		};

		JSON::WriteArchive archive(json_path);
		archive << value;
	}

	TestJSON value;
	JSON::ReadArchive archive(json_path);
	archive >> value;

	CHECK(value.mFalse == false);
	CHECK(value.mTrue == true);
	CHECK(value.mSmall == 0.000001234f);
	CHECK(value.mVector == Vec3(1234.5678f, -0.1f, 1e-7f));
	CHECK(value.mQuoted == "a \"quoted\" C:\\path");
	CHECK(value.mSkipped == 5);
}


static void sTestJobSystem()
{
	Array<Atomic<int>> hits(10007);
	g_JobSystem.ParallelFor(uint32_t(hits.size()), 13, [&](uint32_t inIndex) { hits[inIndex].fetch_add(1); });

	bool all_hit_once = true;
	for (const Atomic<int>& hit : hits)
		all_hit_once &= hit.load() == 1;

	CHECK(all_hit_once);

	Atomic<int> stage = 0;
	Atomic<bool> order_correct = true;

	Job::Ptr first = g_JobSystem.Schedule([&]() { stage = 1; });

	Array<Job::Ptr> middle;
	for (int index = 0; index < 8; index++)
		middle.push_back(g_JobSystem.Schedule([&]() { if (stage.load() != 1) order_correct = false; }, Slice<const Job::Ptr>(&first, 1)));

	Job::Ptr last = g_JobSystem.Schedule([&]() { stage = 2; }, middle);
	last->Wait();

	CHECK(order_correct.load());
	CHECK(stage.load() == 2);

	Atomic<int> nested_sum = 0;

	{
		JobGroup outer;

		for (int outer_index = 0; outer_index < 32; outer_index++)
		{
			outer.Schedule([&]()
			{
				JobGroup inner;

				for (int inner_index = 0; inner_index < 8; inner_index++)
					inner.Schedule([&]() { nested_sum++; });
			});
		}
	}

	CHECK(nested_sum.load() == 32 * 8);
}




}


int main(int argc, char** argv)
{
	using namespace RK;

	gRegisterPrimitiveTypes();
	gRegisterComponentTypes();

	g_RTTIFactory.Register<TestJSON>();
	g_RTTIFactory.Register<TestComponentV1>();
	g_RTTIFactory.Register<TestComponentV2>();
	g_RTTIFactory.Register<TestLegacyComponent>();
	g_RTTIFactory.Register<TestScript>();

	const Path directory = fs::temp_directory_path() / "RaekorTests";
	fs::create_directories(directory);

	sTestJobSystem();
	sTestJSON(directory);
	sTestComponentTableVersioning(directory);
	sTestLegacyMemberSkipping(directory);
	sTestSceneRoundTrip(directory);
	sTestTriggerVolumes();
	sTestLoopingSoundLengths();
	sTestMeshPack(directory, "Assets/Models/glTF-Sample-Models-main/2.0/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf");
	sTestMultiComponentEach();
	sTestTransformHierarchy();
	sTestSceneMergeAndSwap();
	sTestEntityLifetime(directory);
	sTestScriptVariables(directory);
	sTestGltfBlendModes("Assets/Models/glTF-Sample-Models-main/2.0/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf");

	fs::remove_all(directory);

	if (sFailures == 0)
		gLogInfo("Tests", "All tests passed");
	else
		gLogError("Tests", "{} check(s) failed", sFailures);

	return sFailures == 0 ? 0 : 1;
}
