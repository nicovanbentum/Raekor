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

DXApp::DXApp() : Application(RendererFlags::NONE), m_Device(m_Window) {
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

        ComPtr<ID3D12Resource> rtv;
        gThrowIfFailed(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&rtv)));
        backbuffer_data.mBackbufferRTV = m_Device.m_RtvHeap.AddResource(rtv);
        m_Device.CreateRenderTargetView(i);
        
        gThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&backbuffer_data.mCmdAllocator)));
        gThrowIfFailed(m_Device->CreateCommandList(0x00, D3D12_COMMAND_LIST_TYPE_DIRECT, backbuffer_data.mCmdAllocator.Get(), nullptr, IID_PPV_ARGS(&backbuffer_data.mCmdList)));
        backbuffer_data.mCmdList->Close();
    }

    m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent)
        gThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    auto meshes = m_Scene.view<Mesh>();

    for (const auto entity : meshes) {
        auto& mesh = meshes.get<Mesh>(entity);
            const auto vertices = mesh.GetInterleavedVertices();

            {
                ComPtr<ID3D12Resource> vertexBuffer;
                const auto size = vertices.size() * sizeof(vertices[0]);

                gThrowIfFailed(m_Device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                    D3D12_HEAP_FLAG_NONE,
                    &CD3DX12_RESOURCE_DESC::Buffer(size),
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&vertexBuffer))
                );

                uint8_t* mappedPtr;
                gThrowIfFailed(vertexBuffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&mappedPtr)));
        
                memcpy(mappedPtr, vertices.data(), size);
        
                vertexBuffer->Unmap(0, nullptr);

                mesh.vertexBuffer = m_Device.m_CbvSrvUavHeap.AddResource(vertexBuffer);
            }

            {
                ComPtr<ID3D12Resource> indexBuffer;
                const auto size = mesh.indices.size() * sizeof(mesh.indices[0]);

                gThrowIfFailed(m_Device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                    D3D12_HEAP_FLAG_NONE,
                    &CD3DX12_RESOURCE_DESC::Buffer(size),
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&indexBuffer))
                );

                uint8_t* mappedPtr;
                gThrowIfFailed(indexBuffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&mappedPtr)));

                memcpy(mappedPtr, mesh.indices.data(), size);

                indexBuffer->Unmap(0, nullptr);

                mesh.indexBuffer = m_Device.m_CbvSrvUavHeap.AddResource(indexBuffer);
            }
    }

    const auto black_texture_file = TextureAsset::sConvert("assets/system/black4x4.png");
    const auto white_texture_file = TextureAsset::sConvert("assets/system/white4x4.png");
    const auto normal_texture_file = TextureAsset::sConvert("assets/system/normal4x4.png");
    m_DefaultBlackTexture = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(black_texture_file), black_texture_file, DXGI_FORMAT_BC3_UNORM);
    m_DefaultWhiteTexture = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(white_texture_file), white_texture_file, DXGI_FORMAT_BC3_UNORM);
    const auto default_normal_texture = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(normal_texture_file), normal_texture_file, DXGI_FORMAT_BC3_UNORM);

    for (const auto& [entity, material] : m_Scene.view<Material>().each()) {
        if (fs::exists(material.albedoFile))
            material.gpuAlbedoMap = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(material.albedoFile), material.albedoFile, DXGI_FORMAT_BC3_UNORM_SRGB);
        else
            material.gpuAlbedoMap = m_DefaultWhiteTexture;

        if (fs::exists(material.normalFile))
            material.gpuNormalMap = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(material.normalFile), material.normalFile, DXGI_FORMAT_BC3_UNORM);
        else
            material.gpuNormalMap = default_normal_texture;

        if (fs::exists(material.metalroughFile))
            material.gpuMetallicRoughnessMap = QueueDirectStorageLoad(m_Assets.Get<TextureAsset>(material.metalroughFile), material.metalroughFile, DXGI_FORMAT_BC3_UNORM);
        else
            material.gpuMetallicRoughnessMap = m_DefaultWhiteTexture;
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
        D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
        geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        geom.Triangles.IndexBuffer = m_Device.m_CbvSrvUavHeap[mesh.indexBuffer]->GetGPUVirtualAddress();
        geom.Triangles.IndexCount = mesh.indices.size();
        geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

        D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE vertex_info = {};
        vertex_info.StartAddress = m_Device.m_CbvSrvUavHeap[mesh.vertexBuffer]->GetGPUVirtualAddress();
        vertex_info.StrideInBytes = 44;

        geom.Triangles.VertexBuffer.StartAddress = m_Device.m_CbvSrvUavHeap[mesh.vertexBuffer]->GetGPUVirtualAddress();
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

        ComPtr<ID3D12Resource> scratch_buffer;
        gThrowIfFailed(m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(prebuild_info.ScratchDataSizeInBytes),
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(scratch_buffer.GetAddressOf()))
        );

        ComPtr<ID3D12Resource> accel_struct;
        gThrowIfFailed(m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(prebuild_info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            nullptr,
            IID_PPV_ARGS(accel_struct.GetAddressOf()))
        );

        auto& component = m_Scene.emplace<AccelerationStructure>(entity);
        component.handle = m_Device.m_CbvSrvUavHeap.AddResource(accel_struct);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
        desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();
        desc.DestAccelerationStructureData = accel_struct->GetGPUVirtualAddress();
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
    }

    uint32_t instance_id = 0;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;

    for (const auto& [entity, accel_struct, transform] : m_Scene.view<AccelerationStructure, Transform>().each()) {
        D3D12_RAYTRACING_INSTANCE_DESC instance = {};
        instance.InstanceMask = 0xFF;
        instance.InstanceID = instance_id++;
        instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE | D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
        instance.AccelerationStructure = m_Device.m_CbvSrvUavHeap[accel_struct.handle]->GetGPUVirtualAddress();

        glm::mat4 transpose = glm::transpose(glm::mat4(1.0f)); // TODO: transform buffer
        memcpy(instance.Transform, glm::value_ptr(transpose), sizeof(instance.Transform));

        instances.push_back(instance);
    }

    ComPtr<ID3D12Resource> instance_buffer;
    gThrowIfFailed(m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(instances.size() * sizeof(instances[0])),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(instance_buffer.GetAddressOf()))
    );

    uint8_t* mappedPtr;
    gThrowIfFailed(instance_buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&mappedPtr)));

    memcpy(mappedPtr, instances.data(), instances.size() * sizeof(instances[0]));

    instance_buffer->Unmap(0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.InstanceDescs = instance_buffer->GetGPUVirtualAddress();
    inputs.NumDescs = instances.size();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    ComPtr<ID3D12Resource> scratch_buffer;
    gThrowIfFailed(m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(prebuild_info.ScratchDataSizeInBytes),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(scratch_buffer.GetAddressOf()))
    );

    ComPtr<ID3D12Resource> accel_struct;
    gThrowIfFailed(m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(prebuild_info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nullptr,
        IID_PPV_ARGS(accel_struct.GetAddressOf()))
    );

    accel_struct->SetName(L"TLAS_FULL_SCENE");
    m_TLAS = m_Device.m_CbvSrvUavHeap.AddResource(accel_struct);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srv_desc.RaytracingAccelerationStructure.Location = accel_struct->GetGPUVirtualAddress();

    m_Device->CreateShaderResourceView(nullptr, &srv_desc, m_Device.m_CbvSrvUavHeap.GetCPUDescriptorHandle(m_TLAS));

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();
    desc.DestAccelerationStructureData = accel_struct->GetGPUVirtualAddress();
    desc.Inputs = inputs;


    auto& backbuffer_data = GetBackbufferData();
    backbuffer_data.mCmdList->Reset(backbuffer_data.mCmdAllocator.Get(), nullptr);
    backbuffer_data.mCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
    backbuffer_data.mCmdList->Close();

    const std::array cmd_lists = { static_cast<ID3D12CommandList*>(backbuffer_data.mCmdList.Get()) };
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

    ComPtr<ID3D12Resource> font_texture;

    auto font_texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8_UNORM, width, height);

    gThrowIfFailed(m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &font_texture_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(font_texture.GetAddressOf()))
    );


    UINT rows; 
    UINT64 row_size, total_size;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT font_texture_footprint;
    m_Device->GetCopyableFootprints(&font_texture_desc, 0, 1, 0, &font_texture_footprint, &rows, &row_size, &total_size);

    ComPtr<ID3D12Resource> font_upload_buffer;
    
    gThrowIfFailed(m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(total_size),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        nullptr,
        IID_PPV_ARGS(font_upload_buffer.GetAddressOf()))
    );

    {
        void* mappedPtr;
        gThrowIfFailed(font_upload_buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&mappedPtr)));
        memcpy(mappedPtr, pixels, gAlignUp(font_texture_footprint.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
    }

    const auto dest = CD3DX12_TEXTURE_COPY_LOCATION(font_texture.Get(), 0);
    const auto source = CD3DX12_TEXTURE_COPY_LOCATION(font_upload_buffer.Get(), font_texture_footprint);

    backbuffer_data.mCmdList->Reset(backbuffer_data.mCmdAllocator.Get(), nullptr);
    backbuffer_data.mCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);
    backbuffer_data.mCmdList->Close();

    m_Device.GetQueue()->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

    backbuffer_data.mFenceValue++;
    gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));
    gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);


    const auto font_texture_id = m_Device.m_CbvSrvUavHeap.AddResource(font_texture);
    m_Device.CreateShaderResourceView(font_texture_id);

    ImGui_ImplDX12_Init(
        m_Device,
        sFrameCount,
        swapchainDesc.Format,
        m_Device.m_CbvSrvUavHeap.GetHeap(),
        m_Device.m_CbvSrvUavHeap.GetCPUDescriptorHandle(font_texture_id),
        m_Device.m_CbvSrvUavHeap.GetGPUDescriptorHandle(font_texture_id)
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

        if (manipulated) {
            sun_transform.Decompose();
        }
    }

    GUI::EndFrame();

    auto& backbuffer_data = GetBackbufferData();
    auto  completed_value = m_Fence->GetCompletedValue();

    if (completed_value < backbuffer_data.mFenceValue) {
        gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    backbuffer_data.mCmdList->Reset(backbuffer_data.mCmdAllocator.Get(), nullptr);

    m_GBuffer.Render(m_Viewport, m_Scene, m_Device, backbuffer_data.mCmdList.Get());

    std::array barriers = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_Device.m_RtvHeap[m_GBuffer.m_RenderTarget].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(m_Device.m_RtvHeap[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
    };

    backbuffer_data.mCmdList->ResourceBarrier(barriers.size(), barriers.data());

    m_Shadows.Render(m_Viewport, m_Device, m_Scene, m_TLAS, m_GBuffer.m_RenderTargetSRV, m_GBuffer.m_DepthSRV, backbuffer_data.mCmdList.Get());

    m_Blit.Render(m_Device, backbuffer_data.mCmdList.Get(), m_Shadows.m_ResultTexture, m_FrameIndex, m_GBuffer.m_RenderTargetSRV, ESampler::MIN_MAG_MIP_POINT_CLAMP);

    // TODO: ImGui's DX12 impl is sub-optimal and costs over half a millisecond, should integrate it ourselves
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), backbuffer_data.mCmdList.Get());

    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_Device.m_RtvHeap[m_GBuffer.m_RenderTarget].Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_Device.m_RtvHeap[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);

    backbuffer_data.mCmdList->ResourceBarrier(barriers.size(), barriers.data());

    backbuffer_data.mCmdList->Close();

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

            for (uint32_t i = 0; i < sFrameCount; i++) {
                m_Device.m_RtvHeap.RemoveResource(i);
                m_Device.m_RtvHeap[i].Reset();
            }

            gThrowIfFailed(m_Swapchain->ResizeBuffers(
                sFrameCount, m_Viewport.size.x, m_Viewport.size.y, 
                DXGI_FORMAT_B8G8R8A8_UNORM, 0
            ));

            for (uint32_t i = 0; i < sFrameCount; i++) {
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
            }
        }
    }
}

uint32_t DXApp::QueueDirectStorageLoad(const TextureAsset::Ptr& asset, const fs::path& path, DXGI_FORMAT format) {
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

    ComPtr<ID3D12Resource> texture_resource;
    const auto texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, header->dwWidth, header->dwHeight, 1u, mipmap_levels, 1u, 0, D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT_UNKNOWN);

    gThrowIfFailed(m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texture_desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&texture_resource))
    );

    UINT64 total_uncompressed_size = 0;

    for (uint32_t mip = 0; mip< mipmap_levels; mip++) {
        UINT64 uncompressed_size;
        m_Device->GetCopyableFootprints(&texture_desc, mip, 1, 0, nullptr, nullptr, nullptr, &uncompressed_size);
        total_uncompressed_size += uncompressed_size;
    }

    DSTORAGE_REQUEST request = {};
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MULTIPLE_SUBRESOURCES;
    request.Source.File.Source = file.Get();
    request.Source.File.Offset = sizeof(DDS_MAGIC) + sizeof(DDS_HEADER);
    request.Source.File.Size = asset->GetDataSize();
    request.Destination.MultipleSubresources.Resource = texture_resource.Get();
    request.Destination.MultipleSubresources.FirstSubresource = 0;
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

    texture_resource->SetName(path.wstring().c_str());

    auto index = m_Device.m_CbvSrvUavHeap.AddResource(texture_resource);
    m_Device.CreateShaderResourceView(index);

    return index;
}


}