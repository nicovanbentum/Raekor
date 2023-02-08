#include "pch.h"
#include "DXApp.h"
#include "Raekor/util.h"
#include "Raekor/timer.h"

int main(int argc, char** argv) {
	Raekor::Timer timer;

	auto app = new Raekor::DX12::DXApp();

	std::cout << std::format("App creation took {} seconds.\n", timer.GetElapsedTime());

	app->Run();

	delete app;

	return 0;
}