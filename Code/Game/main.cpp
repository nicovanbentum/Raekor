#include "PCH.h"
#include "Game.h"
#include "../Engine/RTTI.h"
#include "../Engine/CVars.h"
#include "../Engine/Script.h"

using namespace RK;

int main(int argc, char** argv)
{
    g_CVariables = new CVariables(argc, argv);

    Application* app = new GameApp();

    app->Run();

    delete app;

    return 0;
}

