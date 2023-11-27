#pragma once

#include "DXDevice.h"
#include "DXCommandList.h"

namespace Raekor {
class Scene;
class Viewport;
}

namespace Raekor::DX12 {

// Index into RenderGraphBuilder::m_ResourceDescriptions
using RenderGraphResourceID = uint32_t;
// Index into RenderGraphBuilder::m_ResourceViewDescriptions
using RenderGraphResourceViewID = uint32_t;



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
    RenderGraphResourceViewID Write(RenderGraphResourceID inGraphResourceID);

    const RenderGraphResourceDesc& GetResourceDesc(RenderGraphResourceID inGraphResourceID) { return m_ResourceDescriptions[inGraphResourceID]; }
    const RenderGraphResourceViewDesc& GetResourceViewDesc(RenderGraphResourceViewID inGraphResourceViewID) { return m_ResourceViewDescriptions[inGraphResourceViewID]; }

private:
    RenderGraphResourceID Create(const RenderGraphResourceDesc& inDesc);
    RenderGraphResourceViewDesc& EmplaceDescriptorDesc(RenderGraphResourceID inGraphResourceID);

    void Clear();
    
    void StartPass(IRenderPass* inRenderPass);
    void EndPass(IRenderPass* inRenderPass);

    IRenderPass* m_CurrentRenderPass = nullptr;
    std::vector<RenderGraphResourceDesc> m_ResourceDescriptions;
    std::vector<RenderGraphResourceViewDesc> m_ResourceViewDescriptions;
};


enum EGlobalResource
{
    GLOBAL_RESOURCE_SKY_CUBE_TEXTURE,
    GLOBAL_RESOURCE_SKY_CUBE_CONVOLVED_TEXTURE,
    GLOBAL_RESOURCE_COUNT,
};


class RenderGraphResources
{
public:
    void Clear(Device& inDevice);
    void Compile(Device& inDevice, const RenderGraphBuilder& inBuilder);

    BufferID GetBufferView(RenderGraphResourceViewID inResource) const;
    TextureID GetTextureView(RenderGraphResourceViewID inResource) const;
    ResourceID GetResourceView(RenderGraphResourceViewID inResource) const;

    BufferID GetBuffer(RenderGraphResourceID inResource) const;
    TextureID GetTexture(RenderGraphResourceID inResource) const;
    ResourceID GetResource(RenderGraphResourceID inResource) const;

    inline bool IsBuffer(RenderGraphResourceViewID inResource) const { return m_ResourceViews[inResource].mResourceType == RESOURCE_TYPE_BUFFER; }
    inline bool IsTexture(RenderGraphResourceViewID inResource) const { return m_ResourceViews[inResource].mResourceType == RESOURCE_TYPE_TEXTURE; }

private:
    std::vector<RenderGraphResource> m_Resources;
    std::vector<RenderGraphResource> m_ResourceViews;
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

    IRenderPass(const std::string& inName) : m_Name(inName) {}

    virtual bool IsCompute() = 0;
    virtual bool IsGraphics() = 0;

    virtual void Setup(RenderGraphBuilder& inBuilder) = 0;
    virtual void Execute(const RenderGraphResources& inResources, CommandList& inCmdList) = 0;

    bool IsCreated(RenderGraphResourceID inResource) const;
    bool IsRead(RenderGraphResourceViewID inResource) const;
    bool IsWritten(RenderGraphResourceViewID inResource) const;

    bool IsExternal() const { return m_IsExternal; }
    void SetExternal(bool inValue) { m_IsExternal = inValue; }

    /* Reserve memory in the frame-based ring allocator. The render graph uses this reserved size to pre-allocate the ring buffer. */
    void ReserveMemory(uint32_t inSize) { m_ConstantsSize += inSize; }

    /* AddExitBarrier is exposed to the user to add manual barriers around resources they have no control over (external code like FSR2).
    D3D12_RESOURCE_TRANSITION_BARRIER::StateAfter could be overwritten by the graph if it finds a better match during graph compilation. */
    void AddExitBarrier(const D3D12_RESOURCE_BARRIER& inBarrier) { m_ExitBarriers.push_back(inBarrier); }
    void AddEntryBarrier(const D3D12_RESOURCE_BARRIER& inBarrier) { m_EntryBarriers.push_back(inBarrier); }

    inline const std::string& GetName() const { return m_Name; }

private:
    void FlushBarriers(Device& inDevice, CommandList& inCmdList, const Slice<D3D12_RESOURCE_BARRIER>& inBarriers) const;
    void SetRenderTargets(Device& inDevice, const RenderGraphResources& inRenderResources, CommandList& inCmdList) const;

protected:
    std::string	m_Name;
    uint32_t	m_ConstantsSize = 0;
    bool		m_IsExternal = false;

    std::vector<RenderGraphResourceID>     m_CreatedResources;
    std::vector<RenderGraphResourceViewID> m_ReadResources;
    std::vector<RenderGraphResourceViewID> m_WrittenResources;

    std::vector<DXGI_FORMAT> m_RenderTargetFormats;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_UNKNOWN;

    std::vector<D3D12_RESOURCE_BARRIER> m_ExitBarriers;
    std::vector<D3D12_RESOURCE_BARRIER> m_EntryBarriers;
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

    virtual void Setup(RenderGraphBuilder& inBuilder) override { m_Setup(inBuilder, this, m_Data); }
    virtual void Execute(const RenderGraphResources& inResources, CommandList& inCmdList) override { m_Execute(m_Data, inResources, inCmdList); }

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
    GraphicsRenderPass(const std::string& inName, const IRenderPass::ExecFn<T>& inExecuteFn) :
        RenderPass<T>(inName, inExecuteFn)
    {
    }

    virtual bool IsCompute() override { return false; }
    virtual bool IsGraphics() override { return true; }
};



template<typename T>
class ComputeRenderPass : public RenderPass<T>
{
public:
    ComputeRenderPass(const std::string& inName, const IRenderPass::ExecFn<T>& inExecuteFn) :
        RenderPass<T>(inName, inExecuteFn)
    {
    }

    virtual bool IsCompute() override { return true; }
    virtual bool IsGraphics() override { return false; }
};



class RenderGraph
{
public:
    RenderGraph(Device& inDevice, const Viewport& inViewport, uint32_t inFrameCount);

    template<typename T, typename PassType>
    const T& AddPass(const std::string& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute);

    template<typename T>
    const T& AddGraphicsPass(const std::string& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute);

    template<typename T>
    const T& AddComputePass(const std::string& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute);

    template<typename T> RenderPass<T>* GetPass();
    template<typename T> RenderPass<T>* GetPass() const;

    /* Clears the graph by destroying all the render passes and their associated resources. After clearing the user is free to call Compile again. */
    void Clear(Device& inDevice);

    /* Compiles the entire graph, performs validity checks, and calculates optimal barriers. */
    bool Compile(Device& inDevice);

    /* Execute the entire graph into inCmdList. inCmdList should be open (.Begin() called). */
    void Execute(Device& inDevice, CommandList& inCmdList, uint64_t inFrameCounter);

    /* Dump the entire graph to GraphViz text, can be written directly to a file and opened using the VS Code extension. */
    std::string	ToGraphVizText(const Device& inDevice, TextureID inBackBuffer) const;

    const Viewport& GetViewport() const { return m_Viewport; }
    const RenderGraphResources& GetResources() const { return m_RenderGraphResources; }

    RingAllocator& GetPerPassAllocator() { return m_PerPassAllocator; }
    RingAllocator& GetPerFrameAllocator() { return m_PerFrameAllocator; }
    uint32_t& GetPerFrameAllocatorOffset() { return m_PerFrameAllocatorOffset; }
    GlobalConstantsAllocator& GetGlobalConstantsAllocator() { return m_GlobalConstantsAllocator; }

private:
    const Viewport& m_Viewport;
    const uint32_t m_FrameCount;

    uint32_t m_PerFrameAllocatorOffset = 0;
    RingAllocator m_PerFrameAllocator;
    RingAllocator m_PerPassAllocator;
    GlobalConstantsAllocator m_GlobalConstantsAllocator;

    RenderGraphBuilder m_RenderGraphBuilder;
    RenderGraphResources m_RenderGraphResources;
    std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
};



template<typename T, typename PassType>
const T& RenderGraph::AddPass(const std::string& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute)
{
    auto& pass = m_RenderPasses.emplace_back(std::make_unique<PassType>(inName, inExecute));

    m_RenderGraphBuilder.StartPass(pass.get());

    inSetup(m_RenderGraphBuilder, pass.get(), static_cast<RenderPass<T>*>( pass.get() )->GetData());

    m_RenderGraphBuilder.EndPass(pass.get());

    return static_cast<RenderPass<T>*>( pass.get() )->GetData();
}



template<typename T>
const T& RenderGraph::AddGraphicsPass(const std::string& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute)
{
    return RenderGraph::AddPass<T, GraphicsRenderPass<T>>(inName, inSetup, inExecute);
}



template<typename T>
const T& RenderGraph::AddComputePass(const std::string& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute)
{
    return RenderGraph::AddPass<T, ComputeRenderPass<T>>(inName, inSetup, inExecute);
}



template<typename T>
RenderPass<T>* RenderGraph::GetPass()
{
    for (auto& renderpass : m_RenderPasses)
    {
        if (auto base = static_cast<RenderPass<T>*>( renderpass.get() ))
            if (base->GetData().GetRTTI() == gGetRTTI<T>())
                return base;
    }

    return nullptr;
}



template<typename T>
RenderPass<T>* RenderGraph::GetPass() const
{
    for (auto& renderpass : m_RenderPasses)
    {
        if (auto base = static_cast<RenderPass<T>*>( renderpass.get() ))
            if (base->GetData().GetRTTI() == gGetRTTI<T>())
                return base;
    }

    return nullptr;
}

}