#include "pch.h"
#include "DXApp.h"
#include "Raekor/OS.h"
#include "Raekor/util.h"
#include "Raekor/timer.h"
#include <locale>
#include <codecvt>

namespace Raekor::DX {

enum ShaderStage {
    VERTEX, PIXEL, COMPUTE, COUNT
};

constexpr std::array shaderEntryName = {
    L"VSMain", L"PSMain", L"CSMain"
};

DXApp::DXApp() : Application(RendererFlags::NONE) {
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForVulkan(m_Window);

    ImGui::GetIO().IniFilename = "";

    while (!fs::exists(m_Settings.defaultScene)) {
        m_Settings.defaultScene = fs::relative(OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0")).string();
    }

    SDL_SetWindowTitle(m_Window, std::string(m_Settings.defaultScene + " - Raekor Renderer").c_str());
    m_Scene.OpenFromFile(m_Assets, m_Settings.defaultScene);

    assert(!m_Scene.empty() && "Scene cannot be empty when starting up DX12 renderer!!");

#ifndef NDEBUG
    ComPtr<ID3D12Debug> mDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&mDebug)))) {
        mDebug->EnableDebugLayer();
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> mAdapter;
    ThrowIfFailed(factory->EnumAdapters1(0, &mAdapter));

    ThrowIfFailed(D3D12CreateDevice(mAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_Device)));

    const D3D12_COMMAND_QUEUE_DESC qd = {};
    ThrowIfFailed(m_Device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_Queue)));

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(m_Window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.BufferCount = FrameCount;
    swapchainDesc.Width = m_Viewport.size.x;
    swapchainDesc.Height = m_Viewport.size.y;
    swapchainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapchain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(m_Queue.Get(), wmInfo.info.win.window, &swapchainDesc, nullptr, nullptr, &swapchain));

    ThrowIfFailed(swapchain.As(&m_Swapchain));

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = FrameCount;

        ThrowIfFailed(m_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_RtvHeap)));
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = std::numeric_limits<uint16_t>::max();

        ThrowIfFailed(m_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_SrvCbvUavHeap)));
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = FrameCount;

        ThrowIfFailed(m_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DsvHeap)));
    }

    auto rtvHeapPtr = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
    auto dsvHeapPtr = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_DsvHeap->GetCPUDescriptorHandleForHeapStart());


    m_FenceValues.resize(FrameCount);
    m_CommandLists.resize(FrameCount);
    m_RenderTargets.resize(FrameCount);
    m_CommnadAllocators.resize(FrameCount);
    m_RenderTargetDepths.resize(FrameCount);

    for (uint32_t i = 0; i < FrameCount; i++) {
        // create a depth stencil texture per frame
        ThrowIfFailed(m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D24_UNORM_S8_UINT, m_Viewport.size.x, m_Viewport.size.y, 1u, 0, 1u, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            nullptr,
            IID_PPV_ARGS(&m_RenderTargetDepths[i]))
        );

        ThrowIfFailed(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i])));
        
        // initialize the render target at the heap pointer location
        m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, rtvHeapPtr);
        m_Device->CreateDepthStencilView(m_RenderTargetDepths[i].Get(), nullptr, dsvHeapPtr);
        
        // increment the heap pointers
        rtvHeapPtr.Offset(m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
        dsvHeapPtr.Offset(m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
        
        ThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommnadAllocators[i])));
        ThrowIfFailed(m_Device->CreateCommandList(0x00, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommnadAllocators[i].Get(), nullptr, IID_PPV_ARGS(&m_CommandLists[i])));
        m_CommandLists[i]->Close();
    }

    m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));

    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    auto meshes = m_Scene.view<Mesh>();

    for (const auto entity : meshes) {
        auto& mesh = meshes.get<Mesh>(entity);
            const auto vertices = mesh.GetInterleavedVertices();

            {
                ComPtr<ID3D12Resource> vertexBuffer;
                const auto size = vertices.size() * sizeof(vertices[0]);

                ThrowIfFailed(m_Device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                    D3D12_HEAP_FLAG_NONE,
                    &CD3DX12_RESOURCE_DESC::Buffer(size),
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&vertexBuffer))
                );

                uint8_t* mappedPtr;
                ThrowIfFailed(vertexBuffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&mappedPtr)));
        
                memcpy(mappedPtr, vertices.data(), size);
        
                vertexBuffer->Unmap(0, nullptr);

                mesh.vertexBuffer = m_Buffers.push_back_or_insert(vertexBuffer);
            }

            {
                ComPtr<ID3D12Resource> indexBuffer;
                const auto size = mesh.indices.size() * sizeof(mesh.indices[0]);

                ThrowIfFailed(m_Device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                    D3D12_HEAP_FLAG_NONE,
                    &CD3DX12_RESOURCE_DESC::Buffer(size),
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&indexBuffer))
                );

                uint8_t* mappedPtr;
                ThrowIfFailed(indexBuffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&mappedPtr)));

                memcpy(mappedPtr, mesh.indices.data(), size);

                indexBuffer->Unmap(0, nullptr);

                mesh.indexBuffer = m_Buffers.push_back_or_insert(indexBuffer);
            }
    }

    if (!fs::exists("assets/system/shaders/DirectX/bin")) {
        fs::create_directory("assets/system/shaders/DirectX/bin");
    }

    fs::file_time_type timeOfMostRecentlyUpdatedIncludeFile;

    for (const auto& file : fs::directory_iterator("assets/system/shaders/DirectX/include")) {
        const auto updateTime = fs::last_write_time(file);

        if (updateTime > timeOfMostRecentlyUpdatedIncludeFile) {
            timeOfMostRecentlyUpdatedIncludeFile = updateTime;
        }
    }

    for (const auto& file : fs::directory_iterator("assets/system/shaders/DirectX")) {
        if (file.is_directory()) continue;

        Async::sDispatch([=]() {
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
                DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library.GetAddressOf()));
                DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(pCompiler.GetAddressOf()));

                uint32_t code_page = CP_UTF8;
                ComPtr<IDxcBlobEncoding> blob;
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                std::wstring file_path = converter.from_bytes(file.path().string());
                ThrowIfFailed(library->CreateBlobFromFile(file_path.c_str(), &code_page, blob.GetAddressOf()));

                std::vector<LPCWSTR> arguments;
                //-E for the entry point (eg. PSMain)
                arguments.push_back(L"-E");
                arguments.push_back(L"main");

                //-T for the target profile (eg. ps_6_2)
                arguments.push_back(L"-T");

                if (type == "ps") {
                    arguments.push_back(L"ps_6_6");
                }
                else if (type == "vs") {
                    arguments.push_back(L"vs_6_6");
                }

                arguments.push_back(L"-Qstrip_debug");
                arguments.push_back(L"-Qstrip_reflect");

                DxcBuffer sourceBuffer;
                sourceBuffer.Ptr = blob->GetBufferPointer();
                sourceBuffer.Size = blob->GetBufferSize();
                sourceBuffer.Encoding = 0;

                ComPtr<IDxcResult> result;
                ThrowIfFailed(pCompiler->Compile(&sourceBuffer, arguments.data(), uint32_t(arguments.size()), nullptr, IID_PPV_ARGS(result.GetAddressOf()))); 
                assert(result);

                HRESULT hr;
                result->GetStatus(&hr);

                if(!SUCCEEDED(hr)) {
                    ComPtr<IDxcBlobUtf8> errors;
                    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr);
                        
                    if (errors && errors->GetStringLength() > 0) {
                        std::cout << static_cast<char*>(errors->GetBufferPointer()) << '\n';
                    }

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
        });
    }

    constexpr std::array vertexLayout = {
        D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC vrsd;
    vrsd.Init_1_1(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&vrsd, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
    ThrowIfFailed(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));

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
    psd.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psd.SampleMask = 0xFFFF;
    psd.SampleDesc.Count = 1;
    psd.NumRenderTargets = 1;
    psd.RTVFormats[0] = m_RenderTargets[m_FrameIndex]->GetDesc().Format;
    psd.DSVFormat = m_RenderTargetDepths[m_FrameIndex]->GetDesc().Format;
    psd.pRootSignature = m_RootSignature.Get();

    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&m_Pipeline)));
}


DXApp::~DXApp() {
    for (const auto& fenceValue : m_FenceValues) {
        ThrowIfFailed(m_Queue->Signal(m_Fence.Get(), fenceValue));

        if (m_Fence->GetCompletedValue() < fenceValue) {
            ThrowIfFailed(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent));
            WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
        }
    }
}


void DXApp::OnUpdate(float dt) {
    SDL_SetWindowTitle(m_Window, std::string("Raekor - " + std::to_string(Timer::sToMilliseconds(dt)) + " ms").c_str());

    const UINT64 currentFenceValue = m_FenceValues[m_FrameIndex];

    ThrowIfFailed(m_Queue->Signal(m_Fence.Get(), currentFenceValue));

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex]) {
        ThrowIfFailed(m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    const auto& cmdList = m_CommandLists[m_FrameIndex];

    cmdList->Reset(m_CommnadAllocators[m_FrameIndex].Get(), nullptr);
    
    const auto clearColour = glm::vec4(0.11f, 0.2f, 0.3f, 1.0f);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
    
    const auto rtvPtr = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_RtvHeap->GetCPUDescriptorHandleForHeapStart()).Offset(m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * m_FrameIndex);
    const auto dsvPtr = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_DsvHeap->GetCPUDescriptorHandleForHeapStart()).Offset(m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV) * m_FrameIndex);
    
    // setup pass state
    cmdList->SetPipelineState(m_Pipeline.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->RSSetViewports(0, &CD3DX12_VIEWPORT(m_RenderTargets[m_FrameIndex].Get()));
    cmdList->OMSetRenderTargets(1, &rtvPtr, FALSE, &dsvPtr);
    cmdList->SetGraphicsRootSignature(m_RootSignature.Get());

    // clear render target
    cmdList->ClearRenderTargetView(rtvPtr, glm::value_ptr(clearColour), 1, &CD3DX12_RECT(0, 0, m_Viewport.size.x, m_Viewport.size.y));

    for (const auto& [entity, mesh] : m_Scene.view<Mesh>().each()) {
        const auto& indexBuffer = m_Buffers[mesh.indexBuffer];
        const auto& vertexBuffer = m_Buffers[mesh.vertexBuffer];

        D3D12_INDEX_BUFFER_VIEW indexView = {};
        indexView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
        indexView.Format = DXGI_FORMAT_R32_UINT;
        indexView.SizeInBytes = mesh.indices.size() * sizeof(mesh.indices[0]);

        D3D12_VERTEX_BUFFER_VIEW vertexView = {};
        vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vertexView.SizeInBytes = mesh.positions.size() * 40; // TODO: store interleaved vertices in Mesh and get the size from that
        vertexView.StrideInBytes = 40; // TODO: derive from input layout since its all tightly packed

        cmdList->IASetIndexBuffer(&indexView);
        cmdList->IASetVertexBuffers(0, 1, &vertexView);
        
        cmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
    }

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON));

    cmdList->Close();

    const std::array commandLists = { static_cast<ID3D12CommandList*>(cmdList.Get()) };
    m_Queue->ExecuteCommandLists(commandLists.size(), commandLists.data());
    
    ThrowIfFailed(m_Swapchain->Present(0, 0));

    m_FenceValues[m_FrameIndex] = currentFenceValue + 1;
}


void DXApp::OnEvent(const SDL_Event& event) {
    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            while (1) {
                SDL_Event ev;
                SDL_PollEvent(&ev);

                if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
                    break;
                }
            }
        }
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
            if (SDL_GetWindowID(m_Window) == event.window.windowID) {
                m_Running = false;
            }
        }
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            const UINT64 currentFenceValue = m_FenceValues[m_FrameIndex];

            ThrowIfFailed(m_Queue->Signal(m_Fence.Get(), currentFenceValue));

            if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex]) {
                ThrowIfFailed(m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent));
                WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
            }

            for (auto& renderTarget : m_RenderTargets) {
                renderTarget.Reset();
            }

            m_RenderTargets.resize(FrameCount);

            ThrowIfFailed(m_Swapchain->ResizeBuffers(
                FrameCount, m_Viewport.size.x, m_Viewport.size.y, 
                DXGI_FORMAT_B8G8R8A8_UNORM, 0
            ));

            auto heapPtr = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_RtvHeap->GetCPUDescriptorHandleForHeapStart());

            for (uint32_t i = 0; i < FrameCount; i++) {
                ThrowIfFailed(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i])));
                m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, heapPtr);
                heapPtr.Offset(m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
            }
        }
    }
}

}