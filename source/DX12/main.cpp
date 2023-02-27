#include "pch.h"
#include "DXApp.h"
#include "Raekor/util.h"
#include "Raekor/timer.h"
#include "Raekor/cvars.h"

using namespace Raekor;

int main(int argc, char** argv) {
	Timer timer;

	CVars::ParseCommandLine(argc, argv);

	auto app = new DX12::DXApp();

	std::cout << std::format("App creation took {} seconds.\n", timer.GetElapsedTime());

	app->Run();

	delete app;

	return 0;
}