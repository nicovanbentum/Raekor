#include "pch.h"
#include "app.h"
#include "Raekor/OS.h"
#include "Raekor/ecs.h"
#include "Raekor/launcher.h"
#include "Raekor/compiler.h"

using namespace RK;

int main(int argc, char** argv)
{
    g_CVars.ParseCommandLine(argc, argv);

    bool should_launch = true;

    if (g_CVars.Create("enable_launcher", 0) && !OS::sCheckCommandLineOption("-asset_compiler"))
    {
        Launcher launcher;
        launcher.Run();

        should_launch = launcher.ShouldLaunch();
    }

    std::string s;

    if (!should_launch)
        return 0;

    Timer timer;

    Application* app = nullptr;

    if (OS::sCheckCommandLineOption("-asset_compiler"))
        app = new CompilerApp(IsDebuggerPresent() ? WindowFlag::NONE : WindowFlag::HIDDEN);
    else
        app = new GL::GLApp();

    app->LogMessage(std::format("[App] startup took {:.2f} seconds", timer.GetElapsedTime()));

    app->Run();

    delete app;

    return 0;
}