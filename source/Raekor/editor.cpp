#include "pch.h"
#include "editor.h"

#include "OS.h"
#include "rmath.h"
#include "systems.h"
#include "components.h"

#include "widgets/assetsWidget.h"
#include "widgets/randomWidget.h"
#include "widgets/metricsWidget.h"
#include "widgets/menubarWidget.h"
#include "widgets/consoleWidget.h"
#include "widgets/viewportWidget.h"
#include "widgets/inspectorWidget.h"
#include "widgets/hierarchyWidget.h"
#include "widgets/NodeGraphWidget.h"

namespace Raekor {

IEditor::IEditor(WindowFlags inWindowFlags, IRenderInterface* inRenderer) :
    Application(inWindowFlags),
    m_Scene(inRenderer),
    m_Physics(inRenderer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGui::StyleColorsDark();

    // get GUI i/o and set a bunch of settings
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    // io.ConfigDockingWithShift = true;

    GUI::SetTheme();
    ImGui::GetStyle().ScaleAllSizes(1.33333333f);
    ImNodes::StyleColorsDark();

    if (!m_Settings.mFontFile.empty())
        GUI::SetFont(m_Settings.mFontFile.c_str());

    m_Widgets.Register<AssetsWidget>(this);
    m_Widgets.Register<RandomWidget>(this);
    m_Widgets.Register<MenubarWidget>(this);
    m_Widgets.Register<MetricsWidget>(this);
    m_Widgets.Register<ConsoleWidget>(this);
    m_Widgets.Register<NodeGraphWidget>(this);
    m_Widgets.Register<ViewportWidget>(this);
    m_Widgets.Register<InspectorWidget>(this);
    m_Widgets.Register<HierarchyWidget>(this);

    LogMessage("[Editor] initialization done.");

    // sponza specific
    m_Viewport.GetCamera().Move(Vec2(42.0f, 10.0f));
    m_Viewport.GetCamera().Zoom(5.0f);
    m_Viewport.GetCamera().Look(Vec2(1.65f, 0.2f));
    m_Viewport.SetFieldOfView(65.0f);

    // launch the asset compiler  app to the system tray
    // Make sure we don't try this when we are the the compiler app 
    // (I've had to restart my PC 3 times already because of recursive process creation lol)
    if (OS::sCheckCommandLineOption("-asset_compiler")) {
        Application::Terminate();
        assert(false);
        return;
    }
   
    if (g_CVars.Create("launch_asset_compiler_on_startup", 0)) {
        auto compiler_app_cmd_line = OS::sGetExecutablePath().string() + " -asset_compiler";

        PROCESS_INFORMATION pi = {};
        STARTUPINFO si = { sizeof(si) };

        if (CreateProcessA(NULL, &compiler_app_cmd_line[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            m_CompilerProcess = pi.hProcess;
        else
            LogMessage("Failed to start asset compiler process.");
    }
}


IEditor::~IEditor() {
    if (m_CompilerProcess) {
        TerminateProcess(m_CompilerProcess, 0);
        CloseHandle(m_CompilerProcess);
    }
}


void IEditor::OnUpdate(float inDeltaTime) {
    // check if any BoxCollider's are waiting to be registered
    m_Physics.OnUpdate(m_Scene);

    // step the physics simulation
    if (m_Physics.GetState() == Physics::Stepping)
        m_Physics.Step(m_Scene, inDeltaTime);

    // update camera matrices
    m_Viewport.OnUpdate(inDeltaTime);

    // update Transform components
    m_Scene.UpdateTransforms();

    // update PointLight and DirectionalLight components
    m_Scene.UpdateLights();

    // update Skeleton and Animation components
    m_Scene.UpdateAnimations(inDeltaTime);


    // update NativeScript components
    m_Scene.UpdateNativeScripts(inDeltaTime);

    // start ImGui
    GUI::BeginFrame();
    GUI::BeginDockSpace();

    // draw widgets
    m_Widgets.Draw(inDeltaTime);

    // end ImGui
    GUI::EndDockSpace();
    GUI::EndFrame();

    // Applications that implement IEditor should call IEditor::OnUpdate first, then do their own rendering
}



void IEditor::OnEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    if (m_Widgets.GetWidget<ViewportWidget>()->IsHovered() || SDL_GetRelativeMouseMode())
        CameraController::OnEvent(m_Viewport.GetCamera(), event);

    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            for (;;) {
                auto temp_event = SDL_Event{};
                SDL_PollEvent(&temp_event);

                if (temp_event.window.event == SDL_WINDOWEVENT_RESTORED)
                    break;
            }
        }
    }

    if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        switch (event.key.keysym.sym) {
        case SDLK_DELETE: {
            if (m_ActiveEntity != NULL_ENTITY) {
                if (m_Scene.Has<Node>(m_ActiveEntity)) {
                    auto tree = NodeSystem::sGetFlatHierarchy(m_Scene, m_ActiveEntity);
                    for (auto entity : tree) {
                        NodeSystem::sRemove(m_Scene, m_Scene.Get<Node>(entity));
                        m_Scene.Destroy(entity);
                    }

                    NodeSystem::sRemove(m_Scene, m_Scene.Get<Node>(m_ActiveEntity));
                }

                m_Scene.Destroy(m_ActiveEntity);
                m_ActiveEntity = NULL_ENTITY;
            }
        } break;
        case SDLK_d: {
            if (SDL_GetModState() & KMOD_LCTRL)
                m_Scene.Clone(m_Scene.Create());
        } break;
        }
    }

    m_Widgets.OnEvent(event);
}


void IEditor::LogMessage(const std::string& inMessage) {
    Application::LogMessage(inMessage);

    auto console_widget = m_Widgets.GetWidget<ConsoleWidget>();
    if (console_widget) {
        // Flush any pending messages
        if (!m_Messages.empty())
            for (const auto& message : m_Messages)
                console_widget->LogMessage(message);

        m_Messages.clear();
        console_widget->LogMessage(inMessage);
    }
    else {
        m_Messages.push_back(inMessage);
    }
}

} // Raekor 