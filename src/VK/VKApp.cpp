#include "pch.h"
#include "VKApp.h"

#include "gui.h"
#include "input.h"

namespace Raekor::VK {

    PathTracer::PathTracer() : 
    Application(RendererFlags::VULKAN), 
    renderer(window) 
{
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForVulkan(window);

    ImGui::GetIO().IniFilename = "";

    if (fs::exists(settings.defaultScene) && fs::path(settings.defaultScene).extension() == ".scene") {
        SDL_SetWindowTitle(window, std::string(settings.defaultScene + " - Raekor Renderer").c_str());
        scene.openFromFile(assets, settings.defaultScene);
    }

    assert(!scene.empty());

    auto meshes = scene.view<Mesh>();
    for (auto& [entity, mesh] : meshes.each()) {
        auto component = renderer.createBLAS(mesh);
        scene.emplace<VK::RTGeometry>(entity, component);
    }

    // TODO: Figure this mess out
    renderer.setVsync(settings.vsync);
    renderer.updateMaterials(assets, scene);
    renderer.updateAccelerationStructures(scene);
    renderer.initialize(scene);

    // gui stuff
    GUI::setTheme(settings.themeColors);

    std::puts("Job well done.");

    SDL_ShowWindow(window);
    SDL_SetWindowInputFocus(window);
}



void PathTracer::onUpdate(float dt) {
    if (Input::isButtonPressed(3)) {
        viewport.getCamera().strafeWASD(dt);
        renderer.resetAccumulation();
    }

    viewport.update();

    auto lightView = scene.view<DirectionalLight, Transform>();
    auto lookDirection = glm::vec3(0.25f, -0.9f, 0.0f);

    if (lightView.begin() != lightView.end()) {
        auto& lightTransform = lightView.get<Transform>(lightView.front());
        lookDirection = static_cast<glm::quat>(lightTransform.rotation) * lookDirection;
    } else {
        // we rotate default light a little or else we get nan values in our view matrix
        lookDirection = static_cast<glm::quat>(glm::vec3(glm::radians(15.0f), 0, 0)) * lookDirection;
    }

    lookDirection = glm::clamp(lookDirection, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });

    renderer.constants().lightDir = glm::vec4(lookDirection, 0);

    if (shouldRecreateSwapchain) {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        viewport.resize(glm::uvec2(w, h));
        renderer.recreateSwapchain(window);
        renderer.resetAccumulation();
        shouldRecreateSwapchain = false;
    }

    SDL_SetWindowTitle(window, std::string(std::to_string(dt) + " ms.").c_str());

    bool reset = false;

    GUI::beginFrame();

    bool open = true;
    ImGui::Begin("Path Trace Settings", &open, ImGuiWindowFlags_AlwaysAutoResize);
    
    if (ImGui::Checkbox("vsync", &settings.vsync)) {
        renderer.setVsync(settings.vsync);
        renderer.recreateSwapchain(window);
        renderer.resetAccumulation();
    }

    reset |= ImGui::SliderInt("Bounces", reinterpret_cast<int*>(&renderer.constants().bounces), 1, 8);
    reset |= ImGui::DragFloat("Sun Cone", &renderer.constants().sunConeAngle, 0.001f, 0.0f, 1.0f, "%.3f");

    ImGui::End();

    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0, 0, float(viewport.size.x), float(viewport.size.y));

    if (lightView.begin() != lightView.end()) {
        auto& lightTransform = lightView.get<Transform>(lightView.front());

        bool manipulated = ImGuizmo::Manipulate(
            glm::value_ptr(viewport.getCamera().getView()),
            glm::value_ptr(viewport.getCamera().getProjection()),
            ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::WORLD,
            glm::value_ptr(lightTransform.localTransform)
        );

        if (manipulated) {
            lightTransform.decompose();
        }

        reset |= manipulated;
    }

    GUI::endFrame();

    if (reset) {
        renderer.resetAccumulation();
    }

    renderer.render(window, viewport, scene);
}



void PathTracer::onEvent(const SDL_Event& ev) {
    ImGui_ImplSDL2_ProcessEvent(&ev);

    if (viewport.getCamera().onEventEditor(ev)) {
        renderer.resetAccumulation();
    }

    if (ev.type == SDL_WINDOWEVENT) {
        if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            while (1) {
                SDL_Event ev;
                SDL_PollEvent(&ev);

                if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
                    break;
                }
            }
        }
        if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
            if (SDL_GetWindowID(window) == ev.window.windowID) {
                running = false;
            }
        }
        if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
            shouldRecreateSwapchain = true;
        }
    }

    if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
        switch (ev.key.keysym.sym) {
        case SDLK_r: {
            renderer.reloadShaders();
            renderer.resetAccumulation();
        } break;
        }
    }
}



PathTracer::~PathTracer() {
    auto view = scene.view<VK::RTGeometry>();
    for (auto& [entity, geometry] : view.each()) {
        renderer.destroyBLAS(geometry);
    }
}

} // raekor