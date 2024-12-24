#pragma once

#include "RTTI.h"
#include "Camera.h"
#include "Device.h"
#include "Profiler.h"
#include "CommandList.h"

namespace RK {
class Scene;
}

namespace RK::DX12 {

enum EResourceType
{
    RESOURCE_TYPE_BUFFER,
    RESOURCE_TYPE_TEXTURE
};


// Index into RenderGraphBuilder::m_ResourceDescriptions
enum RenderGraphResourceID : uint32_t {};
// Index into RenderGraphBuilder::m_ResourceViewDescriptions
enum RenderGraphResourceViewID : uint32_t {};


// resource description so we know how to allocate the resource
struct RenderGraphResourceDesc
{
    ResourceID mResourceID;
    EResourceType mResourceType;
    
    union
    {
        Buffer::Desc mBufferDesc;
        Texture::Desc mTextureDesc;
    };
};



// descriptor descriptions so we know how to create the resource view (RTV, DSV, SRV, or UAV)
struct RenderGraphResourceViewDesc
{
    /* The RenderGraphResourceID to create a view of. */
    RenderGraphResourceID mGraphResourceID;
    /* the resource view description. */
    RenderGraphResourceDesc mResourceDesc;
};



// resource handle to the actual buffer / texture owned by the device
struct RenderGraphResource
{
    /* the resource type of the on-device resource. */
    EResourceType mResourceType; 
    /* The device-owned resource index. use BufferID(mResourceID) or TextureID(mResourceID) to get the device index. */
    ResourceID mResourceID; 
    /* Was this resource imported and should it stay alive outside the graph? */
    bool mImported = false;
    /* Bindless descriptor heap index */
    DescriptorID mDescriptorID;
};



class RenderGraphBuilder
{
    friend class RenderGraph;
    friend class RenderGraphResources;

public:
    RenderGraphResourceID Create(const Buffer::Desc& inDesc);
    RenderGraphResourceID Create(const Texture::Desc& inDesc);

    RenderGraphResourceID Import(Device& inDevice, BufferID inBuffer);
    RenderGraphResourceID Import(Device& inDevice, TextureID inTexture);

    RenderGraphResourceViewID RenderTarget(RenderGraphResourceID inGraphResourceID);
    RenderGraphResourceViewID DepthStencilTarget(RenderGraphResourceID inGraphResourceID);

    RenderGraphResourceViewID Read(RenderGraphResourceID inGraphResourceID);
    RenderGraphResourceViewID ReadIndirectArgs(RenderGraphResourceID inGraphResourceID);
    
    RenderGraphResourceViewID Write(RenderGraphResourceID inGraphResourceID);

    RenderGraphResourceViewID ReadTexture(RenderGraphResourceID inGraphResourceID, uint32_t inMip);
    RenderGraphResourceViewID WriteTexture(RenderGraphResourceID inGraphResourceID, uint32_t inMip);

    const RenderGraphResourceDesc& GetResourceDesc(RenderGraphResourceID inGraphResourceID) { return m_ResourceDescriptions[inGraphResourceID]; }
    const RenderGraphResourceViewDesc& GetResourceViewDesc(RenderGraphResourceViewID inGraphResourceViewID) { return m_ResourceViewDescriptions[inGraphResourceViewID]; }

private:
    RenderGraphResourceID Create(const RenderGraphResourceDesc& inDesc);
    RenderGraphResourceViewDesc& EmplaceDescriptorDesc(RenderGraphResourceID inGraphResourceID);

    void Clear();
    
    void PushRenderPass(IRenderPass* inRenderPass)  { m_CurrentRenderPasses.push_back(inRenderPass); }
    void PopRenderPass()                            { m_CurrentRenderPasses.pop_back(); }
    IRenderPass* GetRenderPass()                    { return m_CurrentRenderPasses.empty() ? nullptr : m_CurrentRenderPasses.back(); }

    Array<IRenderPass*> m_CurrentRenderPasses;
    Array<RenderGraphResourceDesc> m_ResourceDescriptions;
    Array<RenderGraphResourceViewDesc> m_ResourceViewDescriptions;
};



class RenderGraphResourceAllocator
{
public:
    ~RenderGraphResourceAllocator() { Clear(); }

    void Reserve(Device& inDevice, uint64_t inSize, uint64_t inAlignment);
    void Release();

    void Clear() { if (m_VirtualBlock) m_VirtualBlock->Clear(); }

    BufferID CreateBuffer(Device& inDevice, const Buffer::Desc& inDesc);
    TextureID CreateTexture(Device& inDevice, const Texture::Desc& inDesc);

    uint64_t GetSize() const { return m_Size; }
    uint64_t GetOffset() const { return m_Offset; }

private:
    uint64_t Allocate(Device& inDevice, const D3D12_RESOURCE_DESC& inDesc);

private:
    uint64_t m_Size = 0;
    uint64_t m_Offset = 0;

    ComPtr<D3D12MA::Allocation> m_Allocation = nullptr;
    ComPtr<D3D12MA::VirtualBlock> m_VirtualBlock = nullptr;
};


class RenderGraphResources
{
public:
    friend class RenderGraph;

    void Clear(Device& inDevice);
    void Compile(Device& inDevice, const RenderGraphBuilder& inBuilder);

    const RenderGraphResourceAllocator& GetAllocator() const { return m_Allocator; }

    BufferID GetBufferView(RenderGraphResourceViewID inResource) const;
    TextureID GetTextureView(RenderGraphResourceViewID inResource) const;
    ResourceID GetResourceView(RenderGraphResourceViewID inResource) const;

    BufferID GetBuffer(RenderGraphResourceID inResource) const;
    TextureID GetTexture(RenderGraphResourceID inResource) const;
    ResourceID GetResource(RenderGraphResourceID inResource) const;

    uint32_t GetBindlessHeapIndex(RenderGraphResourceID inResource) const { return m_Resources[inResource].mDescriptorID.GetIndex(); }
    uint32_t GetBindlessHeapIndex(RenderGraphResourceViewID inResource) const { return m_ResourceViews[inResource].mDescriptorID.GetIndex(); }

    bool IsBuffer(RenderGraphResourceViewID inResource) const { return m_ResourceViews[inResource].mResourceType == RESOURCE_TYPE_BUFFER; }
    bool IsTexture(RenderGraphResourceViewID inResource) const { return m_ResourceViews[inResource].mResourceType == RESOURCE_TYPE_TEXTURE; }

private:
    RenderGraphResourceAllocator m_Allocator;
    Array<RenderGraphResource> m_Resources;
    Array<RenderGraphResource> m_ResourceViews;
};



class IRenderPass
{
public:
    friend class Device;
    friend class RenderGraph;
    friend class RenderGraphBuilder;

    template<typename T>
    using SetupFn = std::function<void(RenderGraphBuilder& inBuilder, IRenderPass* inRenderPass, T& inData)>;

    template<typename T>
    using ExecFn = std::function<void(T& inData, const RenderGraphResources& inResources, CommandList& inCmdList)>;

    IRenderPass(const String& inName) : m_Name(inName) {}

    virtual bool IsCompute() = 0;
    virtual bool IsGraphics() = 0;
    
    virtual void Setup(RenderGraphBuilder& inBuilder) = 0;
    virtual void Execute(const RenderGraphResources& inResources, CommandList& inCmdList) = 0;
    
    const String& GetName() const { return m_Name; }

    bool IsCreated(RenderGraphResourceID inResource) const;
    bool IsRead(RenderGraphResourceViewID inResource) const;
    bool IsWritten(RenderGraphResourceViewID inResource) const;

    /* Indicate if this pass calls external D3D12 code and needs to rebind defaults after. */
    bool IsExternal() const { return m_IsExternal; }
    void SetExternal(bool inValue) { m_IsExternal = inValue; }

    /* Reserve memory in the frame-based ring allocator. The render graph uses this reserved size to pre-allocate the ring buffer. */
    void ReserveMemory(uint32_t inSize) { m_ConstantsSize += inSize; }

    /* AddExitBarrier is exposed to the user to add manual barriers around resources they have no control over (external code like FSR2).
    D3D12_RESOURCE_TRANSITION_BARRIER::StateAfter could be overwritten by the graph if it finds a better match during graph compilation. */
    void AddExitBarrier(const D3D12_RESOURCE_BARRIER& inBarrier) { m_ExitBarriers.push_back(inBarrier); }

    D3D12_COMPUTE_PIPELINE_STATE_DESC  CreatePipelineStateDesc(Device& inDevice, const ComputeProgram& inShaderProgram);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc(Device& inDevice, const GraphicsProgram& inShaderProgram);

private:
    void FlushBarriers(Device& inDevice, CommandList& inCmdList, const Slice<D3D12_RESOURCE_BARRIER>& inBarriers) const;
    void SetRenderTargets(Device& inDevice, const RenderGraphResources& inRenderResources, CommandList& inCmdList) const;


protected:
    String	  m_Name;
    uint32_t  m_ConstantsSize = 0;
    bool	  m_IsExternal = false;

    Array<RenderGraphResourceID>     m_CreatedResources;
    Array<RenderGraphResourceViewID> m_ReadResources;
    Array<RenderGraphResourceViewID> m_WrittenResources;

    Array<DXGI_FORMAT> m_RenderTargetFormats;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_UNKNOWN;

    Array<D3D12_RESOURCE_BARRIER> m_ExitBarriers;
};



template<typename T>
class RenderPass : public IRenderPass
{
public:
    friend class RenderGraph;

    RenderPass(const std::string& inName, const IRenderPass::ExecFn<T>& inExecute) :
        IRenderPass(inName), m_Execute(inExecute)
    {
    }

    virtual void Setup(RenderGraphBuilder& inBuilder) override 
    { 
        PROFILE_SCOPE_CPU(m_Name.c_str()); 
        m_Setup(inBuilder, this, m_Data); 
    }

    virtual void Execute(const RenderGraphResources& inResources, CommandList& inCmdList) override 
    { 
        PROFILE_SCOPE_CPU(m_Name.c_str());
        m_Execute(m_Data, inResources, inCmdList); 
    }

    T& GetData() { return m_Data; }

protected:
    T			m_Data;
    SetupFn<T>	m_Setup;
    ExecFn<T>	m_Execute;
};



template<typename T>
class GraphicsRenderPass : public RenderPass<T>
{
public:
    GraphicsRenderPass(const std::string& inName, const IRenderPass::ExecFn<T>& inExecuteFn) : RenderPass<T>(inName, inExecuteFn) {}

    virtual bool IsCompute() override { return false; }
    virtual bool IsGraphics() override { return true; }
};



template<typename T>
class ComputeRenderPass : public RenderPass<T>
{
public:
    ComputeRenderPass(const std::string& inName, const IRenderPass::ExecFn<T>& inExecuteFn) : RenderPass<T>(inName, inExecuteFn) {}

    virtual bool IsCompute() override { return true; }
    virtual bool IsGraphics() override { return false; }
};



class RenderGraph
{
public:
    RenderGraph(Device& inDevice, const Viewport& inViewport, uint32_t inFrameCount);

    template<typename T, typename PassType>
    const T& AddPass(const String& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute);

    template<typename T>
    const T& AddGraphicsPass(const String& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute);

    template<typename T>
    const T& AddComputePass(const String& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute);

    /* Clears the graph by destroying all the render passes and their associated resources. After clearing the user is free to call Compile again. */
    void Clear(Device& inDevice);

    /* Compiles the entire graph, performs validity checks, and calculates optimal barriers. */
    bool Compile(Device& inDevice, const GlobalConstants& inGlobalConstants);

    /* Execute the entire graph into inCmdList. inCmdList should be open (.Begin() called). */
    void Execute(Device& inDevice, CommandList& inCmdList);

    /* Dump the entire graph to GraphViz text, can be written directly to a file and opened using the VS Code extension. */
    String	ToGraphVizText(const Device& inDevice, TextureID inBackBuffer) const;

    const Viewport& GetViewport() const { return m_Viewport; }
    const RenderGraphResources& GetResources() const { return m_RenderGraphResources; }

    RingAllocator& GetPerPassAllocator() { return m_PerPassAllocator; }
    RingAllocator& GetPerFrameAllocator() { return m_PerFrameAllocator; }
    GlobalConstantsAllocator& GetGlobalConstantsAllocator() { return m_GlobalConstantsAllocator; }

private:
    const Viewport& m_Viewport;
    const uint32_t m_FrameCount;
    
    uint32_t m_TimestampCount = 0;
    BufferID m_TimestampReadbackBuffer;
    ComPtr<ID3D12QueryHeap> m_TimestampQueryHeap = nullptr;

    RingAllocator m_PerFrameAllocator;
    RingAllocator m_PerPassAllocator;
    GlobalConstantsAllocator m_GlobalConstantsAllocator;

    RenderGraphBuilder m_RenderGraphBuilder;
    RenderGraphResources m_RenderGraphResources;
    Array<UniquePtr<IRenderPass>> m_RenderPasses;
    Array<D3D12_RESOURCE_BARRIER> m_FinalBarriers;
};


template<typename T, typename PassType>
const T& RenderGraph::AddPass(const String& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute)
{
    // have to use index here, taking the emplace_back ref would invalidate it if we add aditional passes inside of the setup function
    const int pass_index = m_RenderPasses.size();
    m_RenderPasses.emplace_back(std::make_unique<PassType>(inName, inExecute));

    m_RenderGraphBuilder.PushRenderPass(m_RenderPasses[pass_index].get());

    inSetup(m_RenderGraphBuilder, m_RenderPasses[pass_index].get(), static_cast<RenderPass<T>*>( m_RenderPasses[pass_index].get() )->GetData());

    m_RenderGraphBuilder.PopRenderPass();

    return static_cast<RenderPass<T>*>( m_RenderPasses[pass_index].get() )->GetData();
}


template<typename T>
const T& RenderGraph::AddGraphicsPass(const String& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute)
{
    return RenderGraph::AddPass<T, GraphicsRenderPass<T>>(inName, inSetup, inExecute);
}


template<typename T>
const T& RenderGraph::AddComputePass(const String& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute)
{
    return RenderGraph::AddPass<T, ComputeRenderPass<T>>(inName, inSetup, inExecute);
}

}