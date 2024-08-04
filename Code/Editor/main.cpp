#include "pch.h"
#include "App.h"
#include "Launcher.h"
#include "Compiler.h"
#include "Engine/OS.h"
#include "Engine/ecs.h"
#include "Engine/timer.h"

using namespace RK;

int main(int argc, char** argv)
{
    g_CVariables = new CVariables(argc, argv);

    bool should_launch = true;
    bool is_asset_compiler = OS::sCheckCommandLineOption("-asset_compiler");

    if (g_CVariables->Create("enable_launcher", 0) && !is_asset_compiler)
    {
        Launcher launcher;
        launcher.Run();

        should_launch = launcher.ShouldLaunch();
    }

    if (!should_launch)
        return 0;

    Timer timer;

    Application* app = nullptr;

    if (is_asset_compiler)
        app = new CompilerApp(IsDebuggerPresent() ? WindowFlag::NONE : WindowFlag::HIDDEN);
    else
        app = new DX12::DXApp();

    app->LogMessage(std::format("[App] App creation took {:.2f} seconds", timer.GetElapsedTime()));

    app->Run();

    delete app;
    delete g_CVariables;

    return 0;
}