#include "pch.h"
#include "App.h"
#include "Compiler.h"
#include "ShaderGraph.h"
#include "Engine/OS.h"
#include "Engine/ecs.h"
#include "Engine/timer.h"

RK_EXPORT_AGILITY_SDK()

using namespace RK;

int main(int argc, char** argv)
{
    g_CVariables = new CVariables(argc, argv);

    if (const String shader_graphs = OS::sGetCommandLineValue("-shader_graph"); !shader_graphs.empty())
    {
        bool success = true;
        std::istringstream stream(shader_graphs);

        for (String graph_file; std::getline(stream, graph_file, ','); )
            success &= ShaderGraphBuilder::sGenerateFromGraphFile(graph_file);

        delete g_CVariables;
        return success ? 0 : 1;
    }

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