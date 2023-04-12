#include "pch.h"
#include "DXApp.h"
#include "Raekor/util.h"
#include "Raekor/timer.h"
#include "Raekor/cvars.h"
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

	Timer timer;

	auto app = new DX12::DXApp();

	std::cout << std::format("App creation took {} seconds.\n", timer.GetElapsedTime());

	app->Run();

	delete app;

	return 0;
}