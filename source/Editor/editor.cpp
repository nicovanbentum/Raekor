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
    renderer(window, m_Viewport) 
{
    GUI::setTheme(settings.themeColors);
    GUI::setFont(settings.font.c_str());

    scene.SetUploadMeshCallbackFunction(GLRenderer::uploadMeshBuffers);
    scene.SetUploadMaterialCallbackFunction(GLRenderer::uploadMaterialTextures);

    if (fs::exists(settings.defaultScene) && fs::path(settings.defaultScene).extension() == ".scene") {
        SDL_SetWindowTitle(window, std::string(settings.defaultScene + " - Raekor Renderer").c_str());
        scene.openFromFile(assets, settings.defaultScene);
    }

    widgets.emplace_back(std::make_shared<AssetsWidget>(this));
    widgets.emplace_back(std::make_shared<RandomWidget>(this));
    widgets.emplace_back(std::make_shared<MenubarWidget>(this));
    widgets.emplace_back(std::make_shared<ConsoleWidget>(this));
    widgets.emplace_back(std::make_shared<MetricsWidget>(this));
    widgets.emplace_back(std::make_shared<NodeGraphWidget>(this));
    widgets.emplace_back(std::make_shared<ViewportWidget>(this));
    widgets.emplace_back(std::make_shared<InspectorWidget>(this));
    widgets.emplace_back(std::make_shared<HierarchyWidget>(this));

    std::cout << "Initialization done.\n";
    auto sink = scene.on_destroy<Mesh>();
}



void Editor::onUpdate(float dt) {
    assets.CollectGarbage();

    // update the camera
    m_Viewport.OnUpdate(dt);

    // update scene components
    scene.updateTransforms();
    scene.updateLights();

    // update animations in parallel
    scene.view<Skeleton>().each([&](Skeleton& skeleton) {
        Async::dispatch([&]() {
            skeleton.UpdateFromAnimation(skeleton.animations[0], dt);
        });
    });

    Async::wait();

    // update scripts
    scene.view<NativeScript>().each([&](NativeScript& component) {
        if (component.script) {
            try {
                component.script->update(dt);
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
            }
        }
    });

    // draw the bounding box of the active entity
    if (active != entt::null && scene.all_of<Mesh>(active)) {
        const auto& [mesh, transform] = scene.get<Mesh, Transform>(active);
        renderer.addDebugBox(mesh.aabb[0], mesh.aabb[1], transform.worldTransform);
    }

    // start ImGui
    GUI::beginFrame();
    GUI::beginDockSpace();

    // draw widgets
    for (const auto& widget : widgets) {
        if (widget->IsVisible()) {
            widget->draw(dt);
        }
    }

    // end ImGui
    GUI::endDockSpace();
    GUI::endFrame();

    // render scene
    renderer.render(scene, m_Viewport);

    // swap the backbuffer
    SDL_GL_SwapWindow(window);
}



void Editor::onEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    const bool is_viewport_hovered = GetWidget<ViewportWidget>()->IsHovered();

    // free the mouse if the window loses focus
    auto flags = SDL_GetWindowFlags(window);
    if (!(flags & SDL_WINDOW_INPUT_FOCUS || flags & SDL_WINDOW_MINIMIZED)) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        return;
    }

    if (event.button.button == 2 || event.button.button == 3) {
        if (event.type == SDL_MOUSEBUTTONDOWN && is_viewport_hovered) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        else if (event.type == SDL_MOUSEBUTTONUP) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
    }

    if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        switch (event.key.keysym.sym) {
            case SDLK_DELETE: {
                if (active != entt::null) {
                    if (scene.all_of<Node>(active)) {
                        auto tree = NodeSystem::getFlatHierarchy(scene, scene.get<Node>(active));
                        for (auto entity : tree) {
                            NodeSystem::remove(scene, scene.get<Node>(entity));
                            scene.destroy(entity);
                        }

                        NodeSystem::remove(scene, scene.get<Node>(active));
                    }

                    scene.destroy(active);
                    active = entt::null;
                }
            } break;
            case SDLK_d: {
                if (SDL_GetModState() & KMOD_LCTRL) {
                    auto copy = scene.create();

                    scene.visit(active, [&](const entt::type_info info) {
                        for_each_tuple_element(Components, [&](auto component) {
                            using ComponentType = decltype(component)::type;
                            if (info.seq() == entt::type_seq<ComponentType>()) {
                                clone<ComponentType>(scene, active, copy);
                            }
                        });
                    });

                    if (auto mesh = scene.try_get<Mesh>(copy); mesh) {
                        GLRenderer::uploadMeshBuffers(*mesh);
                    }
                }
            } break;
        }
    }

    auto& camera = m_Viewport.GetCamera();

    if (event.type == SDL_MOUSEMOTION) {
        if (SDL_GetRelativeMouseMode() && Input::isButtonPressed(3)) {
            auto formula = glm::radians(0.022f * camera.sensitivity * 2.0f);
            camera.Look(glm::vec2(event.motion.xrel * formula, event.motion.yrel * formula));
        }
        else if (SDL_GetRelativeMouseMode() && Input::isButtonPressed(2)) {
            camera.Move(glm::vec2(event.motion.xrel * 0.02f, event.motion.yrel * 0.02f));
        }
    }
    else if (event.type == SDL_MOUSEWHEEL && is_viewport_hovered) {
        camera.Zoom(float(event.wheel.y));
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
                m_Running = false;
            }
        }
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            renderer.createRenderTargets(m_Viewport);
        }
    }

    for (const auto& widget : widgets) {
        widget->onEvent(event);
    }
}

} // raekor