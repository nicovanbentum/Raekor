#include "pch.h"
#include "VKApp.h"

#include "Raekor/OS.h"
#include "Raekor/gui.h"
#include "Raekor/input.h"
#include "Raekor/timer.h"

namespace Raekor::VK {

    PathTracer::PathTracer() : 
    Application(WindowFlag::VULKAN | WindowFlag::RESIZE), 
    m_Renderer(m_Window),
    m_Scene(nullptr)
{
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForVulkan(m_Window);

    ImGui::GetIO().IniFilename = "";

    if (fs::exists(m_Settings.mSceneFile)) {
        SDL_SetWindowTitle(m_Window, std::string(m_Settings.mSceneFile + " - Raekor Renderer").c_str());
        m_Scene.OpenFromFile(m_Assets, m_Settings.mSceneFile);
    }
    else {
        std::string filepath;

        while (filepath.empty()) {
            filepath = OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0");
            if (!filepath.empty()) {
                SDL_SetWindowTitle(m_Window, std::string(filepath + " - Raekor Renderer").c_str());
                m_Scene.OpenFromFile(m_Assets, filepath);
                break;
            }
        }
    }

    assert(!m_Scene.IsEmpty() && "Scene cannot be empty when starting up the Vulkan path tracer!!");

    m_Scene.UpdateTransforms();

    for (const auto& [entity, mesh] : m_Scene.Each<Mesh>()) {
        if (mesh.material != NULL_ENTITY && (mesh.positions.size() > 0 && mesh.indices.size() > 0)) {
            auto component = m_Renderer.CreateBLAS(mesh, m_Scene.Get<Material>(mesh.material));
            m_Scene.Add<VK::RTGeometry>(entity, component);
        }
    }

    // TODO: Figure this mess out
    m_Renderer.SetSyncInterval(m_Settings.mVsyncEnabled);
    m_Renderer.UpdateMaterials(m_Assets, m_Scene);
    m_Renderer.UpdateBVH(m_Scene);
    m_Renderer.Init(m_Scene);
    
    // saves a bit of memory at runtime, as we no longer need the textures on the CPU after uploading them to the GPU
    m_Assets.clear();

    // gui stuff
    GUI::SetTheme();

    std::cout << "Job well done.\n";

    SDL_ShowWindow(m_Window);
    SDL_SetWindowInputFocus(m_Window);

    m_Viewport.SetSize({ 1300, 1300 });
    SDL_SetWindowSize(m_Window, 1300, 1300);

    m_Viewport.SetFieldOfView(65.0f);
}



void PathTracer::OnUpdate(float dt) {
    if (SDL_GetRelativeMouseMode())
        m_Renderer.ResetAccumulation();

    m_Viewport.OnUpdate(dt);

    auto& sun_direction = m_Renderer.GetPushConstants().lightDir;
    sun_direction = glm::vec4(m_Scene.GetSunLightDirection(), sun_direction.w);

    if (m_IsSwapchainDirty) {
        int w, h;
        SDL_GetWindowSize(m_Window, &w, &h);

        m_Viewport.SetSize(glm::uvec2(w, h));
        m_Renderer.RecreateSwapchain(m_Window);
        m_Renderer.ResetAccumulation();
        m_IsSwapchainDirty = false;
    }

    SDL_SetWindowTitle(m_Window, std::string(std::to_string(Timer::sToMilliseconds(dt)) + " ms.").c_str());

    bool reset = false;

    GUI::BeginFrame();

    if (m_IsImGuiEnabled) {
        ImGui::Begin("Path Trace Settings", &m_IsImGuiEnabled, ImGuiWindowFlags_AlwaysAutoResize);
    
        ImGui::Text("F2 - Screenshot");

        if (ImGui::Checkbox("vsync", &m_Settings.mVsyncEnabled)) {
            m_Renderer.SetSyncInterval(m_Settings.mVsyncEnabled);
            m_Renderer.RecreateSwapchain(m_Window);
            m_Renderer.ResetAccumulation();
        }

        auto& push_constants = m_Renderer.GetPushConstants();

        auto bounces = int(m_Renderer.GetPushConstants().bounces - 1);
        if (ImGui::SliderInt("Bounces", &bounces, 0, 7)) {
            m_Renderer.GetPushConstants().bounces = bounces + 1;
            reset = true;
        }

        reset |= ImGui::DragFloat("Sun Strength", &push_constants.lightDir.w, 0.1f, 0.0f, 100.0f, "%.1f");
        reset |= ImGui::DragFloat("Sun Cone", &m_Renderer.GetPushConstants().sunConeAngle, 0.001f, 0.0f, 1.0f, "%.3f");

        auto field_of_view = m_Viewport.GetFieldOfView();
        if (ImGui::DragFloat("Camera Fov", &field_of_view, 0.1f, 65.0f, 120.0f, "%.3f")) {
            m_Viewport.SetFieldOfView(field_of_view);
            reset = true;
        }

        ImGui::Text("Sample Count: %i", (int)m_Renderer.GetFrameCounter());

        ImGui::End();

        ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
        ImGuizmo::SetRect(0, 0, float(m_Viewport.GetSize().x), float(m_Viewport.GetSize().y));

        if (auto sunlight = m_Scene.GetSunLight()) {
            // if we have a sunlight it means at least 1 entity has a DirectionalLight,  so [0] access is safe
            auto& transform = m_Scene.Get<Transform>(m_Scene.GetEntities<DirectionalLight>()[0]);

            bool manipulated = ImGuizmo::Manipulate(
                glm::value_ptr(m_Viewport.GetCamera().GetView()),
                glm::value_ptr(m_Viewport.GetCamera().GetProjection()),
                ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::WORLD,
                glm::value_ptr(transform.localTransform)
            );

            if (manipulated)
                transform.Decompose();

            reset |= manipulated;
        }
    }

    GUI::EndFrame();

    if (reset)
        m_Renderer.ResetAccumulation();

    m_Renderer.RenderScene(m_Window, m_Viewport, m_Scene);
}



void PathTracer::OnEvent(const SDL_Event& inEvent) {
    ImGui_ImplSDL2_ProcessEvent(&inEvent);

    if (!ImGui::GetIO().WantCaptureMouse) {
        if (CameraController::OnEvent(m_Viewport.GetCamera(), inEvent))
            m_Renderer.ResetAccumulation();
    }

    if (SDL_GetRelativeMouseMode())
        m_Renderer.ResetAccumulation();

    if (inEvent.window.event == SDL_WINDOWEVENT_RESIZED)
        m_IsSwapchainDirty = true;

    if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat) {
        switch (inEvent.key.keysym.sym) {
            case SDLK_r: {
                m_Renderer.ReloadShaders();
                m_Renderer.ResetAccumulation();
            } break;
            case SDLK_TAB: {
                m_IsImGuiEnabled = !m_IsImGuiEnabled;
            } break;
            case SDLK_F2: {
                std::string path = OS::sSaveFileDialog("Uncompressed PNG (*.png)\0", "png");

                if (!path.empty())
                    m_Renderer.Screenshot(path);
            } break;
        }
    }
}



PathTracer::~PathTracer() {
    for (const auto& [entity, geometry] : m_Scene.Each<VK::RTGeometry>())
        m_Renderer.DestroyBLAS(geometry);
}

} // raekor