#include "pch.h"
#include "DXApp.h"
#include "Raekor/timer.h"

int main(int argc, char** argv) {
	Raekor::Timer timer;

	auto app = new Raekor::DX::DXApp();

	std::cout << "Constructor time: " << timer.GetElapsedTime() << " seconds.";

	app->Run();

	delete app;

	return 0;
}