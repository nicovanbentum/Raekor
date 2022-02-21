#include "pch.h"
#include "DXApp.h"

int main(int argc, char** argv) {
	auto app = new Raekor::DX::DXApp();

	app->Run();

	delete app;

	return 0;
}