#include "pch.h"
#include "app.h"
#include "Raekor/OS.h"
#include "Raekor/json.h"
#include "Raekor/input.h"
#include "Raekor/rmath.h"
#include "Raekor/cvars.h"
#include "Raekor/systems.h"

#include "assetsWidget.h"
#include "randomWidget.h"
#include "metricsWidget.h"
#include "menubarWidget.h"
#include "consoleWidget.h"
#include "viewportWidget.h"
#include "inspectorWidget.h"
#include "hierarchyWidget.h"
#include "NodeGraphWidget.h"

namespace Raekor::GL {

GLApp::GLApp() :
    Application(WindowFlag::OPENGL | WindowFlag::RESIZE),
    m_Renderer(m_Window, m_Viewport),
    m_Scene(&m_Renderer)
{
    GUI::SetTheme();
    ImGui::GetStyle().ScaleAllSizes(1.33333333f);
    if (!m_Settings.mFontFile.empty())
        GUI::SetFont(m_Settings.mFontFile.c_str());

    if (FileSystem::exists(m_Settings.mSceneFile) && Path(m_Settings.mSceneFile).extension() == ".scene") {
        SDL_SetWindowTitle(m_Window, std::string(m_Settings.mSceneFile + " - Raekor Renderer").c_str());
        m_Scene.OpenFromFile(m_Assets, m_Settings.mSceneFile);
        LogMessage("Loaded scene from file: " + m_Settings.mSceneFile);
    }

    m_Widgets.Register<AssetsWidget>(this);
    m_Widgets.Register<RandomWidget>(this);
    m_Widgets.Register<MenubarWidget>(this);
    m_Widgets.Register<ConsoleWidget>(this);
    m_Widgets.Register<MetricsWidget>(this);
    m_Widgets.Register<NodeGraphWidget>(this);
    m_Widgets.Register<ViewportWidget>(this);
    m_Widgets.Register<InspectorWidget>(this);
    m_Widgets.Register<HierarchyWidget>(this);

    LogMessage("Initialization done.");
    auto sink = m_Scene.on_destroy<Mesh>();

    //m_Viewport.GetCamera().Zoom(-50.0f);
    //m_Viewport.GetCamera().Move(glm::vec2(0.0f, 10.0f));

    // sponza specific
    m_Viewport.GetCamera().Move(Vec2(42.0f, 10.0f));
    m_Viewport.GetCamera().Zoom(5.0f);
    m_Viewport.GetCamera().Look(Vec2(1.65f, 0.2f));
}



void GLApp::OnUpdate(float inDeltaTime) {
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

    // add debug geometry around a selected mesh
    if (GetActiveEntity() != sInvalidEntity && m_Scene.all_of<Mesh>(m_ActiveEntity)) {
        const auto& [mesh, transform] = m_Scene.get<Mesh, Transform>(m_ActiveEntity);
        m_Renderer.AddDebugBox(mesh.aabb[0], mesh.aabb[1], transform.worldTransform);
    }

    // start ImGui
    GUI::BeginFrame();
    GUI::BeginDockSpace();

    // draw widgets
    m_Widgets.Draw(inDeltaTime);

    // end ImGui
    GUI::EndDockSpace();
    GUI::EndFrame();

    // render scene
    m_Renderer.Render(m_Scene, m_Viewport);
}



void GLApp::OnEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    if (m_Widgets.GetWidget<ViewportWidget>()->IsHovered() || SDL_GetRelativeMouseMode())
        CameraController::OnEvent(m_Viewport.GetCamera(), event);

    if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        switch (event.key.keysym.sym) {
            case SDLK_DELETE: {
                if (m_ActiveEntity != entt::null) {
                    if (m_Scene.all_of<Node>(m_ActiveEntity)) {
                        auto tree = NodeSystem::sGetFlatHierarchy(m_Scene, m_Scene.get<Node>(m_ActiveEntity));
                        for (auto entity : tree) {
                            NodeSystem::sRemove(m_Scene, m_Scene.get<Node>(entity));
                            m_Scene.destroy(entity);
                        }

                        NodeSystem::sRemove(m_Scene, m_Scene.get<Node>(m_ActiveEntity));
                    }

                    m_Scene.destroy(m_ActiveEntity);
                    m_ActiveEntity = entt::null;
                }
            } break;
            case SDLK_d: {
                if (SDL_GetModState() & KMOD_LCTRL) {
                    auto copy = m_Scene.create();

                    m_Scene.visit(m_ActiveEntity, [&](const entt::type_info info) {
                        gForEachTupleElement(Components, [&](auto component) {
                            using ComponentType = decltype(component)::type;
                            if (info.seq() == entt::type_seq<ComponentType>())
                                clone<ComponentType>(m_Scene, m_ActiveEntity, copy);
                        });
                    });

                    if (auto mesh = m_Scene.try_get<Mesh>(copy))
                        m_Renderer.UploadMeshBuffers(*mesh);
                }
            } break;
        }
    }

    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_Renderer.CreateRenderTargets(m_Viewport);
    }

    m_Widgets.OnEvent(event);
}


void GLApp::LogMessage(const std::string& inMessage) {
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

} // raekor