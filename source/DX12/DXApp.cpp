#include "pch.h"
#include "DXApp.h"
#include "DXUtil.h"
#include "Raekor/OS.h"
#include "Raekor/util.h"
#include "Raekor/timer.h"
#include "Raekor/input.h"
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
    ImGui_ImplSDL2_InitForVulkan(m_Window);

    ImGui::GetIO().IniFilename = "";

    while (!fs::exists(m_Settings.defaultScene))
        m_Settings.defaultScene = fs::relative(OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0")).string();

    SDL_SetWindowTitle(m_Window, std::string(m_Settings.defaultScene + " - Raekor Renderer").c_str());
    m_Scene.OpenFromFile(m_Assets, m_Settings.defaultScene);

    assert(!m_Scene.empty() && "Scene cannot be empty when starting up DX12 renderer!!");

    DSTORAGE_QUEUE_DESC queueDesc{};
    queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
    queueDesc.Priority = DSTORAGE_PRIORITY_NORMAL;
    queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queueDesc.Device = m_Device.GetRaw();

    ComPtr<IDStorageFactory> storage_factory;
    gThrowIfFailed(DStorageGetFactory(IID_PPV_ARGS(&storage_factory)));

    gThrowIfFailed(storage_factory->CreateQueue(&queueDesc, IID_PPV_ARGS(&m_StorageQueue)));

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

    m_FenceValues.resize(sFrameCount);
    m_CommandLists.resize(sFrameCount);
    m_CommnadAllocators.resize(sFrameCount);

    for (uint32_t i = 0; i < sFrameCount; i++) {
        // create a depth stencil texture per frame
        ComPtr<ID3D12Resource> dsv;

        gThrowIfFailed(m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D24_UNORM_S8_UINT, m_Viewport.size.x, m_Viewport.size.y, 1u, 0, 1u, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0u),
            IID_PPV_ARGS(&dsv))
        );

        m_Device.m_DsvHeap.Add(dsv);

        ComPtr<ID3D12Resource> rtv;
        gThrowIfFailed(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&rtv)));
        m_Device.m_RtvHeap.Add(rtv);
        
        // initialize the render target at the heap pointer location
        m_Device->CreateRenderTargetView(m_Device.m_RtvHeap[i].Get(), nullptr, m_Device.m_RtvHeap.GetCPUDescriptorHandle(i));
        m_Device->CreateDepthStencilView(m_Device.m_DsvHeap[i].Get(), nullptr, m_Device.m_DsvHeap.GetCPUDescriptorHandle(i));

        gThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommnadAllocators[i])));
        gThrowIfFailed(m_Device->CreateCommandList(0x00, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommnadAllocators[i].Get(), nullptr, IID_PPV_ARGS(&m_CommandLists[i])));
        m_CommandLists[i]->Close();
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

                mesh.vertexBuffer = m_Device.m_CbvSrvUavHeap.Add(vertexBuffer);
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

                mesh.indexBuffer = m_Device.m_CbvSrvUavHeap.Add(indexBuffer);
            }
    }

    for (const auto& [entity, material] : m_Scene.view<Material>().each()) {
        if (!material.albedoFile.empty()) {
            material.gpuAlbedoMap = RegisterTexture(m_Assets.Get<TextureAsset>(material.albedoFile), material.albedoFile, DXGI_FORMAT_BC3_UNORM_SRGB);
            material.gpuNormalMap = RegisterTexture(m_Assets.Get<TextureAsset>(material.normalFile), material.normalFile, DXGI_FORMAT_BC3_UNORM);
            material.gpuMetallicRoughnessMap = RegisterTexture(m_Assets.Get<TextureAsset>(material.metalroughFile), material.metalroughFile, DXGI_FORMAT_BC3_UNORM);
        }
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

                arguments.push_back(L"-Qstrip_reflect");

                arguments.push_back(L"-I");
                arguments.push_back(L"assets/system/shaders/DirectX");

                arguments.push_back(L"-H");

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
                    ComPtr<IDxcBlob> shader;
                    ComPtr<IDxcBlobUtf16> pDebugDataPath;
                    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader.GetAddressOf()), pDebugDataPath.GetAddressOf());

                    auto lock = Async::sLock();

                    m_Shaders.insert(std::make_pair(file.path().filename().string(), shader));
                    
                    std::cout << "Compilation " << COUT_GREEN("Finished") << " for shader: " << file.path().string() << '\n';
                }
            }
       // });
    }

    constexpr std::array vertexLayout = {
        D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_ROOT_PARAMETER1 constants_param = {};
    constants_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    constants_param.Constants.ShaderRegister = 0;
    constants_param.Constants.RegisterSpace = 0;
    constants_param.Constants.Num32BitValues = 32;

    D3D12_STATIC_SAMPLER_DESC static_sampler_desc = {};
    static_sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    static_sampler_desc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    static_sampler_desc.MaxAnisotropy = 0.0f;
    static_sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
    static_sampler_desc.MinLOD = 0.0f;
    static_sampler_desc.MipLODBias = 0.0f;
    static_sampler_desc.RegisterSpace = 0;
    static_sampler_desc.ShaderRegister = 0;
    static_sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC vrsd;
    vrsd.Init_1_1(1, &constants_param, 1, &static_sampler_desc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    auto serialize_vrs_hr = D3DX12SerializeVersionedRootSignature(&vrsd, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);

    if (error)
        OutputDebugStringA((char*)error->GetBufferPointer());

    gThrowIfFailed(serialize_vrs_hr);
    gThrowIfFailed(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));

    // Wait for shader compilation to finish before continuing on with pipeline creation
    Async::sWait();

    const auto& vertexShader = m_Shaders.at("gbufferVS.hlsl");
    const auto& pixelShader = m_Shaders.at("gbufferPS.hlsl");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psd = {};
    psd.VS = CD3DX12_SHADER_BYTECODE(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
    psd.PS = CD3DX12_SHADER_BYTECODE(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
    psd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psd.InputLayout.NumElements = vertexLayout.size();
    psd.InputLayout.pInputElementDescs = vertexLayout.data();
    psd.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psd.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psd.RasterizerState.FrontCounterClockwise = TRUE;
    psd.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psd.SampleMask = UINT_MAX;
    psd.SampleDesc.Count = 1;
    psd.NumRenderTargets = 1;
    psd.RTVFormats[0] = m_Device.m_RtvHeap[m_FrameIndex]->GetDesc().Format;
    psd.DSVFormat = m_Device.m_DsvHeap[m_FrameIndex]->GetDesc().Format;
    psd.pRootSignature = m_RootSignature.Get();

    gThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&m_Pipeline)));

    m_Viewport.GetCamera().Zoom(-50.0f);
    m_Viewport.GetCamera().Move(glm::vec2(0.0f, 10.0f));
}


DXApp::~DXApp() {
    for (const auto& fenceValue : m_FenceValues) {
        gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), fenceValue));

        if (m_Fence->GetCompletedValue() < fenceValue) {
            gThrowIfFailed(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent));
            WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
        }
    }
}


void DXApp::OnUpdate(float dt) {
    SDL_SetWindowTitle(m_Window, std::string("Raekor - " + std::to_string(Timer::sToMilliseconds(dt)) + " ms").c_str());

    m_Viewport.OnUpdate(dt);

    const UINT64 currentFenceValue = m_FenceValues[m_FrameIndex];

    gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), currentFenceValue));

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex]) {
        gThrowIfFailed(m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    const auto& cmd_list = m_CommandLists[m_FrameIndex];

    cmd_list->Reset(m_CommnadAllocators[m_FrameIndex].Get(), nullptr);
    
    const auto clearColour = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

    cmd_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Device.m_RtvHeap[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
    const auto depth_target = m_Device.m_DsvHeap.GetCPUDescriptorHandle(m_FrameIndex);
    const auto render_target = m_Device.m_RtvHeap.GetCPUDescriptorHandle(m_FrameIndex);

    // setup pass state
    cmd_list->SetPipelineState(m_Pipeline.Get());
    cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd_list->RSSetViewports(1, &CD3DX12_VIEWPORT(m_Device.m_RtvHeap[m_FrameIndex].Get()));
    cmd_list->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, m_Viewport.size.x, m_Viewport.size.y));
    cmd_list->OMSetRenderTargets(1, &render_target, FALSE, &depth_target);
    cmd_list->SetGraphicsRootSignature(m_RootSignature.Get());

    const std::array heaps = { m_Device.m_CbvSrvUavHeap.GetHeap().Get() };
    cmd_list->SetDescriptorHeaps(heaps.size(), heaps.data());

    // clear render targets
    const auto clear_rect = CD3DX12_RECT(0, 0, m_Viewport.size.x, m_Viewport.size.y);
    cmd_list->ClearRenderTargetView(render_target, glm::value_ptr(clearColour), 1, &clear_rect);
    cmd_list->ClearDepthStencilView(depth_target, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &clear_rect);

    m_RootConstants.mViewProj = m_Viewport.GetCamera().GetProjection() * m_Viewport.GetCamera().GetView();
    cmd_list->SetGraphicsRoot32BitConstants(0, sizeof(m_RootConstants) / sizeof(DWORD), &m_RootConstants, 0);

    for (const auto& [entity, mesh] : m_Scene.view<Mesh>().each()) {
        const auto& indexBuffer = m_Device.m_CbvSrvUavHeap[mesh.indexBuffer];
        const auto& vertexBuffer = m_Device.m_CbvSrvUavHeap[mesh.vertexBuffer];

        D3D12_INDEX_BUFFER_VIEW indexView = {};
        indexView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
        indexView.Format = DXGI_FORMAT_R32_UINT;
        indexView.SizeInBytes = mesh.indices.size() * sizeof(mesh.indices[0]);

        D3D12_VERTEX_BUFFER_VIEW vertexView = {};
        vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vertexView.SizeInBytes = vertexBuffer->GetDesc().Width;
        vertexView.StrideInBytes = 44; // TODO: derive from input layout since its all tightly packed

        if (auto material = m_Scene.try_get<Material>(mesh.material)) {
            m_RootConstants.albedo = material->albedo;
            m_RootConstants.textures.x = material->gpuAlbedoMap;
            m_RootConstants.textures.y = material->gpuNormalMap;
            m_RootConstants.textures.z = material->gpuMetallicRoughnessMap;
            cmd_list->SetGraphicsRoot32BitConstants(0, sizeof(m_RootConstants) / sizeof(DWORD), &m_RootConstants, 0);
        }

        cmd_list->IASetIndexBuffer(&indexView);
        cmd_list->IASetVertexBuffers(0, 1, &vertexView);
        
        cmd_list->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
    }

    cmd_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Device.m_RtvHeap[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON));

    cmd_list->Close();

    const std::array commandLists = { static_cast<ID3D12CommandList*>(cmd_list.Get()) };
    m_Device.GetQueue()->ExecuteCommandLists(commandLists.size(), commandLists.data());
    
    gThrowIfFailed(m_Swapchain->Present(0, 0));

    m_FenceValues[m_FrameIndex] = currentFenceValue + 1;
}


void DXApp::OnEvent(const SDL_Event& event) {
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

            const UINT64 currentFenceValue = m_FenceValues[m_FrameIndex];

            gThrowIfFailed(m_Device.GetQueue()->Signal(m_Fence.Get(), currentFenceValue));

            if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex]) {
                gThrowIfFailed(m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent));
                WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
            }

            for (uint32_t i = 0; i < sFrameCount; i++) {
                m_Device.m_DsvHeap.Remove(i);
                m_Device.m_RtvHeap.Remove(i);
                m_Device.m_RtvHeap[i].Reset();
                m_Device.m_DsvHeap[i].Reset();
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
                m_Device->CreateRenderTargetView(m_Device.m_RtvHeap[i].Get(), nullptr, m_Device.m_RtvHeap.GetCPUDescriptorHandle(i));
                m_Device->CreateDepthStencilView(m_Device.m_DsvHeap[i].Get(), nullptr, m_Device.m_DsvHeap.GetCPUDescriptorHandle(i));
            }
        }
    }
}

uint32_t DXApp::RegisterTexture(const TextureAsset::Ptr& asset, const fs::path& path, DXGI_FORMAT format) {
    if (!asset || !fs::exists(path))
        return 0;

    ComPtr<IDStorageFactory> factory;
    gThrowIfFailed(DStorageGetFactory(IID_PPV_ARGS(&factory)));

    ComPtr<IDStorageFile> file;
    gThrowIfFailed(factory->OpenFile(path.wstring().c_str(), IID_PPV_ARGS(&file)));

    BY_HANDLE_FILE_INFORMATION info{};
    gThrowIfFailed(file->GetFileInformation(&info));
    uint32_t fileSize = info.nFileSizeLow;

    const auto data = asset->GetData();
    const auto header = asset->GetHeader();

    ComPtr<ID3D12Resource> texture_resource;

    gThrowIfFailed(m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(format, header->dwWidth, header->dwHeight, 1u, header->dwMipMapCount, 1u, 0, D3D12_RESOURCE_FLAG_NONE),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&texture_resource))
    );

    DSTORAGE_REQUEST request = {};
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION;
    request.Source.File.Source = file.Get();
    request.Source.File.Offset = 128; // DDS header offset
    request.Source.File.Size = header->dwWidth * header->dwHeight;
    request.UncompressedSize = fileSize;
    request.Destination.Texture.Region = CD3DX12_BOX(0, 0, header->dwWidth, header->dwHeight);
    request.Destination.Texture.Resource = texture_resource.Get();
    request.Destination.Texture.SubresourceIndex = 0;
    request.Name = path.string().c_str();

    m_StorageQueue->EnqueueRequest(&request);

    ComPtr<ID3D12Fence> fence;
    gThrowIfFailed(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));

    HANDLE fenceEvent(CreateEvent(nullptr, FALSE, FALSE, nullptr));
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

    auto index = m_Device.m_CbvSrvUavHeap.Add(texture_resource);
    m_Device->CreateShaderResourceView(m_Device.m_CbvSrvUavHeap[index].Get(), nullptr, m_Device.m_CbvSrvUavHeap.GetCPUDescriptorHandle(index));

    return index;
}


}