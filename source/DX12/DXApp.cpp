#include "pch.h"
#include "DXApp.h"

#include <locale>
#include <codecvt>

#include "Raekor/OS.h"
#include "Raekor/gui.h"
#include "Raekor/util.h"
#include "Raekor/timer.h"
#include "Raekor/input.h"

#include "shared.h"
#include "DXUtil.h"
#include "DXRenderer.h"
#include "DXRenderGraph.h"


namespace Raekor::DX12 {

DXApp::DXApp() : 
    Application(WindowFlag::RESIZE), 
    m_Device(m_Window, sFrameCount), 
    m_StagingHeap(m_Device),
    m_Renderer(m_Device, m_Viewport, m_Window)
{
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForD3D(m_Window);
    ImGui::GetIO().IniFilename = "";
    GUI::SetTheme(m_Settings.themeColors);

    m_ImGuiFontTextureID = InitImGui(m_Device, Renderer::sSwapchainFormat, sFrameCount);

    while (!FileSystem::exists(m_Settings.defaultScene))
        m_Settings.defaultScene = FileSystem::relative(OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0")).string();

    SDL_SetWindowTitle(m_Window, std::string(m_Settings.defaultScene + " - Raekor Renderer").c_str());
    m_Scene.OpenFromFile(m_Assets, m_Settings.defaultScene);

    assert(!m_Scene.empty() && "Scene cannot be empty when starting up DX12 renderer!!");

    //m_Viewport.GetCamera().Zoom(-50.0f);
    //m_Viewport.GetCamera().Move(glm::vec2(0.0f, 10.0f));

    // Pre-transform the scene, we don't support matrix transformations yet
    for (auto [entity, transform, mesh] : m_Scene.view<Transform, Mesh>().each()) {
        for (auto& p : mesh.positions)
            p = transform.worldTransform * glm::vec4(p, 1.0);

        for (auto& n : mesh.normals)
            n = glm::normalize(glm::mat3(glm::transpose(glm::inverse(transform.worldTransform))) * n);

        for (auto& t : mesh.tangents)
            t = glm::normalize(transform.worldTransform * glm::vec4(t, 0.0));
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    gThrowIfFailed(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));

    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1) {
        SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR, "DX12 Error", "GPU does not support D3D12_RAYTRACING_TIER_1_1.", m_Window);
        abort();
    }

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

    UploadSceneToGPU();
    
    CompileShaders();

    UploadBvhToGPU();

    m_Renderer.Recompile(m_Device, m_Scene, m_TLASDescriptor, m_Device.GetBuffer(m_InstancesBuffer).GetView(), m_Device.GetBuffer(m_MaterialsBuffer).GetView());

    std::cout << "Render Size: " << m_Viewport.size.x << " , " << m_Viewport.size.y << '\n';

    m_Viewport.GetCamera().Move(Vec2(42.0f, 10.0f));
    m_Viewport.GetCamera().Zoom(5.0f);
    m_Viewport.GetCamera().Look(Vec2(1.65f, 0.2f));
}


DXApp::~DXApp() {
    m_Renderer.WaitForIdle(m_Device);
    m_Device.ReleaseTextureImmediate(m_ImGuiFontTextureID);
}


void DXApp::OnUpdate(float inDeltaTime) {
    m_Viewport.OnUpdate(inDeltaTime);

    m_Scene.UpdateLights();

    m_Renderer.OnRender(m_Device, m_Viewport, m_Scene, m_TLASDescriptor, m_Device.GetBuffer(m_InstancesBuffer).GetView(), m_Device.GetBuffer(m_MaterialsBuffer).GetView(), inDeltaTime);
}


void DXApp::OnEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    if (event.button.button == 2 || event.button.button == 3) {
        if (event.type == SDL_MOUSEBUTTONDOWN)
            SDL_SetRelativeMouseMode(SDL_TRUE);
        else if (event.type == SDL_MOUSEBUTTONUP)
            SDL_SetRelativeMouseMode(SDL_FALSE);
    }

    if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym == SDLK_LSHIFT) {
        m_Viewport.GetCamera().zoomConstant *= 20.0f;
        m_Viewport.GetCamera().moveConstant *= 20.0f;
    }

    if (event.type == SDL_KEYUP && !event.key.repeat && event.key.keysym.sym == SDLK_LSHIFT) {
        m_Viewport.GetCamera().zoomConstant /= 20.0f;
        m_Viewport.GetCamera().moveConstant /= 20.0f;
    }

    if (!ImGui::GetIO().WantCaptureMouse) {
        auto& camera = m_Viewport.GetCamera();

        if (event.type == SDL_MOUSEMOTION) {
            if (SDL_GetRelativeMouseMode() && Input::sIsButtonPressed(3)) {
                auto formula = glm::radians(0.022f * camera.sensitivity * 2.0f);
                camera.Look(glm::vec2(event.motion.xrel * formula, event.motion.yrel * formula));
            }
            else if (SDL_GetRelativeMouseMode() && Input::sIsButtonPressed(2))
                camera.Move(glm::vec2(event.motion.xrel * 0.02f, event.motion.yrel * 0.02f));
        }
        else if (event.type == SDL_MOUSEWHEEL)
            camera.Zoom(float(event.wheel.y));
    }


    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            while (1) {
                auto temp_event = SDL_Event{};
                SDL_PollEvent(&temp_event);

                if (temp_event.window.event == SDL_WINDOWEVENT_RESTORED)
                    break;
            }
        }
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
            if (SDL_GetWindowID(m_Window) == event.window.windowID)
                m_Running = false;
        }
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            auto w = 0, h = 0;
            SDL_GetWindowSize(m_Window, &w, &h);
            m_Viewport.Resize(glm::uvec2(w, h));
            m_Renderer.OnResize(m_Device, m_Viewport);
            m_Renderer.Recompile(m_Device, m_Scene, m_TLASDescriptor, m_Device.GetBuffer(m_InstancesBuffer).GetView(), m_Device.GetBuffer(m_MaterialsBuffer).GetView());
        }
    }
}


void DXApp::CompileShaders() {
    if (!FileSystem::exists("assets/system/shaders/DirectX/bin"))
        FileSystem::create_directory("assets/system/shaders/DirectX/bin");

    auto timeOfMostRecentlyUpdatedIncludeFile = FileSystem::file_time_type{};

    for (const auto& file : FileSystem::directory_iterator("assets/system/shaders/DirectX/include")) {
        const auto updateTime = FileSystem::last_write_time(file);

        if (updateTime > timeOfMostRecentlyUpdatedIncludeFile)
            timeOfMostRecentlyUpdatedIncludeFile = updateTime;
    }

    for (const auto& file : FileSystem::directory_iterator("assets/system/shaders/DirectX")) {
        if (file.is_directory() || file.path().extension() != ".hlsl")
            continue;

        //Async::sQueueJob([&]() {
            m_Device.m_Shaders[file.path().stem().string()] = Device::ShaderEntry {
                .mPath = file.path(),
                .mBlob = sCompileShaderDXC(file.path()),
                .mLastWriteTime = FileSystem::last_write_time(file)
            };
        //});
    }

    // Wait for shader compilation to finish before continuing on with pipeline creation
    Async::sWait();
}


void DXApp::UploadSceneToGPU() {
    auto& cmd_list = m_Renderer.StartSingleSubmit();

    for (const auto& [entity, mesh] : m_Scene.view<Mesh>().each()) {
        const auto vertices = mesh.GetInterleavedVertices();
        const auto vertices_size = vertices.size() * sizeof(vertices[0]);
        const auto indices_size = mesh.indices.size() * sizeof(mesh.indices[0]);

        if (!vertices_size || !indices_size)
            continue;

        mesh.indexBuffer = m_Device.CreateBuffer(Buffer::Desc{
            .size   = uint32_t(indices_size),
            .stride = sizeof(uint32_t) * 3,
            .usage  = Buffer::Usage::INDEX_BUFFER
        }).ToIndex();

        mesh.vertexBuffer = m_Device.CreateBuffer(Buffer::Desc{
            .size   = uint32_t(vertices_size),
            .stride = sizeof(Vertex),
            .usage  = Buffer::Usage::VERTEX_BUFFER
        }).ToIndex();

        auto& index_buffer = m_Device.GetBuffer(BufferID(mesh.indexBuffer));
        m_StagingHeap.StageBuffer(cmd_list, index_buffer.GetResource(), 0, mesh.indices.data(), indices_size);

        auto& vertex_buffer = m_Device.GetBuffer(BufferID(mesh.vertexBuffer));
        m_StagingHeap.StageBuffer(cmd_list, vertex_buffer.GetResource(), 0, vertices.data(), vertices_size);
    }

    m_Renderer.FlushSingleSubmit(m_Device, cmd_list);

    const auto black_texture_file = TextureAsset::sConvert("assets/system/black4x4.png");
    const auto white_texture_file = TextureAsset::sConvert("assets/system/white4x4.png");
    const auto normal_texture_file = TextureAsset::sConvert("assets/system/normal4x4.png");
    m_DefaultBlackTexture = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(black_texture_file), DXGI_FORMAT_BC3_UNORM);
    m_DefaultWhiteTexture = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(white_texture_file), DXGI_FORMAT_BC3_UNORM);
    const auto default_normal_texture = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(normal_texture_file), DXGI_FORMAT_BC3_UNORM);

    Material::Default.gpuAlbedoMap = m_DefaultWhiteTexture.ToIndex();
    Material::Default.gpuNormalMap = default_normal_texture.ToIndex();
    Material::Default.gpuMetallicRoughnessMap = m_DefaultWhiteTexture.ToIndex();

    for (const auto& [entity, material] : m_Scene.view<Material>().each()) {
        if (const auto asset = m_Assets.Get<TextureAsset>(material.albedoFile))
            material.gpuAlbedoMap = QueueDirectStorageLoad(asset, DXGI_FORMAT_BC3_UNORM_SRGB).ToIndex();
        else
            material.gpuAlbedoMap = m_DefaultWhiteTexture.ToIndex();

        if (const auto asset = m_Assets.Get<TextureAsset>(material.normalFile))
            material.gpuNormalMap = QueueDirectStorageLoad(asset, DXGI_FORMAT_BC3_UNORM).ToIndex();
        else
            material.gpuNormalMap = default_normal_texture.ToIndex();

        if (const auto asset = m_Assets.Get<TextureAsset>(material.metalroughFile))
            material.gpuMetallicRoughnessMap = QueueDirectStorageLoad(asset, DXGI_FORMAT_BC3_UNORM).ToIndex();
        else
            material.gpuMetallicRoughnessMap = m_DefaultWhiteTexture.ToIndex();
    }
}


DescriptorID DXApp::QueueDirectStorageLoad(const TextureAsset::Ptr& inAsset, DXGI_FORMAT inFormat) {
    auto factory = ComPtr<IDStorageFactory>();
    gThrowIfFailed(DStorageGetFactory(IID_PPV_ARGS(&factory)));

    auto file = ComPtr<IDStorageFile>();
    if (FileSystem::exists(inAsset->GetPath()))
        gThrowIfFailed(factory->OpenFile(inAsset->GetPath().wstring().c_str(), IID_PPV_ARGS(&file)));

    const auto queue = FileSystem::exists(inAsset->GetPath()) ? m_FileStorageQueue : m_MemoryStorageQueue;

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

    auto width = header_ptr->dwWidth, height = header_ptr->dwHeight;
    auto data_offset = sizeof(DDS_MAGIC) + sizeof(DDS_HEADER);
    auto requests = std::vector<DSTORAGE_REQUEST>(mipmap_levels);

    for (auto [mip, request] : gEnumerate(requests)) {
        request = DSTORAGE_REQUEST {
            .Options = DSTORAGE_REQUEST_OPTIONS {
                .SourceType = DSTORAGE_REQUEST_SOURCE_FILE,
                .DestinationType = DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION,
            },
            .Source = DSTORAGE_SOURCE {
                .File = DSTORAGE_SOURCE_FILE {
                    .Source = file.Get(),
                    .Offset = data_offset,
                    .Size = width * height,
                },
            },
            .Destination = DSTORAGE_DESTINATION {
                .Texture = DSTORAGE_DESTINATION_TEXTURE_REGION {
                    .Resource = texture.GetResource().Get(),
                    .SubresourceIndex = uint32_t(mip),
                    .Region = CD3DX12_BOX(0, 0, width, height),
                }
            },
            .Name = inAsset->GetPath().string().c_str()
        };

        if (!FileSystem::exists(inAsset->GetPath())) {
            request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY;

            const auto dimensions = glm::ivec2(std::max(header_ptr->dwWidth >> mip, 1ul), std::max(header_ptr->dwHeight >> mip, 1ul));
            const auto data_size = std::max(1u, ((dimensions.x + 3u) / 4u)) * std::max(1u, ((dimensions.y + 3u) / 4u)) * 16u;

            request.Source = DSTORAGE_SOURCE { 
                DSTORAGE_SOURCE_MEMORY {
                    .Source = data_ptr,
                    .Size = data_size
            }};

            data_ptr += dimensions.x * dimensions.y;
        }


        width /= 2, height /= 2;
        data_offset += request.Source.File.Size;
        
        queue->EnqueueRequest(&request);
    }

    auto fence = ComPtr<ID3D12Fence>{};
    gThrowIfFailed(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));

    auto fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    constexpr auto fence_value = 1ul;
    gThrowIfFailed(fence->SetEventOnCompletion(fence_value, fence_event));
    queue->EnqueueSignal(fence.Get(), fence_value);

    // Tell DirectStorage to start executing all queued items.
    queue->Submit();

    // Wait for the submitted work to complete
    std::cout << "Waiting for the DirectStorage request to complete...\n";
    WaitForSingleObject(fence_event, INFINITE);
    CloseHandle(fence_event);

    return texture.GetView();
}


void DXApp::UploadBvhToGPU() {
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    gThrowIfFailed(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));

    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1) {
        SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR, "Raekor Error", "GPU does not support D3D12_RAYTRACING_TIER_1_1.", m_Window);
        abort();
    }

    for (const auto& [entity, mesh] : m_Scene.view<Mesh>().each()) {
        const auto& gpu_index_buffer = m_Device.GetBuffer(BufferID(mesh.indexBuffer));
        const auto& gpu_vertex_buffer = m_Device.GetBuffer(BufferID(mesh.vertexBuffer));

        D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
        geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        geom.Triangles.IndexBuffer = gpu_index_buffer->GetGPUVirtualAddress();
        geom.Triangles.IndexCount = mesh.indices.size();
        geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

        geom.Triangles.VertexBuffer.StartAddress = gpu_vertex_buffer->GetGPUVirtualAddress();
        geom.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
        geom.Triangles.VertexCount = mesh.positions.size();
        geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.pGeometryDescs = &geom;
        inputs.NumDescs = 1;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
        m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

        auto& component = m_Scene.emplace<AccelerationStructure>(entity);
        component.buffer = m_Device.CreateBuffer(Buffer::Desc{
            .size = prebuild_info.ResultDataMaxSizeInBytes,
            .usage = Buffer::Usage::ACCELERATION_STRUCTURE
        });

        const auto scratch_buffer_id = m_Device.CreateBuffer(Buffer::Desc{
            .size = prebuild_info.ScratchDataSizeInBytes
        });

        auto& scratch_buffer = m_Device.GetBuffer(scratch_buffer_id);
        auto& result_buffer = m_Device.GetBuffer(component.buffer);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
        desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();
        desc.DestAccelerationStructureData = result_buffer->GetGPUVirtualAddress();
        desc.Inputs = inputs;

        auto& cmd_list = m_Renderer.StartSingleSubmit();

        cmd_list->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

        m_Renderer.FlushSingleSubmit(m_Device, cmd_list);

        m_Device.ReleaseBuffer(scratch_buffer_id);
    }

    auto meshes = m_Scene.view<Mesh, Transform, AccelerationStructure>();

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> deviceInstances;
    deviceInstances.reserve(meshes.size_hint());

    uint32_t customIndex = 0;
    for (const auto& [entity, mesh, transform, accel_struct] : meshes.each()) {
        const auto& blas_buffer = m_Device.GetBuffer(accel_struct.buffer);

        D3D12_RAYTRACING_INSTANCE_DESC instance = {};
        instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
        instance.InstanceMask = 0xFF;
        instance.InstanceID = customIndex++;
        instance.AccelerationStructure = blas_buffer->GetGPUVirtualAddress();

        glm::mat4 transpose = glm::transpose(glm::mat4(1.0f)); // TODO: transform buffer
        memcpy(instance.Transform, glm::value_ptr(transpose), sizeof(instance.Transform));

        deviceInstances.push_back(instance);
    }

    auto instance_buffer = m_Device.GetBuffer(m_Device.CreateBuffer(Buffer::Desc{
        .size = deviceInstances.size() * sizeof(deviceInstances[0]),
        .usage = Buffer::UPLOAD,
    }));

    {
        uint8_t* mapped_ptr;
        auto buffer_range = CD3DX12_RANGE(0, 0);
        gThrowIfFailed(instance_buffer->Map(0, &buffer_range, reinterpret_cast<void**>(&mapped_ptr)));
        memcpy(mapped_ptr, deviceInstances.data(), deviceInstances.size() * sizeof(deviceInstances[0]));
        instance_buffer->Unmap(0, nullptr);
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.InstanceDescs = instance_buffer->GetGPUVirtualAddress();
    inputs.NumDescs = deviceInstances.size();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    auto scratch_buffer = m_Device.CreateBuffer(Buffer::Desc { 
        .size = prebuild_info.ScratchDataSizeInBytes 
    }, L"TLAS_SCRATCH_BUFFER");

    m_TLASBuffer = m_Device.CreateBuffer(Buffer::Desc {
        .size  = prebuild_info.ResultDataMaxSizeInBytes,
        .usage = Buffer::Usage::ACCELERATION_STRUCTURE,
    }, L"TLAS_FULL_SCENE");

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

    auto& cmd_list = m_Renderer.StartSingleSubmit();

    cmd_list->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    m_Renderer.FlushSingleSubmit(m_Device, cmd_list);

    m_Device.ReleaseBuffer(scratch_buffer);

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
    rt_geometries.reserve(meshes.size_hint());

    for (const auto& [entity, mesh, transform, blas] : meshes.each()) {
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


}