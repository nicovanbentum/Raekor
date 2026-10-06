#include "PCH.h"
#include "ECS.h"
#include "OS.h"
#include "JSON.h"
#include "GLTF.h"
#include "Scene.h"
#include "Assets.h"
#include "Timer.h"
#include "Member.h"
#include "Archive.h"
#include "Threading.h"
#include "Components.h"

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

		Mesh& mesh = scene.Add<Mesh>(entity);
		Mesh::CreateCube(mesh, 1.0f + index);
		mesh.material = material_entity;

		if (index % 4 == 0)
		{
			Light& light = scene.Add<Light>(entity);
			light.type = LIGHT_TYPE_POINT;
			light.color = Vec4(1.0f, 0.5f, 0.25f, float(index));
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

		CHECK(loaded_mesh.positions == original_mesh.positions);
		CHECK(loaded_mesh.indices == original_mesh.indices);
		CHECK(loaded_mesh.vertices == original_mesh.vertices);
		CHECK(loaded_mesh.material == original_mesh.material);

		const Material& original_material = scene.Get<Material>(original_mesh.material);
		const Material& loaded_material = loaded.Get<Material>(loaded_mesh.material);
		CHECK(loaded_material.albedo == original_material.albedo);
		CHECK(loaded_material.roughness == original_material.roughness);
		CHECK(loaded_material.blendMode == original_material.blendMode);
		CHECK(loaded_material.alphaCutoff == original_material.alphaCutoff);

		CHECK(loaded.Has<Light>(entity) == scene.Has<Light>(entity));

		if (scene.Has<Light>(entity))
			CHECK(loaded.Get<Light>(entity).color == scene.Get<Light>(entity).color);
	}
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
	Mesh::CreateCube(mesh, 2.0f);
	mesh.material = material_entity;

	Scene snapshot = Scene(nullptr);
	snapshot.CopyFrom(source);

	CHECK(snapshot.GetEntities().size() == source.GetEntities().size());
	CHECK(snapshot.GetParent(child) == parent);
	CHECK(snapshot.Get<Mesh>(child).positions == source.Get<Mesh>(child).positions);

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
	CHECK(destination.Get<Mesh>(merged_child).positions == snapshot.Get<Mesh>(child).positions);

	const Entity merged_material = destination.Get<Mesh>(merged_child).material;
	CHECK(destination.Has<Material>(merged_material) && destination.Get<Material>(merged_material).albedo == Vec4(0.25f, 0.5f, 0.75f, 1.0f));

	Scene swapped = Scene(nullptr);
	swapped.Swap(destination);

	CHECK(destination.GetEntities().size() == 1);
	CHECK(swapped.Count<Mesh>() == 1);
	CHECK(swapped.GetParent(merged_child) == merged_parent);
}


static void sTestLegacySceneConversion(const Path& inDirectory, const Path& inLegacyScene)
{
	if (!fs::exists(inLegacyScene))
	{
		gLogWarning("Tests", "Skipping legacy scene conversion test, {} does not exist", inLegacyScene.string());
		return;
	}

	Assets assets;

	Timer timer;

	Scene legacy = Scene(nullptr);
	legacy.OpenFromFile(inLegacyScene.string(), assets);

	const double legacy_load_time = timer.Restart();

	CHECK(legacy.GetEntities().size() > 1);

	const String converted_path = ( inDirectory / "converted.scene" ).string();
	legacy.SaveToFile(converted_path, assets);

	timer.Restart();

	Scene converted = Scene(nullptr);
	converted.OpenFromFile(converted_path, assets);

	const double converted_load_time = timer.Restart();

	gLogInfo("Tests", "Legacy: {:.1f} MB loaded in {:.3f} s, current format: {:.1f} MB loaded in {:.3f} s",
		fs::file_size(inLegacyScene) / 1048576.0, legacy_load_time, fs::file_size(converted_path) / 1048576.0, converted_load_time);

	CHECK(converted.GetEntities() == legacy.GetEntities());

	auto CompareCounts = [&]<typename Component>()
	{
		CHECK(converted.Count<Component>() == legacy.Count<Component>());
	};

	std::apply([&](const auto& ... inComponents)
	{
		( CompareCounts.template operator()<typename std::decay_t<decltype( inComponents )>::type>(), ... );
	}, Components);

	for (const auto& [entity, mesh] : legacy.Each<Mesh>())
	{
		const Mesh& converted_mesh = converted.Get<Mesh>(entity);
		CHECK(converted_mesh.positions == mesh.positions);
		CHECK(converted_mesh.normals == mesh.normals);
		CHECK(converted_mesh.indices == mesh.indices);
		CHECK(converted_mesh.vertices == mesh.vertices);
		CHECK(converted_mesh.material == mesh.material);
	}

	for (const auto& [entity, skeleton] : legacy.Each<Skeleton>())
	{
		const Skeleton& converted_skeleton = converted.Get<Skeleton>(entity);
		CHECK(converted_skeleton.boneWeights == skeleton.boneWeights);
		CHECK(converted_skeleton.boneOffsetMatrices == skeleton.boneOffsetMatrices);
		CHECK(converted_skeleton.rootBone.children.size() == skeleton.rootBone.children.size());
	}

	for (const auto& [entity, name] : legacy.Each<Name>())
		CHECK(converted.Get<Name>(entity).name == name.name);

	gLogInfo("Tests", "Converted {} ({} entities, {} meshes) from the legacy format", inLegacyScene.string(), legacy.GetEntities().size(), legacy.Count<Mesh>());
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

	const Path directory = fs::temp_directory_path() / "RaekorTests";
	fs::create_directories(directory);

	sTestJobSystem();
	sTestJSON(directory);
	sTestComponentTableVersioning(directory);
	sTestLegacyMemberSkipping(directory);
	sTestSceneRoundTrip(directory);
	sTestSceneMergeAndSwap();
	sTestGltfBlendModes("Assets/Models/glTF-Sample-Models-main/2.0/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf");

	String legacy_scene = OS::sGetCommandLineValue("-legacy_scene");

	if (legacy_scene.empty())
		legacy_scene = "Cached/Models/DancingStormtrooper/scene.scene";

	sTestLegacySceneConversion(directory, legacy_scene);

	fs::remove_all(directory);

	if (sFailures == 0)
		gLogInfo("Tests", "All tests passed");
	else
		gLogError("Tests", "{} check(s) failed", sFailures);

	return sFailures == 0 ? 0 : 1;
}
