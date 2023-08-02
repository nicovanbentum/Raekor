#include "pch.h"
#include "app.h"
#include "Raekor/ecs.h"
#include "Raekor/launcher.h"

using namespace Raekor;
using namespace Raekor::ecs;

int main(int argc, char** argv) {
    gRegisterComponentTypes();
    RTTIFactory::Register(RTTI_OF(ConfigSettings));

    g_CVars.ParseCommandLine(argc, argv);

    ECS ecs;
    Raekor::ecs::Entity entity = ecs.Create();

    {
        Name& name = ecs.Add<Name>(entity);
        name.name = "FirstEntity";
    }

    ecs.Add<Transform>(entity);

    {
        Material& material = ecs.Add<Material>(entity);
    }

    for (const auto& [entity, name, transform] : ecs.Each<Name, Transform>())
        std::cout << name.name << std::endl;

    ecs.Remove<Name>(entity);
    assert(!ecs.Has<Name>(entity));

    {
        Name& name = ecs.Add<Name>(entity);
        name.name = "SecondEntity";
    }

    for (const auto& [entity, name, transform] : ecs.Each<Name, Transform>())
        std::cout << name.name << std::endl;

	auto should_launch = true;

	if (g_CVars.Create("enable_launcher", 0)) {
		Launcher launcher;
		launcher.Run();

		should_launch = launcher.ShouldLaunch();
	}

	if (!should_launch)
		return 0;


    auto app =  new GL::GLApp();

    // app->Run();

    delete app;

    return 0;
}