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
    // handle input
    InputHandler::handleEvents(this, mouseInViewport, dt);

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
        renderer.resize(viewport);
    }
}

} // raekor