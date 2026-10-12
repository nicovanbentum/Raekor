#include "PCH.h"
#include "Game.h"
#include "OS.h"
#include "Input.h"
#include "Script.h"
#include "UIRenderer.h"
#include "DebugRenderer.h"
#include "GUIStyle.h"

namespace RK {

GameApp::GameApp(const Path& inSceneFile) :
    Game(WindowFlag::RESIZE),
    m_Scene(&m_RenderSystem),
    m_Physics(&m_RenderSystem),
    m_RenderSystem(this)
{
    LoadScripts();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = "";

    const float ui_scale = GUI::GetDisplayScale(m_Window);

    GUI::SetDarkTheme(ui_scale);

    if (!m_ConfigSettings.mFontFile.empty() && fs::exists(m_ConfigSettings.mFontFile))
        GUI::SetFont(m_ConfigSettings.mFontFile.string(), ui_scale);

    m_RenderSystem.InitImGui(m_Window);

    if (!inSceneFile.empty() && fs::exists(inSceneFile))
    {
        m_Scene.OpenFromFile(inSceneFile.string(), m_Assets, this);
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

    m_IgnoreTextInput = false;

    g_Input->SetBlocked(m_Console.IsOpen());

    // update relative mouse mode
    SDL_SetWindowRelativeMouseMode(m_Window, g_Input->IsRelativeMouseMode() && !m_Console.IsOpen());

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

    // update SoundSource, AudioListener and TriggerVolume components
    UpdateAudio(inDeltaTime);

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
    ImGui::Begin("Game", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs);

    auto cursor_pos = ImGui::GetCursorPos();
    
    ImGui::Image(m_RenderSystem.GetDisplayTexture(), ImVec2(width, height));
    
    ImGui::SetCursorPos(cursor_pos);
    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::End();
    ImGui::PopStyleVar(3);

    m_Console.Draw(inDeltaTime);

    ImGui::EndFrame();
    ImGui::Render();

    m_RenderSystem.OnRender(this, m_Scene, inDeltaTime);
}

void GameApp::OnEvent(const SDL_Event& inEvent)
{
    if (inEvent.type == SDL_EVENT_KEY_DOWN && inEvent.key.scancode == SDL_SCANCODE_GRAVE)
    {
        if (!inEvent.key.repeat)
            m_Console.Toggle();

        m_IgnoreTextInput = true;
        return;
    }

    if (inEvent.type == SDL_EVENT_TEXT_INPUT && m_IgnoreTextInput)
    {
        m_IgnoreTextInput = false;
        return;
    }

    if (m_Console.IsOpen() && inEvent.type == SDL_EVENT_KEY_DOWN && inEvent.key.key == SDLK_ESCAPE)
    {
        m_Console.Close();
        return;
    }

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

    bool is_input_event = false;

    switch (inEvent.type)
    {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_TEXT_EDITING:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
            is_input_event = true;
            break;
    }

    if (GetGameState() == GAME_RUNNING && !( m_Console.IsOpen() && is_input_event ))
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