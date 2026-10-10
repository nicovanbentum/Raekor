#include "PCH.h"
#include "Game.h"
#include "Launcher.h"
#include "../Engine/OS.h"
#include "../Engine/RTTI.h"
#include "../Engine/CVars.h"
#include "../Engine/Script.h"

RK_EXPORT_AGILITY_SDK()

using namespace RK;

int main(int argc, char** argv)
{
    g_CVariables = new CVariables(argc, argv);

    Path scene_file = OS::sGetCommandLineValue("-scene");

    if (scene_file.empty())
    {
        Launcher launcher;
        launcher.Run();

        if (!launcher.ShouldLaunch())
        {
            delete g_CVariables;
            return 0;
        }

        scene_file = launcher.GetSceneFile();
    }

    Application* app = new GameApp(scene_file);

    app->Run();

    delete app;
    delete g_CVariables;

    return 0;
}
