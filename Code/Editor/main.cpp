#include "pch.h"
#include "App.h"
#include "Compiler.h"
#include "Engine/OS.h"
#include "Engine/ecs.h"
#include "Engine/timer.h"

RK_EXPORT_AGILITY_SDK()

using namespace RK;

int main(int argc, char** argv)
{
    g_CVariables = new CVariables(argc, argv);

    bool is_asset_compiler = OS::sCheckCommandLineOption("-asset_compiler");

    Timer timer;

    Application* app = nullptr;

    if (is_asset_compiler)
        app = new CompilerApp(IsDebuggerPresent() ? WindowFlag::NONE : WindowFlag::HIDDEN);
    else
        app = new DX12::DXApp();

    gLogInfo("App", "App creation took {:.2f} seconds", timer.GetElapsedTime());

    app->Run();

    delete app;
    delete g_CVariables;

    return 0;
}