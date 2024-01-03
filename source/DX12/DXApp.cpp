#include "pch.h"
#include "DXApp.h"

#include <locale>
#include <codecvt>

#include "Raekor/OS.h"
#include "Raekor/gui.h"
#include "Raekor/util.h"
#include "Raekor/timer.h"
#include "Raekor/input.h"
#include "Raekor/archive.h"

#include "shared.h"
#include "DXUtil.h"
#include "DXShader.h"
#include "DXRenderer.h"
#include "DXRenderGraph.h"

extern float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(int pixel_i, int pixel_j, int sampleIndex, int sampleDimension);

namespace Raekor::DX12 {

DXApp::DXApp() :
    IEditor(WindowFlag::RESIZE, &m_RenderInterface),
    m_Device(m_Window, sFrameCount),
    m_StagingHeap(m_Device),
    m_Renderer(m_Device, m_Viewport, m_Window),
    m_RayTracedScene(m_Scene),
    m_RenderInterface(this, m_Device, m_Renderer, m_Renderer.GetRenderGraph().GetResources(), m_StagingHeap)
{
    g_RTTIFactory.Register(RTTI_OF(ShaderProgram));
    g_RTTIFactory.Register(RTTI_OF(SystemShadersDX12));
    g_RTTIFactory.Register(RTTI_OF(EShaderProgramType));

    Timer timer;

    auto json_data = JSON::JSONData("assets\\system\\shaders\\DirectX\\shaders.json");
    auto read_archive = JSON::ReadArchive(json_data);
    if (!json_data.IsEmpty())
        read_archive >> g_SystemShaders;

    // compile shaders
    g_SystemShaders.OnCompile();
    g_ThreadPool.WaitForJobs();

    // compile PSOs
    g_SystemShaders.CompilePSOs(m_Device);
    g_ThreadPool.WaitForJobs();

    if (!g_SystemShaders.IsCompiled())
    {
        SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR, "DX12 Error", "Failed to compile system shaders", m_Window);
        std::abort();
    }

    LogMessage(std::format("[CPU] Shader compilation took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // Creating the SRV for the blue noise texture at heap index 0 results in a 4x4 black square in the top left of the texture,
    // this is a hacky workaround. at least we get the added benefit of 0 being an 'invalid' index :D
    (void)m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Add(nullptr);

    static auto blue_noise_samples = std::vector<Vec4>();
    blue_noise_samples.reserve(128 * 128);

    for (auto y = 0u; y < 128; y++)
    {
        for (auto x = 0u; x < 128; x++)
        {
            auto& sample = blue_noise_samples.emplace_back();

            for (auto i = 0u; i < sample.length(); i++)
                sample[i] = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, i);
        }
    }

    auto bluenoise_texture = m_Device.CreateTexture(Texture::Desc2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 128, 128, Texture::Usage::SHADER_READ_ONLY));
    gSetDebugName(m_Device.GetD3D12Resource(bluenoise_texture), "BLUENOISE128x1spp");
    
    assert(m_Device.GetBindlessHeapIndex(bluenoise_texture) == BINDLESS_BLUE_NOISE_TEXTURE_INDEX);
    
    m_Renderer.QueueTextureUpload(bluenoise_texture, 0, Slice<char>((const char*)blue_noise_samples.data(), blue_noise_samples.size() / sizeof(blue_noise_samples[0])));

    LogMessage(std::format("[CPU] Blue noise texture took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // Create default textures / assets
    const auto black_texture_file  = TextureAsset::sConvert("assets/system/black4x4.png");
    const auto white_texture_file  = TextureAsset::sConvert("assets/system/white4x4.png");
    const auto normal_texture_file = TextureAsset::sConvert("assets/system/normal4x4.png");
    m_DefaultBlackTexture  = TextureID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(black_texture_file)));
    m_DefaultWhiteTexture  = TextureID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(white_texture_file)));
    m_DefaultNormalTexture = TextureID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(normal_texture_file)));

    m_Renderer.QueueTextureUpload(m_DefaultBlackTexture, 0, m_Assets.GetAsset<TextureAsset>(black_texture_file));
    m_Renderer.QueueTextureUpload(m_DefaultWhiteTexture, 0, m_Assets.GetAsset<TextureAsset>(white_texture_file));
    m_Renderer.QueueTextureUpload(m_DefaultNormalTexture, 0, m_Assets.GetAsset<TextureAsset>(normal_texture_file));

    assert(m_DefaultBlackTexture.IsValid() && m_DefaultBlackTexture.ToIndex() != 0);
    assert(m_DefaultWhiteTexture.IsValid() && m_DefaultWhiteTexture.ToIndex() != 0);
    assert(m_DefaultNormalTexture.IsValid() && m_DefaultNormalTexture.ToIndex() != 0);

    Material::Default.gpuAlbedoMap    = uint32_t(m_DefaultWhiteTexture);
    Material::Default.gpuNormalMap    = uint32_t(m_DefaultNormalTexture);
    Material::Default.gpuEmissiveMap  = uint32_t(m_DefaultWhiteTexture);
    Material::Default.gpuMetallicMap  = uint32_t(m_DefaultWhiteTexture);
    Material::Default.gpuRoughnessMap = uint32_t(m_DefaultWhiteTexture);

    m_RenderInterface.SetBlackTexture(uint32_t(m_DefaultBlackTexture));
    m_RenderInterface.SetWhiteTexture(uint32_t(m_DefaultWhiteTexture));

    LogMessage(std::format("[CPU] Default material upload took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // initialize ImGui
    ImGui_ImplSDL2_InitForD3D(m_Window);
    m_ImGuiFontTextureID = InitImGui(m_Device, Renderer::sSwapchainFormat, sFrameCount);

    LogMessage(std::format("[CPU] ImGui init took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // check for ray-tracing support
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    gThrowIfFailed(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));

    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1)
    {
        SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR, "DX12 Error", "GPU does not support D3D12_RAYTRACING_TIER_1_1.", m_Window);
        abort();
    }

    // initialize DirectStorage 1.0
    auto queue_desc = DSTORAGE_QUEUE_DESC
    {
        .SourceType = DSTORAGE_REQUEST_SOURCE_FILE,
        .Capacity = DSTORAGE_MAX_QUEUE_CAPACITY,
        .Priority = DSTORAGE_PRIORITY_NORMAL,
        .Device = *m_Device,
    };

    auto storage_factory = ComPtr<IDStorageFactory> {};
    gThrowIfFailed(DStorageGetFactory(IID_PPV_ARGS(&storage_factory)));

    gThrowIfFailed(storage_factory->CreateQueue(&queue_desc, IID_PPV_ARGS(&m_FileStorageQueue)));

    queue_desc.SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY;
    gThrowIfFailed(storage_factory->CreateQueue(&queue_desc, IID_PPV_ARGS(&m_MemoryStorageQueue)));

    LogMessage(std::format("[CPU] DirectStorage init took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    m_Renderer.Recompile(m_Device, m_RayTracedScene, GetRenderInterface());

    LogMessage(std::format("[CPU] RenderGraph compilation took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    if (!m_Settings.mSceneFile.empty() && fs::exists(m_Settings.mSceneFile))
    {
        m_Scene.OpenFromFile(m_Settings.mSceneFile.string(), m_Assets);
    }
}


DXApp::~DXApp()
{
    m_Renderer.WaitForIdle(m_Device);
    m_Device.ReleaseTextureImmediate(m_ImGuiFontTextureID);
}


void DXApp::OnUpdate(float inDeltaTime)
{
    IEditor::OnUpdate(inDeltaTime);

    // add debug geometry around a selected mesh
    if (GetActiveEntity() != NULL_ENTITY)
    {
        if (m_Scene.Has<Mesh>(m_ActiveEntity))
        {
            const auto& [mesh, transform] = m_Scene.Get<Mesh, Transform>(m_ActiveEntity);
            m_RenderInterface.AddDebugBox(mesh.aabb[0], mesh.aabb[1], transform.worldTransform);
        }

        else if (m_Scene.Has<Light>(m_ActiveEntity))
        {
            const auto& light = m_Scene.Get<Light>(m_ActiveEntity);

            if (light.type == LIGHT_TYPE_SPOT)
            {
                Vec3 pos = light.position;
                Vec3 target = pos + light.direction * light.attributes.x;

                Vec3 right = glm::normalize(glm::cross(light.direction, glm::vec3(0.0f, 1.0f, 0.0f)));
                Vec3 up = glm::cross(right, light.direction);

                // Calculate cone radius and step angle
                float half_angle = light.attributes.z / 2.0f;
                float cone_radius = light.attributes.x * tan(half_angle);

                constexpr auto steps = 16u;
                constexpr auto step_angle = glm::radians(360.0f / float(steps));

                Vec3 last_point;

                for (int i = 0; i <= steps; i++)
                {
                    float angle = step_angle * i;
                    Vec3 circle_point = target + cone_radius * ( cos(angle) * right + sin(angle) * up );

                    m_RenderInterface.AddDebugLine(pos, circle_point);

                    if (i > 0)
                        m_RenderInterface.AddDebugLine(last_point, circle_point);

                    last_point = circle_point;
                }
            }

            else if (light.type == LIGHT_TYPE_POINT)
            {
                const auto light_pos = Vec3(light.position);

                constexpr float step_size = M_PI * 2.0f / 32.0f;

                auto DrawDebugCircle = [&](Vec3 inDir, Vec3 inAxis)
                {
                    Vec3 point = inDir;

                    for (float angle = 0.0f; angle < M_PI * 2.0f; angle += step_size)
                    {
                        Vec3 new_point = glm::toMat3(glm::angleAxis(step_size, inAxis)) * point;

                        Vec3 start_point = light_pos + point * light.attributes.x;

                        Vec3 end_point = light_pos + new_point * light.attributes.x;

                        m_RenderInterface.AddDebugLine(start_point, end_point);

                        point = new_point;

                    }
                };

                DrawDebugCircle(Vec3(1, 0, 0), Vec3(0, 1, 0));
                DrawDebugCircle(Vec3(0, 1, 0), Vec3(0, 0, 1));
                DrawDebugCircle(Vec3(0, 0, 1), Vec3(1, 0, 0));
            }
        }
        else if (m_Scene.Has<DirectionalLight>(GetActiveEntity()))
        {
            const DirectionalLight& light = m_Scene.Get<DirectionalLight>(m_ActiveEntity);
            Vec3 direction = Vec3(light.GetDirection());

            if (m_Scene.Has<Transform>(GetActiveEntity()))
            {
                const Transform& transform = m_Scene.Get<Transform>(GetActiveEntity());

                constexpr float cWidth = 0.6f;
                constexpr float cLength = 2.4f;

                Vec3 start_point = transform.GetPositionWorldSpace();
                Vec3 end_point = start_point + direction * 4.0f;

                for (int i = 0; i < 4; i++)
                {
                    constexpr float up_dirs[4] = { 1.0f, -1.0f, 0.0f,  0.0f };
                    constexpr float right_dirs[4] = { 0.0f, 0.0f, -1.0f, 1.0f };
                

                    Vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
                    Vec3 up = glm::normalize(glm::cross(right, direction));

                    up = up * up_dirs[i];
                    right = right * right_dirs[i];

                    Vec3 curr_point = start_point;
                    Vec3 next_point = start_point + up * cWidth + right * cWidth;
                    m_RenderInterface.AddDebugLine(curr_point, next_point);

                    curr_point = next_point;
                    next_point = curr_point + direction * cLength;
                    m_RenderInterface.AddDebugLine(curr_point, next_point);


                    curr_point = next_point;
                    next_point = curr_point + up * cWidth + right * cWidth;
                    m_RenderInterface.AddDebugLine(curr_point, next_point);

                    m_RenderInterface.AddDebugLine(next_point, end_point);
                }

            }

        }
    }
    
#if 0 
    constexpr auto cGridLineLength = 256.0f;
    constexpr auto cGridInfiniteLength = 4096.0f;
    constexpr auto cGridLineColor = Vec4(0.8f, 0.8f, 0.8f, 0.5f);

    // dumb and expensive
    for (int x = -cGridLineLength; x < cGridLineLength; x++)
    {
        if (x == 0) continue;
        m_RenderInterface.AddDebugLineColored(Vec3(-cGridLineLength, 0, float(x)), Vec3(cGridLineLength, 0, float(x)), cGridLineColor);
        m_RenderInterface.AddDebugLineColored(Vec3(float(x), 0, -cGridLineLength), Vec3(float(x), 0, cGridLineLength), cGridLineColor);

    }

    // annoying most of the time
    m_RenderInterface.AddDebugLineColored(Vec3(-cGridInfiniteLength, 0, 0), Vec3(cGridInfiniteLength, 0, 0), Vec4(1.0, 0.25, 0.30, 1));
    m_RenderInterface.AddDebugLineColored(Vec3(0, -cGridInfiniteLength, 0), Vec3(0, cGridInfiniteLength, 0), Vec4(0.68, 0.85, 0.05, 1));
    m_RenderInterface.AddDebugLineColored(Vec3(0, 0, -cGridInfiniteLength), Vec3(0, 0, cGridInfiniteLength), Vec4(0.20, 0.70, 0.95, 1));
#endif

    if (m_ViewportChanged || m_Viewport.GetCamera().Changed() || m_Physics.GetState() == Physics::EState::Stepping)
        PathTraceData::mReset = true;
    
    m_RenderInterface.UpdateGPUStats(m_Device);

    m_Renderer.OnRender(this, m_Device, m_Viewport, m_RayTracedScene, m_StagingHeap, GetRenderInterface(), inDeltaTime);

    DebugPrimitivesData::mVertexData.clear();
}


void DXApp::OnEvent(const SDL_Event& inEvent)
{
    IEditor::OnEvent(inEvent);

    if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat)
    {
        // ALT + ENTER event (Windowed <-> Fullscreen toggle)
        if (inEvent.key.keysym.sym == SDLK_RETURN && SDL_GetModState() & KMOD_LALT)
        {
            // This only toggles between windowed and borderless fullscreen, exclusive fullscreen needs to be set from the menu
            if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
                SDL_SetWindowFullscreen(m_Window, 0);
            else
                SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_FULLSCREEN_DESKTOP);

            // SDL2 should have updated the window by now, so get the new size
            auto width = 0, height = 0;
            SDL_GetWindowSize(m_Window, &width, &height);

            // Updat the viewport and tell the renderer to resize to the viewport
            m_Viewport.SetRenderSize(UVec2(width, height));
            m_Renderer.SetShouldResize(true);

            SDL_DisplayMode mode;
            SDL_GetWindowDisplayMode(m_Window, &mode);
            LogMessage(std::format("SDL Display Mode: {}x{}@{}Hz \n", mode.w, mode.h, mode.refresh_rate));
        }
    }

    if (inEvent.window.event == SDL_WINDOWEVENT_RESIZED)
        m_Renderer.SetShouldResize(true);

    m_Widgets.OnEvent(inEvent);
}


DescriptorID DXApp::UploadTextureDirectStorage(const TextureAsset::Ptr& inAsset, DXGI_FORMAT inFormat)
{
    auto factory = ComPtr<IDStorageFactory>();
    gThrowIfFailed(DStorageGetFactory(IID_PPV_ARGS(&factory)));

    auto data_ptr = inAsset->GetData();
    const auto header_ptr = inAsset->GetHeader();
    // I think DirectStorage doesnt do proper texture data alignment for its upload buffers as we get garbage past the 4th mip
    const auto mipmap_levels = std::min(header_ptr->dwMipMapCount, 4ul);
    // const auto mipmap_levels = header->dwMipMapCount;

    const auto debug_name = inAsset->GetPath().string();

    auto& texture = m_Device.GetTexture(m_Device.CreateTexture(Texture::Desc {
        .format     = inFormat,
        .width      = header_ptr->dwWidth,
        .height     = header_ptr->dwHeight,
        .mipLevels  = mipmap_levels,
        .usage      = Texture::SHADER_READ_ONLY,
        .debugName  = debug_name.c_str()
    }));

    auto requests = std::vector<DSTORAGE_REQUEST>(mipmap_levels);

    for (auto [mip, request] : gEnumerate(requests))
    {
        const auto dimensions = glm::ivec2(std::max(header_ptr->dwWidth >> mip, 1ul), std::max(header_ptr->dwHeight >> mip, 1ul));
        const auto data_size = std::max(1u, ( ( dimensions.x + 3u ) / 4u )) * std::max(1u, ( ( dimensions.y + 3u ) / 4u )) * 16u;

        request = DSTORAGE_REQUEST {
            .Options = DSTORAGE_REQUEST_OPTIONS {
                .SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY,
                .DestinationType = DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION,
            },
            .Source = DSTORAGE_SOURCE {
                DSTORAGE_SOURCE_MEMORY {
                    .Source = data_ptr,
                    .Size = data_size
            }},

            .Destination = DSTORAGE_DESTINATION {
                .Texture = DSTORAGE_DESTINATION_TEXTURE_REGION {
                    .Resource = texture.GetD3D12Resource().Get(),
                    .SubresourceIndex = uint32_t(mip),
                    .Region = CD3DX12_BOX(0, 0, dimensions.x, dimensions.y),
                }
            },
            .Name = inAsset->GetPath().string().c_str()
        };

        data_ptr += dimensions.x * dimensions.y;

        m_MemoryStorageQueue->EnqueueRequest(&request);
    }

    auto fence = ComPtr<ID3D12Fence> {};
    gThrowIfFailed(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));

    auto fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    constexpr auto fence_value = 1ul;
    gThrowIfFailed(fence->SetEventOnCompletion(fence_value, fence_event));
    m_MemoryStorageQueue->EnqueueSignal(fence.Get(), fence_value);

    // Tell DirectStorage to start executing all queued items.
    m_MemoryStorageQueue->Submit();

    // Wait for the submitted work to complete
    std::cout << "Waiting for the DirectStorage request to complete...\n";
    WaitForSingleObject(fence_event, INFINITE);
    CloseHandle(fence_event);

    return texture.GetView();
}

} // namespace::Raekor