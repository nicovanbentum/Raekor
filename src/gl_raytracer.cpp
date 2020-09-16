#include "pch.h"
#include "app.h"
#include "ecs.h"
#include "util.h"
#include "scene.h"
#include "mesh.h"
#include "entry.h"
#include "serial.h"
#include "camera.h"
#include "shader.h"
#include "script.h"
#include "framebuffer.h"
#include "platform/OS.h"
#include "renderer.h"
#include "buffer.h"
#include "timer.h"
#include "renderpass.h"
#include "gui.h"

namespace Raekor {

    void Editor::computeRayTracer() {
        // retrieve the application settings from the config file
        serializeSettings("config.json");

        int sdlError = SDL_Init(SDL_INIT_VIDEO);
        m_assert(sdlError == 0, "failed to init SDL for video");

        Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL |
            SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED;

        std::vector<SDL_Rect> displays;
        for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
            displays.push_back(SDL_Rect());
            SDL_GetDisplayBounds(i, &displays.back());
        }

        // if our display setting is higher than the nr of displays we pick the default display
        display = display > displays.size() - 1 ? 0 : display;
        auto sdlWindow = SDL_CreateWindow(name.c_str(),
            displays[display].x,
            displays[display].y,
            displays[display].w,
            displays[display].h,
            wflags);

        SDL_SetWindowInputFocus(sdlWindow);

        Viewport viewport = Viewport(displays[display]);

        // initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // create the renderer object that does sets up the API and does all the runtime magic
        Renderer::setAPI(RenderAPI::OPENGL);
        Renderer::Init(sdlWindow);

        viewport.size.x = 2003, viewport.size.y = 1370;

        // get GUI i/o and set a bunch of settings
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.ConfigDockingWithShift = true;

        // set UI font that's saved in config 
        ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 15.0f);
        if (!io.Fonts->Fonts.empty()) {
            io.FontDefault = io.Fonts->Fonts.back();
        }

        // load the UI's theme from config
        ImVec4* colors = ImGui::GetStyle().Colors;
        for (unsigned int i = 0; i < themeColors.size(); i++) {
            auto& savedColor = themeColors[i];
            ImGuiCol_Text;
            colors[i] = ImVec4(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
        }

        colors[ImGuiCol_DockingEmptyBg] = ImVec4(1, 0, 0, 1);

        ImGui::GetStyle().WindowRounding = 0.0f;
        ImGui::GetStyle().ChildRounding = 0.0f;
        ImGui::GetStyle().FrameRounding = 0.0f;
        ImGui::GetStyle().GrabRounding = 0.0f;
        ImGui::GetStyle().PopupRounding = 0.0f;
        ImGui::GetStyle().ScrollbarRounding = 0.0f;
        ImGui::GetStyle().WindowBorderSize = 0.0f;
        ImGui::GetStyle().ChildBorderSize = 0.0f;
        ImGui::GetStyle().FrameBorderSize = 0.0f;

        // timer for keeping frametime
        Timer deltaTimer;
        double deltaTime = 0;

        // setup the camera that acts as the sun's view (directional light)
        std::cout << "Creating render passes..." << std::endl;

        // all render passes
        auto rayTracePass = std::make_unique<RenderPass::RayComputePass>(viewport);


        // boolean settings needed for a couple things
        bool doSSAO = false, doBloom = false, debugVoxels = false, doDeferred = true;
        bool mouseInViewport = false, gizmoEnabled = false, showSettingsWindow = false;

        // keep a pointer to the texture that's rendered to the window
        unsigned int activeScreenTexture = rayTracePass->result;

        std::cout << "Initialization done." << std::endl;

        SDL_ShowWindow(sdlWindow);
        SDL_MaximizeWindow(sdlWindow);

        while (running) {
            deltaTimer.start();

            handleEvents(sdlWindow, viewport.getCamera(), mouseInViewport, deltaTime);
            viewport.getCamera().update(true);

            // clear the main window
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glViewport(0, 0, viewport.size.x, viewport.size.y);

            rayTracePass->execute(viewport);

            //get new frame for ImGui and ImGuizmo
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            Renderer::ImGuiNewFrame(sdlWindow);
            ImGuizmo::BeginFrame();

            if (ImGui::IsAnyItemActive()) {
                // perform input mapping
            }

            // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
            // because it would be confusing to have two docking targets within each others.
            ImGuiWindowFlags dockWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
            ImGuiViewport* imGuiViewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(imGuiViewport->Pos);
            ImGui::SetNextWindowSize(imGuiViewport->Size);
            ImGui::SetNextWindowViewport(imGuiViewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            dockWindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

            // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, 
            // so we ask Begin() to not render a background.
            ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
            if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) dockWindowFlags |= ImGuiWindowFlags_NoBackground;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("DockSpace", (bool*)true, dockWindowFlags);
            ImGui::PopStyleVar();
            ImGui::PopStyleVar(2);

            // DockSpace
            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
                ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
                ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
            }

            ImGui::Begin("Settings");
            ImGui::Button("Add sphere");
            ImGui::End();

            // renderer viewport
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            // figure out if we need to resize the viewport
            static bool resizing = false;
            auto size = ImGui::GetContentRegionAvail();
            if (viewport.size.x != size.x || viewport.size.y != size.y) {
                resizing = true;
                viewport.size.x = static_cast<uint32_t>(size.x), viewport.size.y = static_cast<uint32_t>(size.y);
            }
            auto pos = ImGui::GetWindowPos();

            // render the active screen texture to the view port as an imgui image
            ImGui::Image((void*)((intptr_t)activeScreenTexture), ImVec2((float)viewport.size.x, (float)viewport.size.y), { 0,1 }, { 1,0 });

            ImGui::End();
            ImGui::PopStyleVar();

            // application/render metrics
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGui::SetNextWindowPos(ImVec2(pos.x + size.x - size.x / 7.5f - 5.0f, pos.y + 5.0f));
            ImGui::SetNextWindowSize(ImVec2(size.x / 7.5f, size.y / 9.0f));
            auto metricWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;
            ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
            ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
            ImGui::Text("Product: %s", glGetString(GL_RENDERER));
            ImGui::Text("Resolution: %i x %i", viewport.size.x, viewport.size.y);
            ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::Text("Graphics API: OpenGL %s", glGetString(GL_VERSION));
            ImGui::End();


            ImGui::End();
            Renderer::ImGuiRender();
            Renderer::SwapBuffers(false);

            if (resizing) {
                // adjust the camera and gizmo
                viewport.getCamera().getProjection() = glm::perspectiveRH(glm::radians(viewport.getFov()), (float)viewport.size.x / (float)viewport.size.y, 0.1f, 10000.0f);
                ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

                rayTracePass->deleteResources();
                rayTracePass->createResources(viewport);

                resizing = false;
            }

            deltaTimer.stop();
            deltaTime = deltaTimer.elapsedMs();

        } // while true loop

        display = SDL_GetWindowDisplayIndex(sdlWindow);
        //clean up SDL
        SDL_DestroyWindow(sdlWindow);
        SDL_Quit();

    } // application::run

} // namespace Raekor  