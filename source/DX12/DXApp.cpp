#include "pch.h"
#include "DXApp.h"
#include "DXUtil.h"
#include "Raekor/OS.h"
#include "Raekor/util.h"
#include "Raekor/timer.h"
#include "Raekor/input.h"
#include "Raekor/gui.h"
#include <locale>
#include <codecvt>

namespace Raekor::DX {

enum ShaderStage {
    VERTEX, PIXEL, COMPUTE, COUNT
};

constexpr std::array shaderEntryName = {
    L"VSMain", L"PSMain", L"CSMain"
};

DXApp::DXApp() : 
    Application(RendererFlags::NONE), 
    m_Device(m_Window), 
    m_StagingHeap(m_Device) 
{
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForD3D(m_Window);
    ImGui::GetIO().IniFilename = "";
    GUI::SetTheme(m_Settings.themeColors);

    while (!fs::exists(m_Settings.defaultScene))
        m_Settings.defaultScene = fs::relative(OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0")).string();

    SDL_SetWindowTitle(m_Window, std::string(m_Settings.defaultScene + " - Raekor Renderer").c_str());
    m_Scene.OpenFromFile(m_Assets, m_Settings.defaultScene);

    assert(!m_Scene.empty() && "Scene cannot be empty when starting up DX12 renderer!!");

    m_Viewport.GetCamera().Zoom(-50.0f);
    m_Viewport.GetCamera().Move(glm::vec2(0.0f, 10.0f));

    DSTORAGE_QUEUE_DESC queue_desc = {};
    queue_desc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
    queue_desc.Priority = DSTORAGE_PRIORITY_NORMAL;
    queue_desc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queue_desc.Device = m_Device;

    ComPtr<IDStorageFactory> storage_factory;
    gThrowIfFailed(DStorageGetFactory(IID_PPV_ARGS(&storage_factory)));

    gThrowIfFailed(storage_factory->CreateQueue(&queue_desc, IID_PPV_ARGS(&m_StorageQueue)));

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(m_Window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.BufferCount = sFrameCount;
    swapchainDesc.Width = m_Viewport.size.x;
    swapchainDesc.Height = m_Viewport.size.y;
    swapchainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGIFactory4> factory;
    gThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGISwapChain1> swapchain;
    gThrowIfFailed(factory->CreateSwapChainForHwnd(m_Device.GetQueue(), wmInfo.info.win.window, &swapchainDesc, nullptr, nullptr, &swapchain));
    gThrowIfFailed(swapchain.As(&m_Swapchain));

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    for (uint32_t i = 0; i < sFrameCount; i++) {
        auto& backbuffer_data = m_BackbufferData[i];

        ResourceRef rtv_resource;
        gThrowIfFailed(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(rtv_resource.GetAddressOf())));
        backbuffer_data.mBackbufferRTV = m_Device.CreateRenderTargetView(rtv_resource);
        
        gThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&backbuffer_data.mCmdAllocator)));
        gThrowIfFailed(m_Device->CreateCommandList(0x00, D3D12_COMMAND_LIST_TYPE_DIRECT, backbuffer_data.mCmdAllocator.Get(), nullptr, IID_PPV_ARGS(&backbuffer_data.mCmdList)));
        backbuffer_data.mCmdList->Close();
    }

    m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent)
        gThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    auto& backbuffer_data = GetBackbufferData();
    backbuffer_data.mCmdList->Reset(backbuffer_data.mCmdAllocator.Get(), nullptr);

    for (const auto& [entity, mesh] : m_Scene.view<Mesh>().each()) {
        const auto vertices = mesh.GetInterleavedVertices();
        const auto vertices_size = vertices.size() * sizeof(vertices[0]);
        const auto indices_size = mesh.indices.size() * sizeof(mesh.indices[0]);

        mesh.indexBuffer = m_Device.CreateBuffer(Buffer::Desc{
            .size = indices_size,
            .stride = sizeof(mesh.indices[0]),
            .usage = Buffer::Usage::INDEX_BUFFER
        }).ToIndex();

        mesh.vertexBuffer = m_Device.CreateBuffer(Buffer::Desc{
            .size = vertices_size,
            .stride = sizeof(Vertex),
            .usage = Buffer::Usage::VERTEX_BUFFER
        }).ToIndex();

        auto& index_buffer  = m_Device.GetBuffer(BufferID(mesh.indexBuffer));
        m_StagingHeap.StageBuffer(backbuffer_data.mCmdList.Get(), index_buffer.GetResource(),  0, mesh.indices.data(), indices_size);
        
        auto& vertex_buffer = m_Device.GetBuffer(BufferID(mesh.vertexBuffer));
        m_StagingHeap.StageBuffer(backbuffer_data.mCmdList.Get(), vertex_buffer.GetResource(), 0, vertices.data(), vertices_size);
    }

    backbuffer_data.mCmdList->Close();

    const std::array cmd_lists = { (ID3D12CommandList*)backbuffer_data.mCmdList.Get() };
    m_Device.GetQueue()->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());
    
    backbuffer_data.mFenceValue++;
    gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));
    gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);

    const auto black_texture_file = TextureAsset::sConvert("assets/system/black4x4.png");
    const auto white_texture_file = TextureAsset::sConvert("assets/system/white4x4.png");
    const auto normal_texture_file = TextureAsset::sConvert("assets/system/normal4x4.png");
    m_DefaultBlackTexture = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(black_texture_file), black_texture_file, DXGI_FORMAT_BC3_UNORM);
    m_DefaultWhiteTexture = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(white_texture_file), white_texture_file, DXGI_FORMAT_BC3_UNORM);
    const auto default_normal_texture = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(normal_texture_file), normal_texture_file, DXGI_FORMAT_BC3_UNORM);

    for (const auto& [entity, material] : m_Scene.view<Material>().each()) {
        if (fs::exists(material.albedoFile))
            material.gpuAlbedoMap = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(material.albedoFile), material.albedoFile, DXGI_FORMAT_BC3_UNORM_SRGB).ToIndex();
        else
            material.gpuAlbedoMap = m_DefaultWhiteTexture.ToIndex();

        if (fs::exists(material.normalFile))
            material.gpuNormalMap = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(material.normalFile), material.normalFile, DXGI_FORMAT_BC3_UNORM).ToIndex();
        else
            material.gpuNormalMap = default_normal_texture.ToIndex();

        if (fs::exists(material.metalroughFile))
            material.gpuMetallicRoughnessMap = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(material.metalroughFile), material.metalroughFile, DXGI_FORMAT_BC3_UNORM).ToIndex();
        else
            material.gpuMetallicRoughnessMap = m_DefaultWhiteTexture.ToIndex();
    }

    if (!fs::exists("assets/system/shaders/DirectX/bin"))
        fs::create_directory("assets/system/shaders/DirectX/bin");

    fs::file_time_type timeOfMostRecentlyUpdatedIncludeFile;

    for (const auto& file : fs::directory_iterator("assets/system/shaders/DirectX/include")) {
        const auto updateTime = fs::last_write_time(file);

        if (updateTime > timeOfMostRecentlyUpdatedIncludeFile)
            timeOfMostRecentlyUpdatedIncludeFile = updateTime;
    }

    for (const auto& file : fs::directory_iterator("assets/system/shaders/DirectX")) {
        if (file.is_directory()) 
            continue;

        //Async::sQueueJob([=]() {
            auto outfile = file.path().parent_path() / "bin" / file.path().filename();
            outfile.replace_extension(".blob");

            const auto textFileWriteTime = file.last_write_time();

            bool success = false;

            if (file.path().extension() == ".hlsl") {
                auto outfile = file.path().parent_path() / "bin" / file.path().filename().replace_extension(".blob");

                const auto name = file.path().stem().string();
                auto type = name.substr(name.size() - 2, 2);
                std::transform(type.begin(), type.end(), type.begin(), tolower);

                ComPtr<IDxcLibrary> library;
                ComPtr<IDxcCompiler3> pCompiler;
                ComPtr<IDxcUtils> utils;
                ComPtr<IDxcIncludeHandler> include_handler;
                DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library.GetAddressOf()));
                DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(pCompiler.GetAddressOf()));
                DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()));
                utils->CreateDefaultIncludeHandler(include_handler.GetAddressOf());

                uint32_t code_page = CP_UTF8;
                ComPtr<IDxcBlobEncoding> blob;
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                std::wstring file_path = converter.from_bytes(file.path().string());
                gThrowIfFailed(library->CreateBlobFromFile(file_path.c_str(), &code_page, blob.GetAddressOf()));

                std::vector<LPCWSTR> arguments;
                //-E for the entry point (eg. PSMain)
                arguments.push_back(L"-E");
                arguments.push_back(L"main");

                //-T for the target profile (eg. ps_6_2)
                arguments.push_back(L"-T");

                if (type == "ps")
                    arguments.push_back(L"ps_6_6");
                else if (type == "vs")
                    arguments.push_back(L"vs_6_6");
                else if (type == "cs")
                    arguments.push_back(L"cs_6_6");

                arguments.push_back(L"-Zi");
                arguments.push_back(L"-Qembed_debug");
                arguments.push_back(L"-Od");

                arguments.push_back(L"-I");
                arguments.push_back(L"assets/system/shaders/DirectX");

                DxcBuffer sourceBuffer;
                sourceBuffer.Ptr = blob->GetBufferPointer();
                sourceBuffer.Size = blob->GetBufferSize();
                sourceBuffer.Encoding = 0;


                ComPtr<IDxcResult> result;
                gThrowIfFailed(pCompiler->Compile(&sourceBuffer, arguments.data(), uint32_t(arguments.size()), include_handler.Get(), IID_PPV_ARGS(result.GetAddressOf())));
                assert(result);

                ComPtr<IDxcBlobUtf8> errors;
                result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr);
                
                if (errors && errors->GetStringLength() > 0)
                    std::cout << static_cast<char*>(errors->GetBufferPointer()) << '\n';

                HRESULT hr;
                result->GetStatus(&hr);

                if(!SUCCEEDED(hr)) {
                    auto lock = Async::sLock();                     
                    std::cout << "Compilation " << COUT_RED("failed") << " for shader: " << file.path().string() << '\n';
                }
                else {
                    ComPtr<IDxcBlob> shader, pdb;
                    ComPtr<IDxcBlobUtf16> pDebugDataPath;
                    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader.GetAddressOf()), pDebugDataPath.GetAddressOf());
                    result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(pdb.GetAddressOf()), pDebugDataPath.GetAddressOf());

                    auto pdb_file = std::ofstream(file.path().parent_path() / file.path().filename().replace_extension(".pdb"));
                    pdb_file.write((char*)pdb->GetBufferPointer(), pdb->GetBufferSize());

                    auto lock = Async::sLock();

                    m_Shaders.insert(std::make_pair(file.path().stem().string(), shader));
                    
                    std::cout << "Compilation " << COUT_GREEN("Finished") << " for shader: " << file.path().string() << '\n';
                }
            }
       // });
    }

    // Wait for shader compilation to finish before continuing on with pipeline creation
    Async::sWait();

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {}; 
    gThrowIfFailed(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
    
    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1) {
        SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR, "Raekor Error", "GPU does not support D3D12_RAYTRACING_TIER_1_1.", m_Window);
        abort();
    }

    for (const auto& [entity, mesh] : m_Scene.view<Mesh>().each()) {
        const auto& gpu_index_buffer  = m_Device.GetBuffer(BufferID(mesh.indexBuffer));
        const auto& gpu_vertex_buffer = m_Device.GetBuffer(BufferID(mesh.vertexBuffer));

        D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
        geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        geom.Triangles.IndexBuffer = gpu_index_buffer->GetGPUVirtualAddress();
        geom.Triangles.IndexCount = mesh.indices.size();
        geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

        D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE vertex_info = {};
        vertex_info.StartAddress = gpu_vertex_buffer->GetGPUVirtualAddress();
        vertex_info.StrideInBytes = 44;

        geom.Triangles.VertexBuffer.StartAddress = gpu_vertex_buffer->GetGPUVirtualAddress();
        geom.Triangles.VertexBuffer.StrideInBytes = 44;
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
        auto& result_buffer  = m_Device.GetBuffer(component.buffer);
        
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
        desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();
        desc.DestAccelerationStructureData = result_buffer->GetGPUVirtualAddress();
        desc.Inputs = inputs;

        auto& backbuffer_data = GetBackbufferData();
        backbuffer_data.mCmdList->Reset(backbuffer_data.mCmdAllocator.Get(), nullptr);
        backbuffer_data.mCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
        backbuffer_data.mCmdList->Close();

        const std::array cmd_lists = { (ID3D12CommandList*)backbuffer_data.mCmdList.Get() };
        m_Device.GetQueue()->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

        backbuffer_data.mFenceValue++;
        gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));
        gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);

        m_Device.ReleaseBuffer(scratch_buffer_id);
    }

    uint32_t instance_id = 0;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;

    for (const auto& [entity, accel_struct, transform] : m_Scene.view<AccelerationStructure, Transform>().each()) {
        const auto& blas_buffer = m_Device.GetBuffer(accel_struct.buffer);

        D3D12_RAYTRACING_INSTANCE_DESC instance = {};
        instance.InstanceMask = 0xFF;
        instance.InstanceID = instance_id++;
        instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE | D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
        instance.AccelerationStructure = blas_buffer->GetGPUVirtualAddress();

        glm::mat4 transpose = glm::transpose(transform.worldTransform); // TODO: transform buffer
        memcpy(instance.Transform, glm::value_ptr(transpose), sizeof(instance.Transform));

        instances.push_back(instance);
    }

    auto instance_buffer = m_Device.GetBuffer(m_Device.CreateBuffer(Buffer::Desc{
        .size = instances.size() * sizeof(instances[0]),
        .usage = Buffer::UPLOAD,
    }));

    uint8_t* mapped_ptr;
    auto buffer_range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(instance_buffer->Map(0, &buffer_range, reinterpret_cast<void**>(&mapped_ptr)));
    memcpy(mapped_ptr, instances.data(), instances.size() * sizeof(instances[0]));
    instance_buffer->Unmap(0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.InstanceDescs = instance_buffer->GetGPUVirtualAddress();
    inputs.NumDescs = instances.size();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    const auto scratch_desc = Buffer::Desc{ .size = prebuild_info.ScratchDataSizeInBytes };
    const auto result_desc  = Buffer::Desc{ .size = prebuild_info.ResultDataMaxSizeInBytes, .usage = Buffer::Usage::ACCELERATION_STRUCTURE };

    auto scratch_buffer = m_Device.GetBuffer(m_Device.CreateBuffer(scratch_desc, L"TLAS_SCRATCH_BUFFER"));
    auto result_buffer = m_Device.GetBuffer(m_Device.CreateBuffer(result_desc, L"TLAS_FULL_SCENE"));

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srv_desc.RaytracingAccelerationStructure.Location = result_buffer->GetGPUVirtualAddress();

    // pResource must be NULL, since the resource location comes from a GPUVA in pDesc
    m_TLAS = m_Device.CreateShaderResourceView(nullptr, &srv_desc);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();
    desc.DestAccelerationStructureData = result_buffer->GetGPUVirtualAddress();
    desc.Inputs = inputs;

    backbuffer_data.mCmdList->Reset(backbuffer_data.mCmdAllocator.Get(), nullptr);
    backbuffer_data.mCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
    backbuffer_data.mCmdList->Close();

    m_Device.GetQueue()->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

    backbuffer_data.mFenceValue++;
    gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));
    gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);

    m_Blit.Init(m_Device, m_Shaders);
    m_GBuffer.Init(m_Viewport, m_Shaders, m_Device);
    m_Shadows.Init(m_Viewport, m_Shaders, m_Device);

    int width, height;
    unsigned char* pixels;
    ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    auto font_texture = m_Device.GetTexture(
        m_Device.CreateTexture(Texture::Desc{ 
            .format = DXGI_FORMAT_R8_UNORM, 
            .width = uint32_t(width), 
            .height = uint32_t(height),
            .usage = Texture::SHADER_SAMPLE
    }));

    const auto& descriptor_heap = m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ImGui_ImplDX12_Init(
        m_Device,
        sFrameCount,
        swapchainDesc.Format,
        descriptor_heap.GetHeap(),
        descriptor_heap.GetCPUDescriptorHandle(font_texture.GetView()),
        descriptor_heap.GetGPUDescriptorHandle(font_texture.GetView())
    );
}


DXApp::~DXApp() {
    for (const auto& backbuffer_data : m_BackbufferData) {
        gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));

        if (m_Fence->GetCompletedValue() < backbuffer_data.mFenceValue) {
            gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
            WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
        }
    }
}


void DXApp::OnUpdate(float dt) {
    m_Viewport.OnUpdate(dt);

    GUI::BeginFrame();
    ImGui_ImplDX12_NewFrame();
    
    ImGui::Begin("Settings", (bool*)true, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    auto lightView = m_Scene.view<DirectionalLight, Transform>();

    if (lightView.begin() != lightView.end()) {
        auto& sun_transform = lightView.get<Transform>(lightView.front());

        auto sun_rotation_degrees = glm::degrees(glm::eulerAngles(sun_transform.rotation));

        if (ImGui::DragFloat3("Sun Angle", glm::value_ptr(sun_rotation_degrees), 0.1f, -360.0f, 360.0f, "%.1f")) {
            sun_transform.rotation = glm::quat(glm::radians(sun_rotation_degrees));
            sun_transform.Compose();
        }
    }

    ImGui::End();

    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetRect(0, 0, float(m_Viewport.size.x), float(m_Viewport.size.y));

    if (lightView.begin() != lightView.end()) {
        auto& sun_transform = lightView.get<Transform>(lightView.front());

        bool manipulated = ImGuizmo::Manipulate(
            glm::value_ptr(m_Viewport.GetCamera().GetView()),
            glm::value_ptr(m_Viewport.GetCamera().GetProjection()),
            ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::WORLD,
            glm::value_ptr(sun_transform.localTransform)
        );

        if (manipulated)
            sun_transform.Decompose();
    }

    GUI::EndFrame();

    auto& backbuffer_data = GetBackbufferData();
    auto  completed_value = m_Fence->GetCompletedValue();

    if (completed_value < backbuffer_data.mFenceValue) {
        gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    gThrowIfFailed(backbuffer_data.mCmdAllocator->Reset());
    gThrowIfFailed(backbuffer_data.mCmdList->Reset(backbuffer_data.mCmdAllocator.Get(), NULL));

    m_GBuffer.Render(m_Viewport, m_Scene, m_Device, backbuffer_data.mCmdList.Get());

    const auto& rtv_heap = m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    std::array barriers = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_Device.GetTexture(m_GBuffer.m_RenderTarget).GetResource().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(rtv_heap.Get(backbuffer_data.mBackbufferRTV).Get(), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
    };

    backbuffer_data.mCmdList->ResourceBarrier(barriers.size(), barriers.data());

    m_Shadows.Render(m_Viewport, m_Device, m_Scene, m_TLAS, m_GBuffer.m_RenderTargetSRV, m_GBuffer.m_DepthStencilSRV, backbuffer_data.mCmdList.Get());

    m_Blit.Render(m_Device, backbuffer_data.mCmdList.Get(), m_Shadows.m_ResultTexture, backbuffer_data.mBackbufferRTV, m_GBuffer.m_RenderTargetSRV, ESampler::MIN_MAG_MIP_POINT_CLAMP);

    // TODO: ImGui's DX12 impl is sub-optimal and costs over half a millisecond, should integrate it ourselves
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), backbuffer_data.mCmdList.Get());

    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_Device.GetTexture(m_GBuffer.m_RenderTarget).GetResource().Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(rtv_heap.Get(backbuffer_data.mBackbufferRTV).Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);

    backbuffer_data.mCmdList->ResourceBarrier(barriers.size(), barriers.data());

    gThrowIfFailed(backbuffer_data.mCmdList->Close());

    const std::array commandLists = { static_cast<ID3D12CommandList*>(backbuffer_data.mCmdList.Get()) };
    m_Device.GetQueue()->ExecuteCommandLists(commandLists.size(), commandLists.data());
    
    gThrowIfFailed(m_Swapchain->Present(0, 0));

    backbuffer_data.mFenceValue++;
    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();
    gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), GetBackbufferData().mFenceValue));

}


void DXApp::OnEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    if (event.button.button == 2 || event.button.button == 3) {
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        else if (event.type == SDL_MOUSEBUTTONUP) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
    }

    auto& camera = m_Viewport.GetCamera();

    if (event.type == SDL_MOUSEMOTION) {
        if (SDL_GetRelativeMouseMode() && Input::sIsButtonPressed(3)) {
            auto formula = glm::radians(0.022f * camera.sensitivity * 2.0f);
            camera.Look(glm::vec2(event.motion.xrel * formula, event.motion.yrel * formula));
        }
        else if (SDL_GetRelativeMouseMode() && Input::sIsButtonPressed(2)) {
            camera.Move(glm::vec2(event.motion.xrel * 0.02f, event.motion.yrel * 0.02f));
        }
    }
    else if (event.type == SDL_MOUSEWHEEL) {
        camera.Zoom(float(event.wheel.y));
    }

    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            while (1) {
                SDL_Event ev;
                SDL_PollEvent(&ev);

                if (ev.window.event == SDL_WINDOWEVENT_RESTORED)
                    break;
            }
        }
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
            if (SDL_GetWindowID(m_Window) == event.window.windowID)
                m_Running = false;
        }
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            int w, h;
            SDL_GetWindowSize(m_Window, &w, &h);
            m_Viewport.Resize(glm::uvec2(w, h));

            const auto currentFenceValue = GetBackbufferData().mFenceValue;

            gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), currentFenceValue));

            if (m_Fence->GetCompletedValue() < GetBackbufferData().mFenceValue) {
                gThrowIfFailed(m_Fence->SetEventOnCompletion(GetBackbufferData().mFenceValue, m_FenceEvent));
                WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
            }

            for (auto& bb_data : m_BackbufferData)
                m_Device.ReleaseShaderResourceView(bb_data.mBackbufferRTV);

            gThrowIfFailed(m_Swapchain->ResizeBuffers(
                sFrameCount, m_Viewport.size.x, m_Viewport.size.y, 
                DXGI_FORMAT_B8G8R8A8_UNORM, 0
            ));

            /*for (uint32_t i = 0; i < sFrameCount; i++) {
                gThrowIfFailed(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&m_Device.m_RtvHeap[i])));

                gThrowIfFailed(m_Device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                    D3D12_HEAP_FLAG_NONE,
                    &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D24_UNORM_S8_UINT, m_Viewport.size.x, m_Viewport.size.y, 1u, 0, 1u, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
                    D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0u),
                    IID_PPV_ARGS(&m_Device.m_DsvHeap[i]))
                );
            }

            for (uint32_t i = 0; i < sFrameCount; i++) {
                m_Device.CreateRenderTargetView(i);
                m_Device.CreateDepthStencilView(i);
            }*/
        }
    }
}

ResourceID DXApp::QueueDirectStorageLoad(const TextureAsset::Ptr& asset, const fs::path& path, DXGI_FORMAT format) {
    if (!asset || !fs::exists(path))
        return m_DefaultWhiteTexture;

    ComPtr<IDStorageFactory> factory;
    gThrowIfFailed(DStorageGetFactory(IID_PPV_ARGS(&factory)));

    ComPtr<IDStorageFile> file;
    gThrowIfFailed(factory->OpenFile(path.wstring().c_str(), IID_PPV_ARGS(&file)));

    BY_HANDLE_FILE_INFORMATION info{};
    gThrowIfFailed(file->GetFileInformation(&info));
    uint32_t fileSize = info.nFileSizeLow;

    const auto data = asset->GetData();
    const auto header = asset->GetHeader();

    // I think DirectStorage doesnt do proper texture data alignment for its upload buffers as we get garbage past the 5th mip
    // TODO: only load in the main mip and kick off a FidelityFX downsample pass on a separate compute queue,
    auto mipmap_levels = std::min(header->dwMipMapCount, 5ul);

    auto texture = m_Device.GetTexture(m_Device.CreateTexture(Texture::Desc{ 
        .format = format, 
        .width = header->dwWidth, 
        .height = header->dwHeight, 
        .mipLevels = 1,
        .usage = Texture::SHADER_SAMPLE
    }, path.wstring().c_str()));

    DSTORAGE_REQUEST request = {};
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION;
    request.Source.File.Source = file.Get();
    request.Source.File.Offset = 128; // DDS header offset
    request.Source.File.Size = header->dwWidth * header->dwHeight;
    request.UncompressedSize = fileSize;
    request.Destination.Texture.Region = CD3DX12_BOX(0, 0, header->dwWidth, header->dwHeight);
    request.Destination.Texture.Resource = texture.GetResource().Get();
    request.Destination.Texture.SubresourceIndex = 0;
    request.Name = path.string().c_str();

    m_StorageQueue->EnqueueRequest(&request);

    ComPtr<ID3D12Fence> fence;
    gThrowIfFailed(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));

    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    constexpr uint64_t fenceValue = 1;
    gThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
    m_StorageQueue->EnqueueSignal(fence.Get(), fenceValue);

    // Tell DirectStorage to start executing all queued items.
    m_StorageQueue->Submit();

    // Wait for the submitted work to complete
    std::cout << "Waiting for the DirectStorage request to complete...\n";
    WaitForSingleObject(fenceEvent, INFINITE);
    CloseHandle(fenceEvent);

    return texture.GetView();
}


}