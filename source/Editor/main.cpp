#include "pch.h"
#include "editor.h"
#include "Raekor/launcher.h"

using namespace Raekor;

int main(int argc, char** argv) {
    RTTIFactory::Register(RTTI_OF(ConfigSettings));

	CVars::ParseCommandLine(argc, argv);

	auto should_launch = true;

	if (CVars::sCreate("enable_launcher", 0)) {
		Launcher launcher;
		launcher.Run();

		should_launch = launcher.ShouldLaunch();
	}

	if (!should_launch)
		return 0;


    auto app =  new Editor();

    app->Run();

    delete app;

    return 0;
}