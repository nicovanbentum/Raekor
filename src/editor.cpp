#include "pch.h"
#include "editor.h"
#include "input.h"
#include "systems.h"
#include "platform/OS.h"
#include "cvars.h"

#include "gui/assetsWidget.h"
#include "gui/randomWidget.h"
#include "gui/metricsWidget.h"
#include "gui/menubarWidget.h"
#include "gui/consoleWidget.h"
#include "gui/viewportWidget.h"
#include "gui/inspectorWidget.h"
#include "gui/hierarchyWidget.h"

namespace Raekor {

Editor::Editor() :
    WindowApplication(RendererFlags::OPENGL),
    renderer(window, viewport) {
    // gui stuff
    gui::setFont(settings.font.c_str());
    gui::setTheme(settings.themeColors);

    if (fs::exists(settings.defaultScene) && fs::path(settings.defaultScene).extension() == ".scene") {
        SDL_SetWindowTitle(window, std::string(settings.defaultScene + " - Raekor Renderer").c_str());
        scene.openFromFile(assets, settings.defaultScene);
    }

    widgets.emplace_back(std::make_shared<ConsoleWidget>(this));
    widgets.emplace_back(std::make_shared<AssetsWidget>(this));
    widgets.emplace_back(std::make_shared<InspectorWidget>(this));
    widgets.emplace_back(std::make_shared<HierarchyWidget>(this));
    widgets.emplace_back(std::make_shared<MenubarWidget>(this));
    widgets.emplace_back(std::make_shared<RandomWidget>(this));
    widgets.emplace_back(std::make_shared<ViewportWidget>(this));

    std::cout << "Initialization done." << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Editor::update(float dt) {
    // poll and handle input
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        onEvent(event);
    }

    if (inAltMode) {
        viewport.getCamera().strafeWASD(dt);
    }

    // update the camera
    viewport.getCamera().update();

    // update scene components
    scene.updateTransforms();
    scene.updateLights();

    // update animations in parallel
    scene.view<Skeleton>().each([&](Skeleton& skeleton) {
        Async::dispatch([&]() {
            skeleton.boneTransform(dt);
        });
    });

    Async::wait();
    
    // update scripts
    scene.view<NativeScript>().each([&](NativeScript& component) {
        if (component.script) {
            component.script->update(dt);
        }
    });

    // draw the bounding box of the active entity
    if (active != entt::null && scene.all_of<Mesh>(active)) {
        auto& mesh = scene.get<Mesh>(active);
        auto& transform = scene.get<Transform>(active);

        const auto min = mesh.aabb[0];
        const auto max = mesh.aabb[1];

        renderer.drawBox(min, max, transform.worldTransform);
    }

    // render scene
    renderer.render(scene, viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderer.ImGui_NewFrame(window);

    {
        // draw ImGui
        gui::ScopedDockSpace dockspace;

        for (auto widget : widgets) {
            if (widget->isVisible()) {
                widget->draw();
            }
        }
    }

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
            } break;
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
                }
            } break;
        }
    }

    if (inAltMode) {
        viewport.getCamera().strafeMouse(event);
    }
    else if (ImGui::IsMouseHoveringRect(ImVec(viewport.offset), ImVec(viewport.size), false)) {
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
            renderer.createRenderTargets(viewport);
        }
    }

    if (ImGui::IsAnyItemActive()) {
        for (const auto& widget : widgets) {
            if (widget->isFocused()) {
                widget->onEvent(event);
            }
        }
    }
}

} // raekor