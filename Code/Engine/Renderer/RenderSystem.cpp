#include "PCH.h"
#include "RenderSystem.h"

#include "Shared.h"
#include "Shader.h"
#include "GPUScene.h"
#include "GPUProfiler.h"
#include "RenderGraph.h"
#include "RenderPasses.h"

#include "OS.h"
#include "Timer.h"
#include "Assets.h"
#include "Profiler.h"
#include "Components.h"
#include "Primitives.h"
#include "Application.h"

extern float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(int pixel_i, int pixel_j, int sampleIndex, int sampleDimension);

namespace RK::DX12 {

RenderSystem::RenderSystem(Application* inApp) :
    m_Device(inApp),
    m_Renderer(m_Device, inApp->GetViewport(), inApp->GetWindow())
{
    DXGI_ADAPTER_DESC adapter_desc = {};
    m_Device.GetAdapter()->GetDesc(&adapter_desc);

    GPUInfo gpu_info = {};
    switch (adapter_desc.VendorId)
    {
        case 0x000010de: gpu_info.mVendor = "NVIDIA Corporation";           break;
        case 0x00001002: gpu_info.mVendor = "Advanced Micro Devices, Inc."; break;
        case 0x00008086: gpu_info.mVendor = "Intel Corporation";            break;
        case 0x00001414: gpu_info.mVendor = "Microsoft Corporation";        break;
    }

    char ch = ' ';
    char description[128];
    WideCharToMultiByte(CP_ACP, 0, adapter_desc.Description, -1, description, 128, &ch, NULL);

    gpu_info.mProduct = String(description);
    gpu_info.mActiveAPI = "DirectX 12 Ultimate";

    m_GPUInfo = gpu_info;

    const String light_texture_file = TextureAsset::Convert("Assets/light.png");
    m_LightTexture = TextureID(UploadTextureFromAsset(inApp->GetAssets()->GetAsset<TextureAsset>(light_texture_file)));

    const String camera_texture_file = TextureAsset::Convert("Assets/camera.png");
    m_CameraTexture = TextureID(UploadTextureFromAsset(inApp->GetAssets()->GetAsset<TextureAsset>(camera_texture_file)));

    g_GPUProfiler = new GPUProfiler(m_Device);

    CompileSystemShaders(inApp->GetWindow());
    CreateDefaultTextures();

    m_Renderer.SetDefaultTextures(m_DefaultBlackTexture, m_DefaultWhiteTexture);
    m_Renderer.Recompile(m_Device);
}



RenderSystem::~RenderSystem()
{
    m_Renderer.WaitForIdle(m_Device);

    if (m_ImGuiFontTexture.IsValid())
        m_Device.ReleaseTextureImmediate(m_ImGuiFontTexture);
}



void RenderSystem::CompileSystemShaders(SDL_Window* inWindow)
{
    Timer timer;

    g_RTTIFactory.Register(RTTI_OF<ComputeProgram>());
    g_RTTIFactory.Register(RTTI_OF<GraphicsProgram>());
    g_RTTIFactory.Register(RTTI_OF<SystemShadersDX12>());

    JSON::ReadArchive read_archive("Assets/Shaders/Backend/Shaders.json");
    read_archive >> g_SystemShaders;

    g_SystemShaders.OnCompile(m_Device);

    if (!g_SystemShaders.IsCompiled())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DX12 Error", "Failed to compile system shaders", inWindow);
        std::abort();
    }

    gLogInfo("DX12", "Shader compilation took {:.2f} ms", Timer::sToMilliseconds(timer.GetElapsedTime()));
}



void RenderSystem::CreateDefaultTextures()
{
    (void)m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Add(nullptr);

    Array<Vec4> blue_noise_samples;
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

    m_BlueNoiseTexture = m_Device.CreateTexture(Texture::Desc
    {
        .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .width  = 128,
        .height = 128,
        .usage  = Texture::Usage::SHADER_READ_ONLY,
        .debugName = "BlueNoise128x1spp"
    });

    RK_ASSERT(m_Device.GetBindlessHeapIndex(m_BlueNoiseTexture) == BINDLESS_BLUE_NOISE_TEXTURE_INDEX);

    m_Device.UploadTextureData(m_Device.GetTexture(m_BlueNoiseTexture), 0, 0, sizeof(Vec4) * 128, blue_noise_samples.data());

    m_DefaultBlackTexture  = m_Device.CreateTexture(Texture::Desc2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1, Texture::SHADER_READ_ONLY));
    m_DefaultWhiteTexture  = m_Device.CreateTexture(Texture::Desc2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1, Texture::SHADER_READ_ONLY));
    m_DefaultNormalTexture = m_Device.CreateTexture(Texture::Desc2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1, Texture::SHADER_READ_ONLY));

    m_Device.SetDebugName(m_DefaultBlackTexture, "DefaultBlackTexture");
    m_Device.SetDebugName(m_DefaultWhiteTexture, "DefaultWhiteTexture");
    m_Device.SetDebugName(m_DefaultNormalTexture, "DefaultNormalTexture");

    constexpr Vec4 black_pixel  = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    constexpr Vec4 white_pixel  = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    constexpr Vec4 normal_pixel = Vec4(0.5f, 0.5f, 1.0f, 1.0f);

    m_Device.UploadTextureData(m_Device.GetTexture(m_DefaultBlackTexture),  0, 0, sizeof(Vec4), &black_pixel);
    m_Device.UploadTextureData(m_Device.GetTexture(m_DefaultWhiteTexture),  0, 0, sizeof(Vec4), &white_pixel);
    m_Device.UploadTextureData(m_Device.GetTexture(m_DefaultNormalTexture), 0, 0, sizeof(Vec4), &normal_pixel);

    Material::Default.gpuAlbedoMap = m_DefaultWhiteTexture.GetValue();
    Material::Default.gpuNormalMap = m_DefaultNormalTexture.GetValue();
    Material::Default.gpuEmissiveMap = m_DefaultWhiteTexture.GetValue();
    Material::Default.gpuMetallicMap = m_DefaultWhiteTexture.GetValue();
    Material::Default.gpuRoughnessMap = m_DefaultWhiteTexture.GetValue();
}



void RenderSystem::InitImGui(SDL_Window* inWindow)
{
    ImGui_ImplSDL3_InitForD3D(inWindow);
    m_ImGuiFontTexture = DX12::InitImGui(m_Device, Renderer::sSwapchainFormat, Renderer::sFrameCount);
}



void RenderSystem::OnRender(Application* inApp, const Scene& inScene, float inDeltaTime)
{
    if (inApp->GetGameState() == GAME_RUNNING)
        RenderSettings::mPathTraceReset = true;

    for (const Animation& animation : inScene.GetStorage<Animation>())
    {
        if (animation.IsPlaying())
        {
            RenderSettings::mPathTraceReset = true;
            break;
        }
    }

    UpdateGPUStats();

    m_Renderer.OnRender(inApp, m_Device, inApp->GetViewport(), inScene, inDeltaTime);

    m_Device.OnUpdate();

    g_GPUProfiler->Reset(m_Device);
}



void RenderSystem::UpdateGPUStats()
{
    m_GPUStats.mFrameCounter = m_Renderer.GetFrameCounter();
    m_GPUStats.mLiveBuffers.store(m_Device.GetBufferPool().GetSize());
    m_GPUStats.mLiveTextures.store(m_Device.GetTexturePool().GetSize());
    m_GPUStats.mLiveDSVHeap.store(m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).GetSize());
    m_GPUStats.mLiveRTVHeap.store(m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).GetSize());
    m_GPUStats.mLiveSamplerHeap.store(m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetSize());
    m_GPUStats.mLiveResourceHeap.store(m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetSize());
}



uint64_t RenderSystem::GetDisplayTexture()
{
    return m_Device.GetGPUDescriptorHandle(m_Renderer.GetDisplayTexture()).ptr;
}



uint64_t RenderSystem::GetLightTexture()
{
    return m_LightTexture.IsValid() ? m_Device.GetGPUDescriptorHandle(m_LightTexture).ptr : 0;
}

uint64_t RenderSystem::GetCameraTexture()
{
    return m_CameraTexture.IsValid() ? m_Device.GetGPUDescriptorHandle(m_CameraTexture).ptr : 0;
}


uint64_t RenderSystem::GetImGuiTextureID(uint32_t inHandle)
{
    uint32_t heap_index = m_Device.GetBindlessHeapIndex(TextureID(inHandle));
    uint32_t handle_size = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    DescriptorHeap& resource_heap = m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    uint64_t heap_ptr = resource_heap->GetGPUDescriptorHandleForHeapStart().ptr + heap_index * handle_size;

    return heap_ptr;
}



const char* RenderSystem::GetDebugTextureName(uint32_t inIndex) const
{
    constexpr std::array names =
    {
        "None",
        "Depth",
        "Albedo",
        "Normals",
        "Emissive",
        "Velocity",
        "Metallic",
        "Roughness",
        "Lighting",
        "SSR",
        "GTAO",
        "RT Shadows",
        "RT Reflections",
        "RT Indirect Diffuse",
        "RT Ambient Occlusion",
    };

    static_assert( names.size() == DEBUG_TEXTURE_COUNT );

    assert(inIndex < DEBUG_TEXTURE_COUNT);
    return names[inIndex];
}



void RenderSystem::UploadMeshBuffers(Entity inEntity, Mesh& inMesh)
{
    DestroyMeshBuffers(inEntity, inMesh);

    const uint64_t indices_size = inMesh.indices.size() * sizeof(inMesh.indices[0]);
    const uint64_t vertices_size = inMesh.vertices.size() * sizeof(inMesh.vertices[0]);

    if (!vertices_size || !indices_size)
        return;

    const BufferID index_buffer = m_Device.CreateBuffer(Buffer::Desc
    {
        .format = DXGI_FORMAT_R32_UINT,
        .size   = indices_size,
        .stride = sizeof(uint32_t) * 3,
        .usage  = Buffer::Usage::INDEX_BUFFER,
        .debugName = "IndexBuffer"
    });

    const BufferID vertex_buffer = m_Device.CreateBuffer(Buffer::Desc
    {
        .size   = vertices_size,
        .stride = sizeof(Vertex),
        .usage  = Buffer::Usage::VERTEX_BUFFER,
        .debugName = "VertexBuffer"
    });

    m_Device.UploadInitialBufferData(m_Device.GetBuffer(index_buffer), inMesh.indices.data(), indices_size);
    m_Device.UploadInitialBufferData(m_Device.GetBuffer(vertex_buffer), inMesh.vertices.data(), vertices_size);

    const BufferID bottom_level_as = m_Renderer.GetGPUScene().CreateBottomLevelAS(m_Device, BottomLevelASBuild
    {
        .mIndexBuffer  = index_buffer,
        .mVertexBuffer = vertex_buffer,
        .mIndexCount   = uint32_t(inMesh.indices.size()),
        .mVertexCount  = uint32_t(inMesh.positions.size())
    });

    inMesh.indexBuffer = index_buffer.GetValue();
    inMesh.vertexBuffer = vertex_buffer.GetValue();
    inMesh.BottomLevelAS = bottom_level_as.GetValue();
}


void RenderSystem::DestroyMeshBuffers(Entity inEntity, Mesh& inMesh)
{
    m_Renderer.GetGPUScene().ReleaseBottomLevelAS(m_Device, gToBufferID(inMesh.BottomLevelAS));

    for (uint32_t buffer : { inMesh.indexBuffer, inMesh.vertexBuffer })
    {
        if (buffer != 0)
            m_Device.ReleaseBuffer(BufferID(buffer));
    }

    inMesh.indexBuffer = 0;
    inMesh.vertexBuffer = 0;
    inMesh.BottomLevelAS = 0;
}


void RenderSystem::UploadSkeletonBuffers(Entity inEntity, Skeleton& inSkeleton, Mesh& inMesh)
{
    DestroySkeletonBuffers(inEntity, inSkeleton);

    inSkeleton.boneTransformMatrices.resize(inSkeleton.boneOffsetMatrices.size(), Mat4x4(1.0f));
    inSkeleton.boneWSTransformMatrices.resize(inSkeleton.boneOffsetMatrices.size(), Mat4x4(1.0f));

    if (inSkeleton.boneIndices.empty() || inSkeleton.boneWeights.empty() || inSkeleton.boneTransformMatrices.empty() || inMesh.vertices.empty())
        return;

    const BufferID bone_index_buffer = m_Device.CreateBuffer(Buffer::Desc
    {
        .size   = inSkeleton.boneIndices.size() * sizeof(IVec4),
        .stride = sizeof(IVec4),
        .usage  = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = "BoneIndicesBuffer"
    });

    const BufferID bone_weight_buffer = m_Device.CreateBuffer(Buffer::Desc
    {
        .size   = inSkeleton.boneWeights.size() * sizeof(Vec4),
        .stride = sizeof(Vec4),
        .usage  = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = "BoneWeightsBuffer"
    });

    const BufferID bone_transforms_buffer = m_Device.CreateBuffer(Buffer::Desc
    {
        .size   = inSkeleton.boneTransformMatrices.size() * sizeof(Mat4x4),
        .stride = sizeof(Mat4x4),
        .usage  = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = "BoneTransformsBuffer"
    });

    const BufferID skinned_vertex_buffer = m_Device.CreateBuffer(Buffer::Desc
    {
        .size   = sizeof(inMesh.vertices[0]) * inMesh.vertices.size(),
        .stride = sizeof(RTVertex),
        .usage  = Buffer::Usage::SHADER_READ_WRITE,
        .debugName = "SkinnedVertexBuffer"
    });

    m_Device.UploadInitialBufferData(m_Device.GetBuffer(bone_index_buffer), inSkeleton.boneIndices.data(), inSkeleton.boneIndices.size() * sizeof(IVec4));
    m_Device.UploadInitialBufferData(m_Device.GetBuffer(bone_weight_buffer), inSkeleton.boneWeights.data(), inSkeleton.boneWeights.size() * sizeof(Vec4));

    inSkeleton.boneIndexBuffer = bone_index_buffer.GetValue();
    inSkeleton.boneWeightBuffer = bone_weight_buffer.GetValue();
    inSkeleton.boneTransformsBuffer = bone_transforms_buffer.GetValue();
    inSkeleton.skinnedVertexBuffer = skinned_vertex_buffer.GetValue();

    if (inMesh.BottomLevelAS != 0)
    {
        GPUScene& gpu_scene = m_Renderer.GetGPUScene();
        gpu_scene.ReleaseBottomLevelAS(m_Device, gToBufferID(inMesh.BottomLevelAS));

        const uint32_t index_count = uint32_t(inMesh.indices.size());
        const uint32_t vertex_count = uint32_t(inMesh.positions.size());

        inMesh.BottomLevelAS = gpu_scene.CreateBottomLevelAS(m_Device, BottomLevelASBuild
        {
            .mIndexBuffer  = gToBufferID(inMesh.indexBuffer),
            .mVertexBuffer = gToBufferID(inMesh.vertexBuffer),
            .mIndexCount   = index_count,
            .mVertexCount  = vertex_count,
            .mAllowUpdate  = true
        }).GetValue();

        const uint64_t scratch_size = GPUScene::sGetBottomLevelASUpdateScratchSize(m_Device, index_count, vertex_count);
        constexpr uint64_t alignment = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT;

        inSkeleton.blasScratchBuffer = m_Device.CreateBuffer(Buffer::RWByteAddressBuffer(std::max(gAlignUp(scratch_size, alignment), alignment), "SCRATCH_BUFFER_BLAS_REFIT")).GetValue();
    }

    inSkeleton.gpuBuffersUploaded = true;
}


void RenderSystem::DestroySkeletonBuffers(Entity inEntity, Skeleton& inSkeleton)
{
    for (uint32_t* buffer : { &inSkeleton.boneIndexBuffer, &inSkeleton.boneWeightBuffer, &inSkeleton.skinnedVertexBuffer, &inSkeleton.boneTransformsBuffer, &inSkeleton.blasScratchBuffer })
    {
        if (*buffer != 0)
            m_Device.ReleaseBuffer(BufferID(*buffer));

        *buffer = 0;
    }

    inSkeleton.gpuBuffersUploaded = false;
}


void RenderSystem::CompileMaterialShaders(Entity inEntity, Material& inMaterial)
{
    if (fs::exists(inMaterial.pixelShaderFile) && fs::is_regular_file(inMaterial.pixelShaderFile))
        g_ShaderCompiler.CompileShader(inMaterial.pixelShaderFile, SHADER_TYPE_PIXEL, "", inMaterial.pixelShader);

    if (fs::exists(inMaterial.vertexShaderFile) && fs::is_regular_file(inMaterial.vertexShaderFile))
        g_ShaderCompiler.CompileShader(inMaterial.vertexShaderFile, SHADER_TYPE_VERTEX, "", inMaterial.vertexShader);
}


void RenderSystem::ReleaseMaterialShaders(Entity inEntity, Material& inMaterial)
{
    g_ShaderCompiler.ReleaseShader(inMaterial.pixelShader);
    g_ShaderCompiler.ReleaseShader(inMaterial.vertexShader);
}


uint32_t RenderSystem::UploadTextureFromAsset(TextureAsset::Ptr inAsset, bool inIsSRGB, uint8_t inSwizzle)
{
    dds::Header header = dds::read_header(inAsset->GetData(), inAsset->GetDataSize());
    RK_ASSERT(header.is_valid());

    Texture::Desc desc = {};
    desc.swizzle = inSwizzle;
    desc.width = header.width();
    desc.height = header.height();
    desc.mipLevels = header.mip_levels();
    desc.depthOrArrayLayers = header.array_size();
    desc.usage = Texture::SHADER_READ_ONLY;
    desc.format = (DXGI_FORMAT)header.format();

    if (header.is_3d())
    {
        desc.depthOrArrayLayers = header.depth();
        desc.dimension = Texture::TEX_DIM_3D;
    }

    if (header.is_cubemap())
    {
        desc.dimension = Texture::TEX_DIM_CUBE;
        desc.usage = Texture::SHADER_READ_WRITE;
    }

    // HACK: texture conversion is hardcoded to BC3_UNORM, so add SRGB here..
    if (inIsSRGB && !gIsDXGIFormatSRGB(desc.format))
        desc.format = gDXGIFormatToSRGB(desc.format);

    String debug_name = inAsset->GetPath().string();
    desc.debugName = debug_name.c_str();

    const TextureID texture = m_Device.CreateTexture(desc);

    for (uint32_t layer = 0; layer < desc.depthOrArrayLayers; layer++)
    {
        for (uint32_t mip = 0; mip < desc.mipLevels; mip++)
        {
            size_t data_offset = header.mip_offset(mip, layer);
            const uint8_t* data_ptr = inAsset->GetData() + data_offset;

            m_Device.UploadTextureData(m_Device.GetTexture(texture), mip, layer, header.row_pitch(mip), data_ptr);
        }
    }

    return texture.GetValue();
}



void RenderSystem::DrawDebugSettings(Application* inApp)
{
    ImGui::SeparatorText("Renderer Settings");

    ImGui::Checkbox("##pathtracingtoggle", (bool*)&m_Renderer.GetSettings().mDoPathTrace);

    ImGui::SameLine();

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Enable Path Tracer"))
    {
        ImGui::SeparatorText("Settings");

        if (ImGui::SliderInt("Bounces", (int*)&RenderSettings::mPathTraceBounces, 1, 8))
            RenderSettings::mPathTraceReset = true;

        if (ImGui::SliderInt("Alpha Bounces", (int*)&RenderSettings::mPathTraceAlphaBounces, 0, 64))
            RenderSettings::mPathTraceReset = true;

        ImGui::EndMenu();
    }

    ImGui::Checkbox("##TAAtoggle", (bool*)&m_Renderer.GetSettings().mEnableTAA);

    ImGui::SameLine();

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Enable Temporal AA"))
    {
        ImGui::SeparatorText("Upscaler");

        if (m_Renderer.GetSettings().mEnableTAA)
        {
            constexpr const char* text = "...TAA is enabled";
            ImGui::SetCursorPosX(( ImGui::GetWindowWidth() - ImGui::CalcTextSize(text).x ) / 2.f);
            ImGui::TextDisabled(text);
        }
        else
        {
            constexpr std::array upscaler_items = { "No Upscaler", "AMD FSR2", "Nvidia DLSS", "Intel XeSS" };
            constexpr std::array upscaler_quality_items = { "Native", "Quality", "Balanced", "Performance" };

            Upscaler& upscaler = m_Renderer.GetUpscaler();

            if (ImGui::BeginCombo("##Upscaler", upscaler_items[upscaler.GetActiveUpscaler()], ImGuiComboFlags_None))
            {
                for (uint32_t upscaler_idx = 0; upscaler_idx < UPSCALER_COUNT; upscaler_idx++)
                {
                    if (ImGui::Selectable(upscaler_items[upscaler_idx], upscaler.GetActiveUpscaler() == upscaler_idx))
                    {
                        upscaler.SetActiveUpscaler(EUpscaler(upscaler_idx));
                    }
                }

                ImGui::EndCombo();
            }

            if (upscaler.GetActiveUpscaler() != UPSCALER_NONE)
            {
                if (ImGui::BeginCombo("##UpscalerQuality", upscaler_quality_items[upscaler.GetActiveUpscalerQuality()], ImGuiComboFlags_None))
                {
                    for (uint32_t quality_idx = 0; quality_idx < UPSCALER_QUALITY_COUNT; quality_idx++)
                    {
                        if (ImGui::Selectable(upscaler_quality_items[quality_idx], upscaler.GetActiveUpscalerQuality() == EUpscalerQuality(quality_idx)))
                        {
                            upscaler.SetActiveUpscalerQuality(EUpscalerQuality(quality_idx));
                        }
                    }

                    ImGui::EndCombo();
                }
            }
        }

        ImGui::DragFloat("Jitter Scale", &m_Renderer.GetSettings().mJitterScale, 0.01f, 0.0f, 100.0f, "%.2f");

        ImGui::EndMenu();
    }

    ImGui::Checkbox("Enable Debug Overlay", (bool*)&m_Renderer.GetSettings().mEnableDebugOverlay);

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Debug Settings"))
    {
        ImGui::SeparatorText("Debug");

        if (ImGui::Button("PIX GPU Capture"))
            m_Renderer.SetShouldCaptureNextFrame(true);

        if (ImGui::Button("Save As GraphViz.."))
        {
            const String file_path = OS::sSaveFileDialog("DOT File (*.dot)\0", "dot");

            if (!file_path.empty())
            {
                std::ofstream ofs = std::ofstream(file_path);
                ofs << m_Renderer.GetRenderGraph().ToGraphVizText(m_Device, TextureID());
            }
        }

        ImGui::EndMenu();
    }

    ImGui::AlignTextToFramePadding();


    if (ImGui::BeginMenu("Window Settings"))
    {
        ImGui::SeparatorText("Window");

        static constexpr std::array modes =
        {
            0u /* Windowed */,
            (uint32_t)SDL_WINDOW_FULLSCREEN,
            (uint32_t)SDL_WINDOW_FULLSCREEN
        };

        static constexpr std::array mode_strings =
        {
            "Windowed",
            "Borderless",
            "Fullscreen"
        };

        int mode = 0u; // Windowed
        SDL_Window* window = inApp->GetWindow();

        if (inApp->IsWindowBorderless())
            mode = 1u;
        else if (inApp->IsWindowExclusiveFullscreen())
            mode = 2u;

        if (ImGui::BeginCombo("Mode", mode_strings[mode]))
        {
            if (ImGui::Selectable("Windowed", mode == 0))
            {
                SDL_SetWindowFullscreen(window, false);
                m_Renderer.SetShouldResize(true);
            }

            if (ImGui::Selectable("Borderless", mode == 1))
            {
                SDL_SetWindowFullscreenMode(window, NULL);
                SDL_SetWindowFullscreen(window, true);

                m_Renderer.SetShouldResize(true);
            }

            if (ImGui::Selectable("Fullscreen", mode == 2))
            {
                SDL_SetWindowFullscreen(window, true);
                SDL_SyncWindow(window);
                
                SDL_DisplayID display_id = SDL_GetDisplayForWindow(window);
                SDL_DisplayMode** display_modes = SDL_GetFullscreenDisplayModes(display_id, NULL);
                SDL_SetWindowFullscreenMode(window, display_modes[m_Renderer.GetSettings().mDisplayMode]);
                SDL_SyncWindow(window);

                m_Renderer.SetShouldResize(true);
            }

            ImGui::EndCombo();
        }

        int display_count = 0;
        SDL_DisplayID* displays = SDL_GetDisplays(&display_count);

        const SDL_DisplayID display_id = SDL_GetDisplayForWindow(window);
        if (ImGui::BeginCombo("Monitor", SDL_GetDisplayName(display_id)))
        {
            for (SDL_DisplayID id : Slice(displays, display_count))
            {
                if (ImGui::Selectable(SDL_GetDisplayName(id), display_id == id))
                {
                    if (m_Renderer.GetSettings().mFullscreen)
                    {
                        SDL_SetWindowFullscreen(window, 0);
                        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(id), SDL_WINDOWPOS_CENTERED_DISPLAY(id));
                        SDL_SetWindowFullscreen(window, true);
                    }
                    else
                    {
                        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(id), SDL_WINDOWPOS_CENTERED_DISPLAY(id));
                    }

                    m_Renderer.SetShouldResize(true);
                }
            }

            ImGui::EndCombo();
        }

        const SDL_DisplayID current_display_id = SDL_GetDisplayForWindow(window);

        int num_display_modes = 0;
        SDL_DisplayMode** display_modes = SDL_GetFullscreenDisplayModes(current_display_id, &num_display_modes);

        SDL_DisplayMode* new_display_mode = nullptr;
        Array<String> display_mode_strings(num_display_modes);

        if (m_Renderer.GetSettings().mFullscreen)
        {
            int num_modes = 0;
            SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(current_display_id, &num_modes);

            for (int i = 0; i < num_display_modes; i++)
            {
                SDL_DisplayMode* display_mode = display_modes[i];
                display_mode_strings[i] = std::format("{}x{}@{}Hz", display_mode->w, display_mode->h, display_mode->refresh_rate);
            }

            const SDL_DisplayMode* current_display_mode = SDL_GetCurrentDisplayMode(current_display_id);

            if (ImGui::BeginCombo("Resolution", display_mode_strings[m_Renderer.GetSettings().mDisplayMode].c_str()))
            {
                for (int index = 0; index < display_mode_strings.size(); index++)
                {
                    if (ImGui::Selectable(display_mode_strings[index].c_str(), index == m_Renderer.GetSettings().mDisplayMode))
                    {
                        m_Renderer.GetSettings().mDisplayMode = index;
                        new_display_mode = display_modes[index];

                        if (!SDL_SetWindowFullscreenMode(window, new_display_mode))
                            gLogError("SDL", "{}", SDL_GetError());

                        SDL_SyncWindow(window);

                        gLogInfo("App", "SDL_SetWindowDisplayMode with {}x{} @ {}Hz", new_display_mode->w, new_display_mode->h, new_display_mode->refresh_rate);

                        m_Renderer.SetShouldResize(true);
                    }
                }

                ImGui::EndCombo();
            }
        }

        ImGui::SliderInt("V-Sync", &m_Renderer.GetSettings().mEnableVsync, 0, 3);

        ImGui::InputInt("Target FPS", &m_Renderer.GetSettings().mTargetFps);

        ImGui::EndMenu();
    }

    if (0) 
    {
        ImGui::Text("Grass Settings");
        ImGui::DragFloat("Grass Bend", &RenderSettings::mGrassBend, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Grass Tilt", &RenderSettings::mGrassTilt, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat2("Wind Direction", &RenderSettings::mWindDirection[0], 0.01f, -10.0f, 10.0f, "%.1f");
        ImGui::NewLine();
    }

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Ray Tracing Settings"))
    {
        //ImGui::SeparatorText("Settings");
        ImGui::SeparatorText("Ray Tracing");

        ImGui::Checkbox("##Shadowstoggle", (bool*)&m_Renderer.GetSettings().mEnableShadows);

        ImGui::SameLine();

        if (ImGui::BeginMenu("Enable Shadows"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Sun Cone Angle", &m_Renderer.GetSettings().mSunConeAngle, 0.01f, 0.0f, 1.0f, "%.2f");

            ImGui::EndMenu();
        }

        ImGui::Checkbox("Enable Reflections", (bool*)&m_Renderer.GetSettings().mEnableReflections);

        ImGui::Checkbox("##GItoggle", (bool*)&m_Renderer.GetSettings().mEnableDDGI);

        ImGui::SameLine();

        if (ImGui::BeginMenu("Enable Indirect Diffuse"))
        {
            ImGui::SeparatorText("Debug Options");

            ImGui::Checkbox("Use Chebyshev Test", (bool*)&RenderSettings::mDDGIUseChebyshev);

            ImGui::Checkbox("Visualize Pure White Mode", (bool*)&m_Renderer.GetSettings().mDisableAlbedo);

            ImGui::Checkbox("##ddgiproberaydebug", (bool*)&m_Renderer.GetSettings().mDebugProbeRays);

            ImGui::SameLine();

            if (ImGui::BeginMenu("Visualize Indirect Diffuse Rays"))
            {
                ImGui::SeparatorText("Settings");

                ImGui::DragInt("X", &RenderSettings::mDDGIDebugProbe[0], 1, 0, RenderSettings::mDDGIProbeCount[0]);
                ImGui::DragInt("Y", &RenderSettings::mDDGIDebugProbe[1], 1, 0, RenderSettings::mDDGIProbeCount[1]);
                ImGui::DragInt("Z", &RenderSettings::mDDGIDebugProbe[2], 1, 0, RenderSettings::mDDGIProbeCount[2]);

                ImGui::EndMenu();
            }

            ImGui::Checkbox("##ddgiprobedebug", (bool*)&m_Renderer.GetSettings().mDebugProbes);

            ImGui::SameLine();

            if (ImGui::BeginMenu("Visualize Indirect Diffuse Probes"))
            {
                ImGui::SeparatorText("Settings");

                ImGui::DragFloat("Probe Radius", &RenderSettings::mDDGIDebugRadius, 0.01f, 0.01f, 10.0f, "%.2f");

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::Checkbox("##AOtoggle", (bool*)&m_Renderer.GetSettings().mEnableRTAO))
        {
            if (m_Renderer.GetSettings().mEnableRTAO)
                m_Renderer.GetSettings().mEnableGTAO = false;
        }

        ImGui::SameLine();

        if (ImGui::BeginMenu("Enable Ambient Occlusion"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Radius", &RenderSettings::mRTAORadius, 0.01f, 0.0f, 20.0f, "%.2f");
            ImGui::DragFloat("Intensity", &RenderSettings::mRTAOPower, 0.01f, 0.0f, 10.0f, "%.2f");
            ImGui::DragFloat("Normal Bias", &RenderSettings::mRTAONormalBias, 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::SliderInt("Sample Count", (int*)&RenderSettings::mRTAOSampleCount, 1u, 128u);
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    ImGui::AlignTextToFramePadding();

    ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck | ImGuiItemFlags_AllowOverlap, true);

    if (ImGui::BeginMenu("Post Process Settings"))
    {
        ImGui::SeparatorText("Post Processing");

        ImGui::Checkbox("##SSRToggle", (bool*)&m_Renderer.GetSettings().mEnableSSR);
        ImGui::SameLine();

        ImGui::Text("SSR");

        if (ImGui::Checkbox("##GTAOtoggle", (bool*)&m_Renderer.GetSettings().mEnableGTAO))
        {
            if (m_Renderer.GetSettings().mEnableGTAO)
                m_Renderer.GetSettings().mEnableRTAO = false;
        }

        ImGui::SameLine();

        if (ImGui::BeginMenu("GTAO"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Radius", &RenderSettings::mGTAORadius, 0.01f, 0.01f, 10.0f, "%.2f");
            ImGui::DragFloat("Thickness", &RenderSettings::mGTAOThickness, 0.01f, 0.01f, 5.0f, "%.2f");
            ImGui::DragFloat("Intensity", &RenderSettings::mGTAOPower, 0.01f, 0.0f, 10.0f, "%.2f");
            ImGui::SliderInt("Slices", &RenderSettings::mGTAOSliceCount, 1, 8);
            ImGui::SliderInt("Steps", &RenderSettings::mGTAOStepCount, 1, 32);

            ImGui::EndMenu();
        }

        ImGui::Checkbox("##Bloomtoggle", (bool*)&m_Renderer.GetSettings().mEnableBloom);
        ImGui::SameLine();

        if (ImGui::BeginMenu("Bloom"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Blend Factor", &RenderSettings::mBloomBlendFactor, 0.01f, 0.0f, 0.5f, "%.2f");

            ImGui::EndMenu();
        }

        ImGui::Checkbox("##Vignettetoggle", (bool*)&m_Renderer.GetSettings().mEnableVignette);
        ImGui::SameLine();

        if (ImGui::BeginMenu("Vignette"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Scale", &RenderSettings::mVignetteScale, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Bias", &RenderSettings::mVignetteBias, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Inner Radius", &RenderSettings::mVignetteInner, 0.01f, 0.0f, RenderSettings::mVignetteOuter, "%.2f");
            ImGui::DragFloat("Outer Radius", &RenderSettings::mVignetteOuter, 0.01f, RenderSettings::mVignetteInner, 2.0f, "%.2f");

            ImGui::EndMenu();
        }

        ImGui::Checkbox("##DOFtoggle", (bool*)&m_Renderer.GetSettings().mEnableDoF);
        ImGui::SameLine();


        if (ImGui::BeginMenu("Depth of Field"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::Checkbox("Auto Focus", &RenderSettings::mDoFAutoFocus);

            ImGui::BeginDisabled(RenderSettings::mDoFAutoFocus);
            ImGui::DragFloat("Focus Distance", &RenderSettings::mDoFFocusDistance, 0.05f, inApp->GetViewport().GetNear(), inApp->GetViewport().GetFar(), "%.2f m");
            ImGui::EndDisabled();

            ImGui::DragFloat("Aperture", &RenderSettings::mDoFAperture, 0.05f, 0.7f, 22.0f, "f/%.1f");
            ImGui::EndMenu();
        }

        ImGui::Checkbox("Auto Exposure", (bool*)&m_Renderer.GetSettings().mEnableAutoExposure);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Currently not implemented.");

        if (ImGui::DragFloat("Exposure", &RenderSettings::mEV100, 0.05f, -6.0f, 20.0f, "EV100 %.2f"))
            RenderSettings::mPathTraceReset = true;
        ImGui::DragFloat("Chromatic Aberration", &RenderSettings::mChromaticAberrationStrength, 0.01f, 0.0f, 10.0f, "%.2f");

        ImGui::EndMenu();
    }

    ImGui::PopItemFlag();
}

} // namespace RK::DX12
