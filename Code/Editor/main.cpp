#include "pch.h"
#include "App.h"
#include "Launcher.h"
#include "Compiler.h"
#include "Engine/OS.h"
#include "Engine/ecs.h"
#include "Engine/timer.h"

RK_EXPORT_AGILITY_SDK()

using namespace RK;

int main(int argc, char** argv)
{
    g_CVariables = new CVariables(argc, argv);

    bool should_launch = true;
    bool is_asset_compiler = OS::sCheckCommandLineOption("-asset_compiler");

    Path game_scene_file;

    if (g_CVariables->Create("enable_launcher", 0) && !is_asset_compiler)
    {
        Launcher launcher;
        launcher.Run();

        should_launch = launcher.ShouldLaunch();
        game_scene_file = launcher.GetGameSceneFile();
    }

    if (!should_launch)
        return 0;

    if (!game_scene_file.empty())
    {
        delete g_CVariables;

        String command_line = std::format("\"{}\" -scene=\"{}\"", Launcher::sGetGameExecutablePath().string(), game_scene_file.string());

        PROCESS_INFORMATION process_info = {};
        STARTUPINFOA startup_info = { sizeof(startup_info) };

        const DWORD creation_flags = IsDebuggerPresent() ? 0 : CREATE_NO_WINDOW;

        if (!CreateProcessA(nullptr, command_line.data(), nullptr, nullptr, FALSE, creation_flags, nullptr, nullptr, &startup_info, &process_info))
        {
            const String message = std::format("Failed to start {} (error {}).", Launcher::sGetGameExecutablePath().string(), GetLastError());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RK Launcher", message.c_str(), nullptr);
            return 1;
        }

        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);

        return 0;
    }

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