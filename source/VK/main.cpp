#include "pch.h"
#include "VKApp.h"

using namespace Raekor;

int main(int argc, char** argv)
{
	g_RTTIFactory.Register(RTTI_OF(ConfigSettings));

	auto app = VK::PathTracer();

	app.Run();

	return 0;
}