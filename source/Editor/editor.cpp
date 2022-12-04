#include "pch.h"
#include "editor.h"
#include "Raekor/OS.h"
#include "Raekor/input.h"
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

namespace Raekor {


Editor::Editor() :
    Application(RendererFlags::OPENGL),
    m_Renderer(m_Window, m_Viewport) 
{
    GUI::SetTheme(m_Settings.themeColors);
    GUI::SetFont(m_Settings.font.c_str());

    m_Scene.SetUploadMeshCallbackFunction(GLRenderer::sUploadMeshBuffers);
    m_Scene.SetUploadMaterialCallbackFunction(GLRenderer::sUploadMaterialTextures);

    if (FileSystem::exists(m_Settings.defaultScene) && Path(m_Settings.defaultScene).extension() == ".scene") {
        SDL_SetWindowTitle(m_Window, std::string(m_Settings.defaultScene + " - Raekor Renderer").c_str());
        m_Scene.OpenFromFile(m_Assets, m_Settings.defaultScene);
    }

    m_Widgets.emplace_back(std::make_shared<AssetsWidget>(this));
    m_Widgets.emplace_back(std::make_shared<RandomWidget>(this));
    m_Widgets.emplace_back(std::make_shared<MenubarWidget>(this));
    m_Widgets.emplace_back(std::make_shared<ConsoleWidget>(this));
    m_Widgets.emplace_back(std::make_shared<MetricsWidget>(this));
    m_Widgets.emplace_back(std::make_shared<NodeGraphWidget>(this));
    m_Widgets.emplace_back(std::make_shared<ViewportWidget>(this));
    m_Widgets.emplace_back(std::make_shared<InspectorWidget>(this));
    m_Widgets.emplace_back(std::make_shared<HierarchyWidget>(this));

    std::cout << "Initialization done.\n";
    auto sink = m_Scene.on_destroy<Mesh>();

    m_Viewport.GetCamera().Zoom(-50.0f);
    m_Viewport.GetCamera().Move(glm::vec2(0.0f, 10.0f));
}



void Editor::OnUpdate(float dt) {
    // Check if any BoxCollider's are waiting to be registered
    m_Physics.OnUpdate(m_Scene);

    // Step the physics simulation
    if (m_Physics.GetState() == Physics::Stepping)
        m_Physics.Step(m_Scene, dt);

    // update the camera
    m_Viewport.OnUpdate(dt);

    // update scene components
    m_Scene.UpdateTransforms();
    m_Scene.UpdateLights();

    // update animations in parallel


    m_Scene.view<Skeleton>().each([&](Skeleton& skeleton) {
        Async::sQueueJob([&]() {
            skeleton.UpdateFromAnimation(skeleton.animations[0], dt);
        });
    });

    Async::sWait();

    // update scripts
    m_Scene.view<NativeScript>().each([&](NativeScript& component) {
        if (component.script) {
            try {
                component.script->OnUpdate(dt);
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
            }
        }
    });

    // draw the bounding box of the active entity
    if (m_ActiveEntity != entt::null && m_Scene.all_of<Mesh>(m_ActiveEntity)) {
        const auto& [mesh, transform] = m_Scene.get<Mesh, Transform>(m_ActiveEntity);
        m_Renderer.AddDebugBox(mesh.aabb[0], mesh.aabb[1], transform.worldTransform);
    }

    // start ImGui
    GUI::BeginFrame();
    GUI::BeginDockSpace();

    // draw widgets
    for (const auto& widget : m_Widgets)
        if (widget->IsVisible())
            widget->draw(dt);

    // end ImGui
    GUI::EndDockSpace();
    GUI::EndFrame();

    // render scene
    m_Renderer.Render(m_Scene, m_Viewport);

    // swap the backbuffer
    SDL_GL_SwapWindow(m_Window);
}



void Editor::OnEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    // free the mouse if the window loses focus
    auto flags = SDL_GetWindowFlags(m_Window);
    if (!(flags & SDL_WINDOW_INPUT_FOCUS || flags & SDL_WINDOW_MINIMIZED)) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        return;
    }

    const bool is_viewport_hovered = GetWidget<ViewportWidget>()->IsHovered();

    if (event.button.button == 2 || event.button.button == 3) {
        if (event.type == SDL_MOUSEBUTTONDOWN && is_viewport_hovered)
            SDL_SetRelativeMouseMode(SDL_TRUE);
        else if (event.type == SDL_MOUSEBUTTONUP)
            SDL_SetRelativeMouseMode(SDL_FALSE);
    }

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

                    if (auto mesh = m_Scene.try_get<Mesh>(copy); mesh)
                        GLRenderer::sUploadMeshBuffers(*mesh);
                }
            } break;
        }
    }

    if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym == SDLK_LSHIFT) {
        m_Viewport.GetCamera().zoomConstant *= 20.0f;
        m_Viewport.GetCamera().moveConstant *= 20.0f;
    }

    if (event.type == SDL_KEYUP && !event.key.repeat && event.key.keysym.sym == SDLK_LSHIFT) {
        m_Viewport.GetCamera().zoomConstant /= 20.0f;
        m_Viewport.GetCamera().moveConstant /= 20.0f;
    }

    auto& camera = m_Viewport.GetCamera();

    if (event.type == SDL_MOUSEMOTION) {
        if (SDL_GetRelativeMouseMode() && Input::sIsButtonPressed(3)) {
            auto formula = glm::radians(0.022f * camera.sensitivity * 2.0f);
            camera.Look(glm::vec2(event.motion.xrel * formula, event.motion.yrel * formula));
        }
        else if (SDL_GetRelativeMouseMode() && Input::sIsButtonPressed(2))
            camera.Move(glm::vec2(event.motion.xrel * 0.02f, event.motion.yrel * 0.02f));
    }
    else if (event.type == SDL_MOUSEWHEEL && is_viewport_hovered)
        camera.Zoom(float(event.wheel.y));

    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            while (1) {
                SDL_Event ev;
                SDL_PollEvent(&ev);

                if (ev.window.event == SDL_WINDOWEVENT_RESTORED)
                    break;
            }
        }
     
        if (event.window.event == SDL_WINDOWEVENT_CLOSE)
            if (SDL_GetWindowID(m_Window) == event.window.windowID)
                m_Running = false;
        
        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            m_Renderer.CreateRenderTargets(m_Viewport);
    }

    for (const auto& widget : m_Widgets) {
        widget->onEvent(event);
    }
}

} // raekor