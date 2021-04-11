#include "pch.h"
#include "editor.h"
#include "input.h"
#include "systems.h"
#include "platform/OS.h"
#include "cvars.h"

#include "gui/assetsWidget.h"
#include "gui/randomWidget.h"
#include "gui/menubarWidget.h"
#include "gui/consoleWidget.h"
#include "gui/viewportWidget.h"
#include "gui/inspectorWidget.h"
#include "gui/hierarchyWidget.h"

namespace Raekor
{

Editor::Editor() : 
    WindowApplication(RendererFlags::OPENGL), 
    renderer(window, viewport)
{
    // gui stuff
    gui::setFont(settings.font.c_str());
    gui::setTheme(settings.themeColors);

    if (std::filesystem::is_regular_file(settings.defaultScene)) {
        if (std::filesystem::path(settings.defaultScene).extension() == ".scene") {
            //SDL_SetWindowTitle(window, std::string(settings.defaultScene + " - Raekor Renderer").c_str());
            //scene.openFromFile(settings.defaultScene, assetManager);
        }
    }

    widgets.emplace_back(new AssetsWidget(this));
    widgets.emplace_back(new InspectorWidget(this));
    widgets.emplace_back(new HierarchyWidget(this));
    widgets.emplace_back(new MenubarWidget(this));
    widgets.emplace_back(new ConsoleWidget(this));
    widgets.emplace_back(new ViewportWidget(this));
    widgets.emplace_back(new RandomWidget(this));
    
    std::cout << "Initialization done." << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Editor::update(float dt) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        onEvent(event);
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (inAltMode) {
            viewport.getCamera().strafeMouse(event, dt);

        } else if (ImGui::IsMouseHoveringRect(ImVec(viewport.offset), ImVec(viewport.size), false)) {
            viewport.getCamera().onEventEditor(event);
        }

        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                while (1) {
                    SDL_Event ev;
                    SDL_PollEvent(&ev);

                    if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
                        break;
                    }
                }
            }
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (SDL_GetWindowID(window) == event.window.windowID) {
                    running = false;
                }
            }
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                renderer.createResources(viewport);
            }

        }
    }

    if (inAltMode) {
        viewport.getCamera().strafeWASD(dt);
    }

    // update transforms
    scene.updateTransforms();

    // update animations
    auto animationView = scene->view<ecs::MeshAnimationComponent>();
    std::for_each(std::execution::par_unseq, animationView.begin(), animationView.end(), [&](auto entity) {
        auto& animation = animationView.get<ecs::MeshAnimationComponent>(entity);
        animation.boneTransform(static_cast<float>(dt));
    });

    // update camera
    viewport.getCamera().update();

    // update scripts
    scene->view<ecs::NativeScriptComponent>().each([&](ecs::NativeScriptComponent& component) {
        if (component.script) {
            component.script->update(dt);
        }
    });

     if (active != entt::null && scene->has<ecs::MeshComponent>(active)) {
        auto& mesh = scene->get<ecs::MeshComponent>(active);
        auto& transform = scene->get<ecs::TransformComponent>(active);

        const auto min = mesh.aabb[0];
        const auto max = mesh.aabb[1];

        renderer.drawBox(min, max, transform.localTransform);
    }

    // render scene
    renderer.render(scene, viewport);

    //get new frame for ImGui and ImGuizmo
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    renderer.ImGui_NewFrame(window);
    ImGuizmo::BeginFrame();

    if (ImGui::IsAnyItemActive()) {
        // perform input mapping
    }

    // begin ImGui dockspace
    ImGuiWindowFlags dockWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* imGuiViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(imGuiViewport->Pos);
    ImGui::SetNextWindowSize(imGuiViewport->Size);
    ImGui::SetNextWindowViewport(imGuiViewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    dockWindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) {
        dockWindowFlags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", (bool*)true, dockWindowFlags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    // draw all the widgets
    for (auto widget : widgets) {
        if (widget->isVisible()) {
            widget->draw();
        }
    }

    // end ImGui dockspace
    ImGui::End();

    renderer.ImGui_Render();
    SDL_GL_SwapWindow(window);
}

void Editor::onEvent(const SDL_Event& event) {
    // free the mouse if the window loses focus
    auto flags = SDL_GetWindowFlags(window);
    if (!(flags & SDL_WINDOW_INPUT_FOCUS || flags & SDL_WINDOW_MINIMIZED) && inAltMode) {
        inAltMode = false;
        SDL_SetRelativeMouseMode(SDL_FALSE);
        return;
    }

    // key down and not repeating a hold
    if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        switch (event.key.keysym.sym) {
            case SDLK_LALT: {
                inAltMode = !inAltMode;
                SDL_SetRelativeMouseMode(static_cast<SDL_bool>(inAltMode));
            };
            case SDLK_DELETE: {
                if (active != entt::null) {
                    if (scene->has<ecs::NodeComponent>(active)) {

                        auto childTree = NodeSystem::getFlatHierarchy(scene, scene->get<ecs::NodeComponent>(active));

                        for (auto entity : childTree) {
                            NodeSystem::remove(scene, scene->get<ecs::NodeComponent>(entity));
                            scene->destroy(entity);
                        }

                        NodeSystem::remove(scene, scene->get<ecs::NodeComponent>(active));
                        scene->destroy(active);
                    } else {
                        scene->destroy(active);
                    }

                    active = entt::null;
                }
            };
            case SDLK_c: {
                if (SDL_GetModState() & KMOD_LCTRL) {
                    auto copy = scene->create();

                    scene->visit(active, [&](const entt::id_type id) {
                        for_each_tuple_element(ecs::Components, [&](auto component) {
                            using type = decltype(component)::type;
                            if (id == entt::type_info<type>::id()) {
                                ecs::clone<type>(scene, active, copy);
                            }
                        });
                    });
                }
            };
        }
    }
}

} // raekor