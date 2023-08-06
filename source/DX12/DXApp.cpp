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
    m_RenderInterface(m_Device, m_Renderer, m_StagingHeap)
{
    RTTIFactory::Register(RTTI_OF(ShaderProgram));
    RTTIFactory::Register(RTTI_OF(SystemShadersDX12));
    RTTIFactory::Register(RTTI_OF(EShaderProgramType));

    EShaderProgramType program_type = EShaderProgramType::SHADER_PROGRAM_GRAPHICS;
    std::cout << gToString(program_type) << '\n';

    program_type = gFromString("SHADER_PROGRAM_COMPUTE");
    std::cout << gToString(program_type) << '\n';

    Timer timer;

    auto json_data = JSON::JSONData("shaders.json");
    auto read_archive = JSON::ReadArchive(json_data);
    if (!json_data.IsEmpty())
        read_archive >> g_SystemShaders;

    // Wait for all the shaders to compile before continuing with renderer init
    g_SystemShaders.OnCompile();
    g_ThreadPool.WaitForJobs();

    //{
    //    auto write_archive = BinaryWriteArchive("shaders.bin");
    //    write_archive << g_SystemShaders;
    //}

    //g_SystemShaders = {};

    //{
    //    auto read_archive = BinaryReadArchive("shaders.bin");
    //    read_archive >> g_SystemShaders;
    //}

    if (!g_SystemShaders.IsCompiled()) {
        SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR, "DX12 Error", "Failed to compile system shaders", m_Window);
        std::abort();
    }

    LogMessage(std::format("[CPU] Shader compilation took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // Creating the SRV for the blue noise texture at heap index 0 results in a 4x4 black square in the top left of the texture,
    // this is a hacky workaround. at least we get the added benefit of 0 being an 'invalid' index :D
    (void)m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Add(nullptr);
    
    auto blue_noise_samples = std::vector<Vec4>();
    blue_noise_samples.reserve(128 * 128);

    for (auto y = 0u; y < 128; y++) {
        for (auto x = 0u; x < 128; x++) {
            auto& sample = blue_noise_samples.emplace_back();
         
            for (auto i = 0u; i < sample.length(); i++)
                sample[i] = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, i);
        }
    }

    auto bluenoise_texture = m_Device.CreateTexture(Texture::Desc{
        .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .width  = 128,
        .height = 128,
        .usage  = Texture::Usage::SHADER_READ_ONLY
    }, L"BLUENOISE128x1spp");

    assert(m_Device.GetBindlessHeapIndex(bluenoise_texture) == BINDLESS_BLUE_NOISE_TEXTURE_INDEX);

    {
        auto& cmd_list = m_Renderer.StartSingleSubmit();

        m_StagingHeap.StageTexture(cmd_list, m_Device.GetTexture(bluenoise_texture).GetResource(), 0, blue_noise_samples.data());

        m_Renderer.FlushSingleSubmit(m_Device, cmd_list);
    }

    // Create default textures / assets
    const auto black_texture_file = TextureAsset::sConvert("assets/system/black4x4.png");
    const auto white_texture_file = TextureAsset::sConvert("assets/system/white4x4.png");
    const auto normal_texture_file = TextureAsset::sConvert("assets/system/normal4x4.png");
    m_DefaultBlackTexture  = DescriptorID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(black_texture_file)));
    m_DefaultWhiteTexture  = DescriptorID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(white_texture_file)));
    m_DefaultNormalTexture = DescriptorID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(normal_texture_file)));

    assert(m_DefaultBlackTexture.IsValid()  && m_DefaultBlackTexture.ToIndex()  != 0);
    assert(m_DefaultWhiteTexture.IsValid()  && m_DefaultWhiteTexture.ToIndex()  != 0);
    assert(m_DefaultNormalTexture.IsValid() && m_DefaultNormalTexture.ToIndex() != 0);

    Material::Default.gpuAlbedoMap = m_DefaultWhiteTexture.ToIndex();
    Material::Default.gpuNormalMap = m_DefaultNormalTexture.ToIndex();
    Material::Default.gpuMetallicRoughnessMap = m_DefaultWhiteTexture.ToIndex();

    // initialize ImGui
    ImGui_ImplSDL2_InitForD3D(m_Window);
    m_ImGuiFontTextureID = InitImGui(m_Device, Renderer::sSwapchainFormat, sFrameCount);

    // Pick a scene file and load it
    while (!FileSystem::exists(m_Settings.mSceneFile))
        m_Settings.mSceneFile = FileSystem::relative(OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0")).make_preferred().string();

    SDL_SetWindowTitle(m_Window, std::string(m_Settings.mSceneFile + " - Raekor Renderer").c_str());
    m_Scene.OpenFromFile(m_Assets, m_Settings.mSceneFile);

    assert(!m_Scene.empty() && "Scene cannot be empty when starting up DX12 renderer!!");

    // check for ray-tracing support
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    gThrowIfFailed(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));

    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1) {
        SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR, "DX12 Error", "GPU does not support D3D12_RAYTRACING_TIER_1_1.", m_Window);
        abort();
    }

    // initialize DirectStorage 1.0
    auto queue_desc = DSTORAGE_QUEUE_DESC {
        .SourceType = DSTORAGE_REQUEST_SOURCE_FILE,
        .Capacity   = DSTORAGE_MAX_QUEUE_CAPACITY,
        .Priority   = DSTORAGE_PRIORITY_NORMAL,
        .Device     = m_Device,
    };

    auto storage_factory = ComPtr<IDStorageFactory>{};
    gThrowIfFailed(DStorageGetFactory(IID_PPV_ARGS(&storage_factory)));

    gThrowIfFailed(storage_factory->CreateQueue(&queue_desc, IID_PPV_ARGS(&m_FileStorageQueue)));

    queue_desc.SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY;
    gThrowIfFailed(storage_factory->CreateQueue(&queue_desc, IID_PPV_ARGS(&m_MemoryStorageQueue)));

    {
        auto& cmd_list = m_Renderer.StartSingleSubmit();

        UploadBindlessSceneBuffers(cmd_list);

        UploadTopLevelBVH(cmd_list);

        m_Renderer.FlushSingleSubmit(m_Device, cmd_list);
    }

    LogMessage(std::format("[CPU] Upload BVH to GPU took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    const auto debug_texture = EDebugTexture(m_RenderInterface.GetSettings().mDebugTexture);
    const auto instances_buffer = m_Device.GetBuffer(m_InstancesBuffer).GetView();
    const auto materials_buffer = m_Device.GetBuffer(m_MaterialsBuffer).GetView();
    m_Renderer.Recompile(m_Device, m_Scene, m_TLASDescriptor, instances_buffer, materials_buffer, debug_texture);

    LogMessage(std::format("[CPU] RenderGraph compilation took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    LogMessage(std::format("Render Size: {}", glm::to_string(m_Viewport.GetSize())));
}


DXApp::~DXApp() {
    m_Renderer.WaitForIdle(m_Device);
    m_Device.ReleaseTextureImmediate(m_ImGuiFontTextureID);
}


void DXApp::OnUpdate(float inDeltaTime) {
    IEditor::OnUpdate(inDeltaTime);

    //UploadTopLevelBVH(m_Renderer.GetBackBufferData().mCmdList);

    const auto debug_texture = EDebugTexture(m_RenderInterface.GetSettings().mDebugTexture);
    const auto instances_buffer = m_Device.GetBuffer(m_InstancesBuffer).GetView();
    const auto materials_buffer = m_Device.GetBuffer(m_MaterialsBuffer).GetView();
    m_Renderer.OnRender(m_Device, m_Viewport, m_Scene, m_StagingHeap, m_TLASDescriptor, instances_buffer, materials_buffer, debug_texture, inDeltaTime);
}


void DXApp::OnEvent(const SDL_Event& inEvent) {
    IEditor::OnEvent(inEvent);

    if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat) {
        // ALT + ENTER event (Windowed <-> Fullscreen toggle)
        if (inEvent.key.keysym.sym == SDLK_RETURN && SDL_GetModState() & KMOD_LALT) {
            // This only toggles between windowed and borderless fullscreen, exclusive fullscreen needs to be set from the menu
            if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
                SDL_SetWindowFullscreen(m_Window, 0);
            else
                SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_FULLSCREEN_DESKTOP);

            // SDL2 should have updated the window by now, so get the new size
            auto width = 0, height = 0;
            SDL_GetWindowSize(m_Window, &width, &height);

            // Updat the viewport and tell the renderer to resize to the viewport
            m_Viewport.SetSize(UVec2(width, height));
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


DescriptorID DXApp::UploadTextureDirectStorage(const TextureAsset::Ptr& inAsset, DXGI_FORMAT inFormat) {
    auto factory = ComPtr<IDStorageFactory>();
    gThrowIfFailed(DStorageGetFactory(IID_PPV_ARGS(&factory)));

    auto data_ptr = inAsset->GetData();
    const auto header_ptr = inAsset->GetHeader();
    // I think DirectStorage doesnt do proper texture data alignment for its upload buffers as we get garbage past the 4th mip
    const auto mipmap_levels = std::min(header_ptr->dwMipMapCount, 4ul);
    // const auto mipmap_levels = header->dwMipMapCount;

    auto texture = m_Device.GetTexture(m_Device.CreateTexture(Texture::Desc { 
        .format = inFormat, 
        .width = header_ptr->dwWidth,
        .height = header_ptr->dwHeight,
        .mipLevels = mipmap_levels,
        .usage = Texture::SHADER_READ_ONLY
    }, inAsset->GetPath().wstring().c_str()));

    auto requests = std::vector<DSTORAGE_REQUEST>(mipmap_levels);

    for (auto [mip, request] : gEnumerate(requests)) {
        const auto dimensions = glm::ivec2(std::max(header_ptr->dwWidth >> mip, 1ul), std::max(header_ptr->dwHeight >> mip, 1ul));
        const auto data_size = std::max(1u, ((dimensions.x + 3u) / 4u)) * std::max(1u, ((dimensions.y + 3u) / 4u)) * 16u;

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
                    .Resource = texture.GetResource().Get(),
                    .SubresourceIndex = uint32_t(mip),
                    .Region = CD3DX12_BOX(0, 0, dimensions.x, dimensions.y),
                }
            },
            .Name = inAsset->GetPath().string().c_str()
        };

        data_ptr += dimensions.x * dimensions.y;

        m_MemoryStorageQueue->EnqueueRequest(&request);
    }

    auto fence = ComPtr<ID3D12Fence>{};
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



void DXApp::UploadBindlessSceneBuffers(CommandList& inCmdList) {
    auto materials = m_Scene.view<Material>();

    auto MaterialIndex = [&](entt::entity m) -> uint32_t {
        uint32_t i = 0;

        for (const auto& [entity, material] : materials.each()) {
            if (entity == m) {
                return i;
            }

            i++;
        }

        return 0;
    };

    std::vector<RTGeometry> rt_geometries;
    auto meshes = m_Scene.view<Mesh, Transform>();
    rt_geometries.reserve(meshes.size_hint());

    for (const auto& [entity, mesh, transform] : meshes.each()) {
        if (!BufferID(mesh.BottomLevelAS).IsValid())
            continue;

        rt_geometries.emplace_back(RTGeometry {
            .mIndexBuffer           = m_Device.GetBindlessHeapIndex(m_Device.GetBuffer(BufferID(mesh.indexBuffer)).GetView()),
            .mVertexBuffer          = m_Device.GetBindlessHeapIndex(m_Device.GetBuffer(BufferID(mesh.vertexBuffer)).GetView()),
            .mMaterialIndex         = MaterialIndex(mesh.material),
            .mLocalToWorldTransform = transform.worldTransform
        });
    }

    {
        m_InstancesBuffer = m_Device.CreateBuffer(Buffer::Desc {
            .size   = sizeof(RTGeometry) * meshes.size_hint(),
            .stride = sizeof(RTGeometry),
            .usage  = Buffer::Usage::UPLOAD,
        }, L"RT_INSTANCE_BUFFER");
    }

    {
        uint8_t* mapped_ptr;
        auto buffer_range = CD3DX12_RANGE(0, 0);
        gThrowIfFailed(m_Device.GetBuffer(m_InstancesBuffer)->Map(0, &buffer_range, reinterpret_cast<void**>(&mapped_ptr)));
        memcpy(mapped_ptr, rt_geometries.data(), rt_geometries.size() * sizeof(rt_geometries[0]));
        m_Device.GetBuffer(m_InstancesBuffer)->Unmap(0, nullptr);
    }

    std::vector<RTMaterial> rt_materials(materials.size());

    auto i = 0u;
    for (const auto& [entity, material] : materials.each()) {
        auto& rt_material = rt_materials[i];
        rt_material.mAlbedo = material.albedo;
        
        rt_material.mAlbedoTexture = material.gpuAlbedoMap;
        rt_material.mNormalsTexture = material.gpuNormalMap;
        rt_material.mMetalRoughTexture = material.gpuMetallicRoughnessMap;
        
        rt_material.mMetallic = material.metallic;
        rt_material.mRoughness = material.roughness;
        
        rt_material.mEmissive = Vec4(material.emissive, 1.0);

        assert(rt_material.mAlbedoTexture != 0 && rt_material.mNormalsTexture != 0 && rt_material.mMetalRoughTexture != 0);

        i++;
    }

    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.StructureByteStride = sizeof(RTMaterial);
        srv_desc.Buffer.NumElements = rt_materials.size();

        m_MaterialsBuffer = m_Device.CreateBuffer(Buffer::Desc {
            .size   = sizeof(RTMaterial) * rt_materials.size(),
            .stride = sizeof(RTMaterial),
            .usage  = Buffer::Usage::UPLOAD,
            .viewDesc = &srv_desc
        }, L"RT_MATERIAL_BUFFER");
    }


    {
        uint8_t* mapped_ptr;
        auto buffer_range = CD3DX12_RANGE(0, 0);
        gThrowIfFailed(m_Device.GetBuffer(m_MaterialsBuffer)->Map(0, &buffer_range, reinterpret_cast<void**>(&mapped_ptr)));
        memcpy(mapped_ptr, rt_materials.data(), rt_materials.size() * sizeof(rt_materials[0]));
        m_Device.GetBuffer(m_MaterialsBuffer)->Unmap(0, nullptr);
    }
}



void DXApp::UploadTopLevelBVH(CommandList& inCmdList) {
    auto meshes = m_Scene.view<Mesh, Transform>();

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> rt_instances;
    rt_instances.reserve(meshes.size_hint());

    auto instance_id = 0u;
    for (const auto& [entity, mesh, transform] : meshes.each()) {
        const auto& blas_buffer = m_Device.GetBuffer(BufferID(mesh.BottomLevelAS));

        D3D12_RAYTRACING_INSTANCE_DESC instance = {};
        instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
        instance.InstanceMask = 0xFF;
        instance.InstanceID = instance_id++;
        instance.AccelerationStructure = blas_buffer->GetGPUVirtualAddress();

        glm::mat4 transpose = glm::transpose(transform.worldTransform); // TODO: transform buffer
        memcpy(instance.Transform, glm::value_ptr(transpose), sizeof(instance.Transform));

        rt_instances.push_back(instance);
    }

    const auto instance_buffer_id = m_Device.CreateBuffer(Buffer::Desc{
        .size = rt_instances.size() * sizeof(rt_instances[0]),
        .usage = Buffer::UPLOAD,
    });

    auto& instance_buffer = m_Device.GetBuffer(instance_buffer_id);

    {
        uint8_t* mapped_ptr;
        auto buffer_range = CD3DX12_RANGE(0, 0);
        gThrowIfFailed(instance_buffer->Map(0, &buffer_range, reinterpret_cast<void**>(&mapped_ptr)));
        memcpy(mapped_ptr, rt_instances.data(), rt_instances.size() * sizeof(rt_instances[0]));
        instance_buffer->Unmap(0, nullptr);
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.InstanceDescs = instance_buffer->GetGPUVirtualAddress(); // not used in GetRaytracingAccelerationStructurePrebuildInfo
    inputs.NumDescs = rt_instances.size();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    auto scratch_buffer = m_Device.CreateBuffer(Buffer::Desc {
        .size = prebuild_info.ScratchDataSizeInBytes
    }, L"TLAS_SCRATCH_BUFFER");

    m_TLASBuffer = m_Device.CreateBuffer(Buffer::Desc {
        .size = prebuild_info.ResultDataMaxSizeInBytes,
        .usage = Buffer::Usage::ACCELERATION_STRUCTURE,
    }, L"TLAS_FULL_SCENE");

    LogMessage(std::format("[GPU] TLAS Buffer size is {:.1f} KBs", float(prebuild_info.ResultDataMaxSizeInBytes) / 1000));

    const auto result_buffer = m_Device.GetBuffer(m_TLASBuffer);

    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srv_desc.RaytracingAccelerationStructure.Location = result_buffer->GetGPUVirtualAddress();

        // pResource must be NULL, since the resource location comes from a GPUVA in pDesc
        m_TLASDescriptor = m_Device.CreateShaderResourceView(nullptr, &srv_desc);
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.ScratchAccelerationStructureData = m_Device.GetBuffer(scratch_buffer)->GetGPUVirtualAddress();
    desc.DestAccelerationStructureData = result_buffer->GetGPUVirtualAddress();
    desc.Inputs = inputs;

    auto& cmd_list = m_Renderer.GetBackBufferData().mCmdList;
    
    cmd_list->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    m_Device.ReleaseBuffer(scratch_buffer);
    m_Device.ReleaseBuffer(instance_buffer_id);
}


}