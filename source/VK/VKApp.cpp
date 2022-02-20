#include "pch.h"
#include "VKApp.h"

#include "Raekor/OS.h"
#include "Raekor/gui.h"
#include "Raekor/input.h"

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

    if (fs::exists(settings.defaultScene)) {
        SDL_SetWindowTitle(window, std::string(settings.defaultScene + " - Raekor Renderer").c_str());
        scene.openFromFile(assets, settings.defaultScene);
    }
    else {
        std::string filepath;

        while (filepath.empty()) {
            filepath = OS::openFileDialog("Scene Files (*.scene)\0*.scene\0");
            if (!filepath.empty()) {
                SDL_SetWindowTitle(window, std::string(filepath + " - Raekor Renderer").c_str());
                scene.openFromFile(assets, filepath);
                break;
            }
        }
    }

    assert(!scene.empty() && "Scene cannot be empty when starting up the Vulkan path tracer!!");

    auto meshes = scene.view<Mesh>();
    for (auto& [entity, mesh] : meshes.each()) {
        auto component = renderer.createBLAS(mesh);
        scene.emplace<VK::RTGeometry>(entity, component);
    }

    assets.clear();

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

    SDL_SetWindowSize(window, 1300, 1300);
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

        m_Viewport.Resize(glm::uvec2(w, h));
        renderer.recreateSwapchain(window);
        renderer.resetAccumulation();
        shouldRecreateSwapchain = false;
    }

    SDL_SetWindowTitle(window, std::string(std::to_string(dt) + " ms.").c_str());

    bool reset = false;

    GUI::beginFrame();

    if (isImGuiEnabled) {
        ImGui::Begin("Path Trace Settings", &isImGuiEnabled, ImGuiWindowFlags_AlwaysAutoResize);
    
        ImGui::Text("F2 - Screenshot");

        if (ImGui::Checkbox("vsync", &settings.vsync)) {
            renderer.setVsync(settings.vsync);
            renderer.recreateSwapchain(window);
            renderer.resetAccumulation();
        }

        reset |= ImGui::SliderInt("Bounces", reinterpret_cast<int*>(&renderer.constants().bounces), 1, 8);
        reset |= ImGui::DragFloat("Sun Cone", &renderer.constants().sunConeAngle, 0.001f, 0.0f, 1.0f, "%.3f");

        ImGui::End();

        ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
        ImGuizmo::SetRect(0, 0, float(m_Viewport.size.x), float(m_Viewport.size.y));

        if (lightView.begin() != lightView.end()) {
            auto& lightTransform = lightView.get<Transform>(lightView.front());

            bool manipulated = ImGuizmo::Manipulate(
                glm::value_ptr(m_Viewport.GetCamera().GetView()),
                glm::value_ptr(m_Viewport.GetCamera().GetProjection()),
                ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::WORLD,
                glm::value_ptr(lightTransform.localTransform)
            );

            if (manipulated) {
                lightTransform.decompose();
            }

            reset |= manipulated;
        }
    }

    GUI::endFrame();

    if (reset) {
        renderer.resetAccumulation();
    }

    renderer.render(window, m_Viewport, scene);
}



void PathTracer::onEvent(const SDL_Event& ev) {
    ImGui_ImplSDL2_ProcessEvent(&ev);

    if (Input::isButtonPressed(3)) {
        viewport.getCamera().strafeMouse(ev);
        renderer.resetAccumulation();
    }
    else if(viewport.getCamera().onEventEditor(ev)) {
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
                m_Running = false;
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
            case SDLK_F2: {
                std::string path = OS::saveFileDialog("Uncompressed PNG (*.png)\0", "png");

                if (!path.empty()) {
                    renderer.screenshot(path);
                }
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