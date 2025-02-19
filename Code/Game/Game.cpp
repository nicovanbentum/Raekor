#include "PCH.h"
#include "Game.h"
#include "Input.h"
#include "Script.h"
#include "UIRenderer.h"
#include "DebugRenderer.h"
#include "Scripts/Scripts.h"
#include "Renderer/Shader.h"
#include "Renderer/Renderer.h"
#include "Renderer/GPUProfiler.h"

using namespace RK::DX12;

extern float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(int pixel_i, int pixel_j, int sampleIndex, int sampleDimension);

namespace RK {

GameApp::GameApp() :
    Game(WindowFlag::RESIZE),
    m_Device(this),
    m_Scene(&m_RenderInterface),
    m_Physics(&m_RenderInterface),
    m_Renderer(m_Device, m_Viewport, m_Window),
    m_RayTracedScene(m_Scene),
    m_RenderInterface(this, m_Device, m_Renderer)
{
    gRegisterScriptTypes();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = "";
    ImGui::GetStyle().ScaleAllSizes(1.33333333f);

    DX12::g_GPUProfiler = new DX12::GPUProfiler(m_Device);

    g_RTTIFactory.Register(RTTI_OF<DX12::ComputeProgram>());
    g_RTTIFactory.Register(RTTI_OF<DX12::GraphicsProgram>());
    g_RTTIFactory.Register(RTTI_OF<DX12::SystemShadersDX12>());

    Timer timer;

    JSON::ReadArchive read_archive("Assets\\Shaders\\Backend\\Shaders.json");
    read_archive >> DX12::g_SystemShaders;

    // compile shaders
    DX12::g_SystemShaders.OnCompile(m_Device);
    g_ThreadPool.WaitForJobs();

    if (!DX12::g_SystemShaders.IsCompiled())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DX12 Error", "Failed to compile system shaders", m_Window);
        std::abort();
    }

    // Creating the SRVs at heap index 0 results in a 4x4 black square in the top left of the texture,
   // this is a hacky workaround. at least we get the added benefit of 0 being an 'invalid' index :D
    (void)m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Add(nullptr);

    static Array<Vec4> blue_noise_samples;
    blue_noise_samples.reserve(128 * 128);

    for (int y = 0; y < 128; y++)
    {
        for (int x = 0; x < 128; x++)
        {
            Vec4& sample = blue_noise_samples.emplace_back();

            for (int i = 0; i < sample.length(); i++)
                sample[i] = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, i);
        }
    }

    TextureID bluenoise_texture = m_Device.CreateTexture(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width = 128,
            .height = 128,
            .usage = Texture::Usage::SHADER_READ_ONLY,
            .debugName = "BlueNoise128x1spp"
        });

    assert(m_Device.GetBindlessHeapIndex(bluenoise_texture) == BINDLESS_BLUE_NOISE_TEXTURE_INDEX);

    m_Device.UploadTextureData(m_Device.GetTexture(bluenoise_texture), 0, 0, sizeof(Vec4) * 128, blue_noise_samples.data());

    LogMessage(std::format("[CPU] Shader compilation took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // Create default textures / assets
    const String black_texture_file = "Assets/black4x4.dds";
    const String white_texture_file = "Assets/white4x4.dds";
    const String normal_texture_file = "Assets/normal4x4.dds";
    m_DefaultBlackTexture = TextureID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(black_texture_file)));
    m_DefaultWhiteTexture = TextureID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(white_texture_file)));
    m_DefaultNormalTexture = TextureID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(normal_texture_file)));

    assert(m_DefaultBlackTexture.IsValid() && m_DefaultBlackTexture.GetIndex() != 0);
    assert(m_DefaultWhiteTexture.IsValid() && m_DefaultWhiteTexture.GetIndex() != 0);
    assert(m_DefaultNormalTexture.IsValid() && m_DefaultNormalTexture.GetIndex() != 0);

    Material::Default.gpuAlbedoMap = m_DefaultWhiteTexture.GetValue();
    Material::Default.gpuNormalMap = m_DefaultNormalTexture.GetValue();
    Material::Default.gpuEmissiveMap = m_DefaultWhiteTexture.GetValue();
    Material::Default.gpuMetallicMap = m_DefaultWhiteTexture.GetValue();
    Material::Default.gpuRoughnessMap = m_DefaultWhiteTexture.GetValue();

    m_RenderInterface.SetBlackTexture(m_DefaultBlackTexture.GetValue());
    m_RenderInterface.SetWhiteTexture(m_DefaultWhiteTexture.GetValue());

    LogMessage(std::format("[CPU] Default material upload took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // initialize ImGui
    ImGui_ImplSDL3_InitForD3D(m_Window);
    InitImGui(m_Device, Renderer::sSwapchainFormat, sFrameCount);

    m_Renderer.Recompile(m_Device, m_RayTracedScene, GetRenderInterface());

    if (!m_ConfigSettings.mSceneFile.empty() && fs::exists(m_ConfigSettings.mSceneFile))
    {
        m_Scene.OpenFromFile(m_ConfigSettings.mSceneFile.string(), m_Assets, this);
    }

    Game::Start();
}

GameApp::~GameApp()
{
    Game::Stop();
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

    // update NativeScript components
    if (GetGameState() == GAME_RUNNING)
        m_Scene.UpdateNativeScripts(inDeltaTime);


    if (m_GameState == GAME_RUNNING)
        RenderSettings::mPathTraceReset = true;

    for (const Animation& animation : m_Scene.GetComponents<Animation>())
    {
        if (animation.IsPlaying())
        {
            RenderSettings::mPathTraceReset = true;
            break;
        }
    }

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
    
    ImGui::Image(m_RenderInterface.GetDisplayTexture(), ImVec2(width, height));
    
    ImGui::SetCursorPos(cursor_pos);
    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::End();
    ImGui::PopStyleVar(3);

    ImGui::EndFrame();
    ImGui::Render();

    m_Renderer.OnRender(this, m_Device, m_Viewport, m_RayTracedScene, GetRenderInterface(), inDeltaTime);

    m_Device.OnUpdate();

    g_GPUProfiler->Reset(m_Device);
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
            m_Renderer.SetShouldResize(true);
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
                    std::cerr << e.what() << '\n';
                }
            }
        }
    }

    if (inEvent.type == SDL_EVENT_WINDOW_RESIZED)
        m_Renderer.SetShouldResize(true);
}

} // RK