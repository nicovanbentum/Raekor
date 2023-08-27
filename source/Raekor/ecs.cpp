#include "pch.h"
#include "ecs.h"

namespace Raekor::ecs {

struct TestName {
    RTTI_CLASS_HEADER(TestName);

    std::string name;
};

RTTI_CLASS_CPP(TestName) {}

struct TestTransform {
    RTTI_CLASS_HEADER(TestTransform);

    Vec3 pos;
};

RTTI_CLASS_CPP(TestTransform) {}


struct TestMaterial {
    RTTI_CLASS_HEADER(TestMaterial);

    Vec4 color;
};

RTTI_CLASS_CPP(TestMaterial) {}


void RunTests() {
    ECS ecs;
    Entity entity = ecs.Create();
    {
        TestName& name = ecs.Add<TestName>(entity);
        name.name = "FirstEntity";
    }

    assert(ecs.IsValid(entity));
    assert(ecs.Has<TestName>(entity));
    assert(ecs.Get<TestName>(entity).name == "FirstEntity");
    assert(ecs.Count<TestName>() == 1);
    assert(ecs.GetPtr<TestName>(entity) != nullptr);
    assert(ecs.GetStorage<TestName>().Length() == 1);
    assert(ecs.GetEntities<TestName>().Length() == 1);

    ecs.Add<TestTransform>(entity);
    {
        TestMaterial& material = ecs.Add<TestMaterial>(entity);
    }

    for (const auto& [entity, name, transform] : ecs.Each<TestName, TestTransform>())
        name.name = "NotFirstEntity";

    for (const auto& [entity, name, transform] : ecs.Each<TestName, TestTransform>())
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
}

}
