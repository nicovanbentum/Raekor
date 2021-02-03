#include "pch.h"
#include "editor.h"
#include "gui.h"
#include "input.h"
#include "systems.h"
#include "platform/OS.h"

namespace Raekor {

EditorOpenGL::EditorOpenGL() : WindowApplication(RendererFlags::OPENGL), renderer(window, viewport) {
    // gui stuff
    gui::setFont(settings.font.c_str());
    gui::setTheme(settings.themeColors);

    // keep a pointer to the texture that's rendered to the window
    std::cout << "Initialization done." << std::endl;

    if (std::filesystem::is_regular_file(settings.defaultScene)) {
        if (std::filesystem::path(settings.defaultScene).extension() == ".scene") {
            SDL_SetWindowTitle(window, std::string(settings.defaultScene + " - Raekor Renderer").c_str());
            scene.openFromFile(settings.defaultScene);
        }
    }

    viewportWindow.setTexture(renderer.tonemappingPass->result);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void EditorOpenGL::update(float dt) {
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
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (SDL_GetWindowID(window) == event.window.windowID) {
                    running = false;
                }
            }
            if (event.window.event = SDL_WINDOWEVENT_RESIZED) {
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
    viewport.getCamera().update(true);

    // update scripts
    scene->view<ecs::NativeScriptComponent>().each([&](ecs::NativeScriptComponent& component) {
        if (component.script) {
            component.script->update(dt);
        }
    });

    // render scene
    renderer.render(scene, viewport, active);

    //get new frame for ImGui and ImGuizmo
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    renderer.ImGui_NewFrame(window);
    ImGuizmo::BeginFrame();

    if (ImGui::IsAnyItemActive()) {
        // perform input mapping
    }

    dockspace.begin();

    topMenuBar.draw(this, scene, renderer, active);
    
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete), true)) {
        if (active != entt::null) {
            if (scene->has<ecs::NodeComponent>(active)) {

                auto childTree = NodeSystem::getTree(scene, scene->get<ecs::NodeComponent>(active));

                for (auto entity : childTree) {
                    NodeSystem::remove(scene, scene->get<ecs::NodeComponent>(entity));
                    scene->destroy(entity);
                }

                NodeSystem::remove(scene, scene->get<ecs::NodeComponent>(active));
                scene->destroy(active);
            }
            else {
                scene->destroy(active);
            }

            active = entt::null;
        }
    }

    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C), true)) {
        if (SDL_GetModState() & KMOD_LCTRL) {
            auto newEntity = scene->create();

            scene->visit(active, [&](const auto& component) {
                auto clone_function = ecs::cloner::getSingleton()->getFunction(component);
                if (clone_function) {
                    clone_function(scene, active, newEntity);
                }
            });
        }
    }

    assetBrowser.drawWindow(scene, active);

    inspectorWindow.draw(scene, active);

    ecsWindow.draw(scene, active);

    // post processing panel
    postprocessWindow.drawWindow(renderer);

    // scene panel
    randomWindow.drawWindow(renderer);

    cameraWindow.drawWindow(viewport.getCamera());

    gizmo.drawWindow();

    viewportWindow.setTexture(renderer.tonemappingPass->result);

    bool resized = viewportWindow.draw(viewport, renderer, scene, active);

    dockspace.end();

    renderer.ImGui_Render();
    SDL_GL_SwapWindow(window);

    if (resized) {
        renderer.createResources(viewport);
    }

    float pixel;
    //glGetTextureSubImage(renderer.geometryBufferPass->entityTexture, 0, 1, 1,
     //   0, 1, 1, 1, GL_RED, GL_FLOAT, sizeof(float), &pixel);
}

void EditorOpenGL::onEvent(const SDL_Event& event) {
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
        }
    }
}

} // raekor