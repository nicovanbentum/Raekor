#include "pch.h"
#include "ecs.h"
#include "scene.h"

namespace RK {

RTTI_DEFINE_TYPE_PRIMITIVE(Entity);

struct TestName
{
	RTTI_DECLARE_TYPE(TestName);

	std::string name;
};

RTTI_DEFINE_TYPE(TestName) {}

struct TestTransform
{
	RTTI_DECLARE_TYPE(TestTransform);

	Vec3 pos;
};

RTTI_DEFINE_TYPE(TestTransform) {}


struct TestMaterial
{
	RTTI_DECLARE_TYPE(TestMaterial);

	Vec4 color;
};

RTTI_DEFINE_TYPE(TestMaterial) {}


void RunECStorageTests()
{
	ECStorage ecs;
	ecs.EnsureExists<TestName>();
	ecs.EnsureExists<TestMaterial>();
	ecs.EnsureExists<TestTransform>();

	Entity entity = ecs.Create();
	{
		TestName& name = ecs.Add<TestName>(entity);
		name.name = "FirstEntity";
	}

	assert(ecs.Exists(entity));
	assert(ecs.Has<TestName>(entity));
	assert(ecs.Get<TestName>(entity).name == "FirstEntity");
	assert(ecs.Count<TestName>() == 1);
	assert(ecs.GetPtr<TestName>(entity) != nullptr);
	assert(ecs.GetStorage<TestName>().size() == 1);
	assert(ecs.GetEntities<TestName>().size() == 1);

	ecs.Add<TestTransform>(entity);
	{
		TestMaterial& material = ecs.Add<TestMaterial>(entity);
	}

	for (const auto& [entity, name, transform] : ecs.Each<TestName, TestTransform>())
		name.name = "NotFirstEntity";

	for (const auto& [entity, name, transform] : ecs.Each<TestName, TestTransform>())
		assert(name.name == "NotFirstEntity");

	for (const auto& [entity, name, transform, material] : ecs.Each<TestName, TestTransform, TestMaterial>())
		assert(name.name == "NotFirstEntity");

	ecs.Remove<TestName>(entity);
	assert(!ecs.Has<TestName>(entity));

	{
		TestName& name = ecs.Add<TestName>(entity);
		name.name = "SecondEntity";
	}

	assert(ecs.Get<TestName>(entity).name == "SecondEntity");

	for (const auto& [entity, name, transform] : ecs.Each<TestName, TestTransform>())
		std::cout << name.name << std::endl;

	Array<Entity> entities;

	for (int i = 0; i < 20; i++)
		ecs.Add<TestMaterial>(entities.emplace_back(ecs.Create()));

	int count = ecs.Count<TestMaterial>();

	for (int i = 0; i < 20; i++)
		ecs.Destroy(entities[i]);

	entities.clear();

	for (int i = 0; i < 20; i++)
		ecs.Add<TestMaterial>(entities.emplace_back(ecs.Create()));

	int new_count = ecs.Count<TestMaterial>();
	assert(new_count == count);
}

} // namespace RK
