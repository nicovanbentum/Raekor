#include "pch.h"
#include "VKEntry.h"
#include "gui.h"

namespace Raekor {

VulkanPathTracer::VulkanPathTracer() : WindowApplication(RendererFlags::VULKAN), renderer(window) {
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (fs::exists(settings.defaultScene) && fs::path(settings.defaultScene).extension() == ".scene") {
        SDL_SetWindowTitle(window, std::string(settings.defaultScene + " - Raekor Renderer").c_str());
        scene.openFromFile(assets, settings.defaultScene);
    }

    auto meshes = scene.view<Mesh>();
    for (auto& [entity, mesh] : meshes.each()) {
        auto component = renderer.createBLAS(mesh);
        scene.emplace<VK::RTGeometry>(entity, component);
    }

    // TODO: Figure this mess out
    renderer.updateAccelerationStructures(scene);
    renderer.updateMaterials(assets, scene);
    renderer.initialize(scene);

    // gui stuff
    gui::setTheme(settings.themeColors);

    std::puts("Job well done.");

    SDL_ShowWindow(window);
    SDL_SetWindowInputFocus(window);
}



void VulkanPathTracer::update(float dt) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        onEvent(ev);
        ImGui_ImplSDL2_ProcessEvent(&ev);

        viewport.getCamera().onEventEditor(ev);

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
            } break;
            }
        }
    }

    viewport.getCamera().update();

    if (shouldRecreateSwapchain) {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        viewport.resize(glm::uvec2(w, h));
        renderer.recreateSwapchain(useVsync);
        shouldRecreateSwapchain = false;
    }

    SDL_SetWindowTitle(window, std::string(std::to_string(dt) + " ms.").c_str());

    renderer.render(viewport, scene);
}



VulkanPathTracer::~VulkanPathTracer() {
    auto view = scene.view<VK::RTGeometry>();
    for (auto& [entity, geometry] : view.each()) {
        renderer.destroyBLAS(geometry);
    }
}

} // raekor