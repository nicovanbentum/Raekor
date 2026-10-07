#include "PCH.h"
#include "Game.h"
#include "OS.h"
#include "Input.h"
#include "Script.h"
#include "UIRenderer.h"
#include "DebugRenderer.h"

namespace RK {

GameApp::GameApp() :
    Game(WindowFlag::RESIZE),
    m_Scene(&m_RenderSystem),
    m_Physics(&m_RenderSystem),
    m_RenderSystem(this)
{
    LoadScripts();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = "";
    ImGui::GetStyle().ScaleAllSizes(1.33333333f);

    m_RenderSystem.InitImGui(m_Window);

    const String scene_override = OS::sGetCommandLineValue("-scene");
    const Path scene_file = scene_override.empty() ? m_ConfigSettings.mSceneFile : Path(scene_override);

    if (!scene_file.empty() && fs::exists(scene_file))
    {
        m_Scene.OpenFromFile(scene_file.string(), m_Assets, this);
    }

    Game::Start();
}

GameApp::~GameApp()
{
    Game::Stop();
    UnloadScripts();
}

void GameApp::OnUpdate(float inDeltaTime)
{
    // clear all profile sections
    g_Profiler->Reset();

    PROFILE_FUNCTION_CPU();

    // Clear UI draw command buffers
    g_UIRenderer.Reset();

    // clear the debug renderer vertex buffers
    g_DebugRenderer.Reset();

    // update relative mouse mode
    SDL_SetWindowRelativeMouseMode(m_Window, g_Input->IsRelativeMouseMode());

    // update the physics system
    m_Physics.OnUpdate(m_Scene);

    // step the physics simulation
    m_Physics.Step(m_Scene, inDeltaTime);

    // update Transform components
    m_Scene.UpdateTransforms();

    // update camera transforms
    m_Scene.UpdateCameras();

    // update Light and DirectionalLight components
    m_Scene.UpdateLights();

    if (m_CameraEntity != Entity::Null)
    {
        m_Viewport.OnUpdate(m_Scene.Get<Camera>(m_CameraEntity));
    }
    else // if the game has not taken over the camera, use the editor controls
    {
        m_Viewport.OnUpdate(m_Camera);
        if (m_GameState != GAME_RUNNING)
            EditorCameraController::OnUpdate(m_Camera, inDeltaTime);
    }

    // update Skeleton and Animation components
    m_Scene.UpdateAnimations(inDeltaTime);

    ReloadScriptsIfChanged(inDeltaTime);

    // update NativeScript components
    if (GetGameState() == GAME_RUNNING)
        m_Scene.UpdateNativeScripts(inDeltaTime, this);

    int width, height;
    SDL_GetWindowSize(m_Window, &width, &height);

    m_Viewport.SetRenderSize({ width, height });
    m_Viewport.SetDisplaySize({ width, height });

    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("Game", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

    auto cursor_pos = ImGui::GetCursorPos();
    
    ImGui::Image(m_RenderSystem.GetDisplayTexture(), ImVec2(width, height));
    
    ImGui::SetCursorPos(cursor_pos);
    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::End();
    ImGui::PopStyleVar(3);

    ImGui::EndFrame();
    ImGui::Render();

    m_RenderSystem.OnRender(this, m_Scene, inDeltaTime);
}

void GameApp::OnEvent(const SDL_Event& inEvent)
{
    ImGui_ImplSDL3_ProcessEvent(&inEvent);

    if (inEvent.type == SDL_EVENT_WINDOW_MINIMIZED)
    {
        for (;;)
        {
            SDL_Event temp_event;
            SDL_PollEvent(&temp_event);

            if (temp_event.type == SDL_EVENT_WINDOW_RESTORED)
                break;
        }
    }

    if (inEvent.type == SDL_EVENT_WINDOW_RESIZED)
    {
        if (!m_ConfigSettings.mShowUI)
        {
            int w, h;
            SDL_GetWindowSize(m_Window, &w, &h);
            m_Viewport.SetDisplaySize(UVec2(w, h));
        }
    }

    if (inEvent.type == SDL_EVENT_KEY_DOWN && !inEvent.key.repeat)
    {
        switch (inEvent.key.key)
        {
            case SDLK_ESCAPE: 
            {
                if (m_GameState == GAME_RUNNING)
                    Game::Pause();
                else if (m_GameState == GAME_PAUSED)
                    Game::Unpause();
            } break;
        }

        // ALT + ENTER event (Windowed <-> Fullscreen toggle)
        if (inEvent.key.key == SDLK_RETURN && SDL_GetModState() & SDL_KMOD_LALT)
        {
            // This only toggles between windowed and borderless fullscreen, exclusive fullscreen needs to be set from the menu
            if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN)
                SDL_SetWindowFullscreen(m_Window, false);
            else
                SDL_SetWindowFullscreen(m_Window, true);

            // Updat the viewport and tell the renderer to resize to the viewport
            m_RenderSystem.GetRenderer().SetShouldResize(true);
        }
    }

    if (GetGameState() == GAME_RUNNING)
    {
        for (auto [entity, script] : m_Scene.Each<NativeScript>())
        {
            if (script.script)
            {
                try
                {
                    script.script->OnEvent(inEvent);
                }
                catch (const std::exception& e)
                {
                    gLogError("Script", "{}", e.what());
                }
            }
        }
    }

    if (inEvent.type == SDL_EVENT_WINDOW_RESIZED)
        m_RenderSystem.GetRenderer().SetShouldResize(true);
}

} // RK