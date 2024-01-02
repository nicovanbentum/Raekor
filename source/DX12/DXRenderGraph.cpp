#include "pch.h"
#include "DXRenderGraph.h"
#include "shared.h"

namespace Raekor::DX12 {

RenderGraphResourceID RenderGraphBuilder::Create(const Buffer::Desc& inDesc)
{
    return Create(RenderGraphResourceDesc { .mResourceType = EResourceType::RESOURCE_TYPE_BUFFER, .mBufferDesc = inDesc });
}



RenderGraphResourceID RenderGraphBuilder::Create(const Texture::Desc& inDesc)
{
    return Create(RenderGraphResourceDesc { .mResourceType = EResourceType::RESOURCE_TYPE_TEXTURE, .mTextureDesc = inDesc });
}



RenderGraphResourceID RenderGraphBuilder::Import(Device& inDevice, BufferID inBuffer)
{
    const auto& buffer = inDevice.GetBuffer(inBuffer);

    m_ResourceDescriptions.emplace_back(RenderGraphResourceDesc
    {
        .mResourceID = inBuffer,
        .mResourceType = EResourceType::RESOURCE_TYPE_BUFFER,
        .mBufferDesc = buffer.GetDesc()
    });

    return RenderGraphResourceID(m_ResourceDescriptions.size() - 1);
}



RenderGraphResourceID RenderGraphBuilder::Import(Device& inDevice, TextureID inTexture)
{
    const auto& texture = inDevice.GetTexture(inTexture);

    m_ResourceDescriptions.emplace_back(RenderGraphResourceDesc 
    { 
        .mResourceID = inTexture,
        .mResourceType = EResourceType::RESOURCE_TYPE_TEXTURE, 
        .mTextureDesc = texture.GetDesc(),
    });

    return RenderGraphResourceID(m_ResourceDescriptions.size() - 1);
}



RenderGraphResourceViewID RenderGraphBuilder::RenderTarget(RenderGraphResourceID inGraphResourceID)
{
    RenderGraphResourceViewDesc& desc = EmplaceDescriptorDesc(inGraphResourceID);
    assert(desc.mResourceDesc.mResourceType != RESOURCE_TYPE_BUFFER);
    desc.mResourceDesc.mTextureDesc.usage = Texture::Usage::RENDER_TARGET;

    const RenderGraphResourceViewID graph_resource_id = RenderGraphResourceViewID(m_ResourceViewDescriptions.size() - 1);

    GetRenderPass()->m_WrittenResources.push_back(graph_resource_id);
    GetRenderPass()->m_RenderTargetFormats.push_back(desc.mResourceDesc.mTextureDesc.format);

    return graph_resource_id;
}



RenderGraphResourceViewID RenderGraphBuilder::DepthStencilTarget(RenderGraphResourceID inGraphResourceID)
{
    assert(GetRenderPass()->m_DepthStencilFormat == DXGI_FORMAT_UNKNOWN);

    RenderGraphResourceViewDesc& desc = EmplaceDescriptorDesc(inGraphResourceID);
    assert(desc.mResourceDesc.mResourceType != RESOURCE_TYPE_BUFFER);
    
    desc.mResourceDesc.mTextureDesc.usage = Texture::Usage::DEPTH_STENCIL_TARGET;

    const RenderGraphResourceViewID graph_resource_id = RenderGraphResourceViewID(m_ResourceViewDescriptions.size() - 1);

    GetRenderPass()->m_WrittenResources.push_back(graph_resource_id);
    GetRenderPass()->m_DepthStencilFormat = desc.mResourceDesc.mTextureDesc.format;

    return graph_resource_id;
}



RenderGraphResourceViewID RenderGraphBuilder::Write(RenderGraphResourceID inGraphResourceID)
{
    RenderGraphResourceViewDesc& desc = EmplaceDescriptorDesc(inGraphResourceID);

    if (desc.mResourceDesc.mResourceType == RESOURCE_TYPE_BUFFER)
    {
        desc.mResourceDesc.mBufferDesc.usage = Buffer::Usage::SHADER_READ_WRITE;
    }
    else if (desc.mResourceDesc.mResourceType == RESOURCE_TYPE_TEXTURE)
    {
        desc.mResourceDesc.mTextureDesc.usage = Texture::Usage::SHADER_READ_WRITE;
    }

    const RenderGraphResourceViewID graph_resource_id = RenderGraphResourceViewID(m_ResourceViewDescriptions.size() - 1);

    GetRenderPass()->m_WrittenResources.push_back(graph_resource_id);

    return graph_resource_id;
}



RenderGraphResourceViewID RenderGraphBuilder::Read(RenderGraphResourceID inGraphResourceID)
{
    RenderGraphResourceViewDesc& desc = EmplaceDescriptorDesc(inGraphResourceID);

    if (desc.mResourceDesc.mResourceType == RESOURCE_TYPE_BUFFER)
    {
        desc.mResourceDesc.mBufferDesc.usage = Buffer::Usage::SHADER_READ_ONLY;
    }
    else if (desc.mResourceDesc.mResourceType == RESOURCE_TYPE_TEXTURE)
    {
        desc.mResourceDesc.mTextureDesc.usage = Texture::Usage::SHADER_READ_ONLY;
    }

    const RenderGraphResourceViewID graph_resource_id = RenderGraphResourceViewID(m_ResourceViewDescriptions.size() - 1);

    GetRenderPass()->m_ReadResources.push_back(graph_resource_id);

    return graph_resource_id;
}



RenderGraphResourceViewID RenderGraphBuilder::ReadIndirectArgs(RenderGraphResourceID inGraphResourceID)
{
    RenderGraphResourceViewDesc& desc = EmplaceDescriptorDesc(inGraphResourceID);
    assert(desc.mResourceDesc.mResourceType == RESOURCE_TYPE_BUFFER);

    desc.mResourceDesc.mBufferDesc.usage = Buffer::Usage::INDIRECT_ARGUMENTS;

    const RenderGraphResourceViewID graph_resource_id = RenderGraphResourceViewID(m_ResourceViewDescriptions.size() - 1);

    GetRenderPass()->m_ReadResources.push_back(graph_resource_id);

    return graph_resource_id;
}



RenderGraphResourceViewID RenderGraphBuilder::ReadTexture(RenderGraphResourceID inGraphResourceID, uint32_t inMip)
{
    assert(m_ResourceDescriptions[inGraphResourceID].mResourceType == RESOURCE_TYPE_TEXTURE);
    assert(inMip < m_ResourceDescriptions[inGraphResourceID].mTextureDesc.mipLevels);

    RenderGraphResourceViewDesc& desc = EmplaceDescriptorDesc(inGraphResourceID);
    desc.mResourceDesc.mTextureDesc.usage = Texture::Usage::SHADER_READ_ONLY;
    desc.mResourceDesc.mTextureDesc.baseMip = inMip;
    desc.mResourceDesc.mTextureDesc.mipLevels = 1;

    const RenderGraphResourceViewID graph_resource_id = RenderGraphResourceViewID(m_ResourceViewDescriptions.size() - 1);

    GetRenderPass()->m_ReadResources.push_back(graph_resource_id);

    return graph_resource_id;
}



RenderGraphResourceViewID RenderGraphBuilder::WriteTexture(RenderGraphResourceID inGraphResourceID, uint32_t inMip)
{
    assert(inMip < m_ResourceDescriptions[inGraphResourceID].mTextureDesc.mipLevels);
    assert(m_ResourceDescriptions[inGraphResourceID].mResourceType == RESOURCE_TYPE_TEXTURE);
    
    RenderGraphResourceViewDesc& desc = EmplaceDescriptorDesc(inGraphResourceID);
    desc.mResourceDesc.mTextureDesc.usage = Texture::Usage::SHADER_READ_WRITE;
    desc.mResourceDesc.mTextureDesc.baseMip = inMip;
    desc.mResourceDesc.mTextureDesc.mipLevels = 1;

    const RenderGraphResourceViewID graph_resource_id = RenderGraphResourceViewID(m_ResourceViewDescriptions.size() - 1);

    GetRenderPass()->m_WrittenResources.push_back(graph_resource_id);

    return graph_resource_id;
}



RenderGraphResourceID RenderGraphBuilder::Create(const RenderGraphResourceDesc& inDesc)
{
    m_ResourceDescriptions.emplace_back(inDesc);

    const RenderGraphResourceID graph_resource_id = RenderGraphResourceID(m_ResourceDescriptions.size() - 1);

    GetRenderPass()->m_CreatedResources.push_back(graph_resource_id);

    return graph_resource_id;
}



RenderGraphResourceViewDesc& RenderGraphBuilder::EmplaceDescriptorDesc(RenderGraphResourceID inGraphResourceID)
{
    return m_ResourceViewDescriptions.emplace_back(RenderGraphResourceViewDesc
    {
        .mGraphResourceID = inGraphResourceID,
        .mResourceDesc = m_ResourceDescriptions[inGraphResourceID]
    });
}



void RenderGraphBuilder::Clear()
{
    m_CurrentRenderPasses.clear();
    m_ResourceDescriptions.clear();
    m_ResourceViewDescriptions.clear();
}



void RenderGraphResources::Clear(Device& inDevice)
{
    for (const RenderGraphResource& resource : m_Resources)
    {
        if (resource.mImported)
            continue;

        if (resource.mResourceType == RESOURCE_TYPE_BUFFER)
            inDevice.ReleaseBufferImmediate(BufferID(resource.mResourceID));

        if (resource.mResourceType == RESOURCE_TYPE_TEXTURE)
            inDevice.ReleaseTextureImmediate(TextureID(resource.mResourceID));
    }

    for (const RenderGraphResource& resource : m_ResourceViews)
    {
        if (resource.mImported)
            continue;

        if (resource.mResourceType == RESOURCE_TYPE_BUFFER)
            inDevice.ReleaseBufferImmediate(BufferID(resource.mResourceID));

        if (resource.mResourceType == RESOURCE_TYPE_TEXTURE)
            inDevice.ReleaseTextureImmediate(TextureID(resource.mResourceID));
    }

    m_Resources.clear();
    m_ResourceViews.clear();
}



void RenderGraphResources::Compile(Device& inDevice, const RenderGraphBuilder& inBuilder)
{
    // Allocate actual device buffers/textures
    // TODO: aliasing
    for (const RenderGraphResourceDesc& desc : inBuilder.m_ResourceDescriptions)
    {
        RenderGraphResource resource;
        resource.mResourceType = desc.mResourceType;

        if (desc.mResourceID.IsValid())
        {
            resource.mImported = true;
            resource.mResourceID = desc.mResourceID;
        }
        else if (desc.mResourceType == RESOURCE_TYPE_BUFFER)
        {
            resource.mResourceID = inDevice.CreateBuffer(desc.mBufferDesc);
        }
        else if (desc.mResourceType == RESOURCE_TYPE_TEXTURE)
        {
            resource.mResourceID = inDevice.CreateTexture(desc.mTextureDesc);
        }

        m_Resources.push_back(resource);
    }

    // Create buffer/texture resource views
    for (const RenderGraphResourceViewDesc& descriptor_desc : inBuilder.m_ResourceViewDescriptions)
    {
        const RenderGraphResource& resource = m_Resources[descriptor_desc.mGraphResourceID];
        const RenderGraphResourceDesc& resource_desc = inBuilder.m_ResourceDescriptions[descriptor_desc.mGraphResourceID];

        RenderGraphResource new_resource = resource;

        // generic ResourceID, cast to BufferID or TextureID to get the device-owned resource
        const ResourceID device_resource_index = m_Resources[descriptor_desc.mGraphResourceID].mResourceID;

        if (resource.mResourceType == RESOURCE_TYPE_BUFFER)
        {
            if (descriptor_desc.mResourceDesc.mBufferDesc != resource_desc.mBufferDesc)
                new_resource.mResourceID = inDevice.CreateBufferView(BufferID(device_resource_index), descriptor_desc.mResourceDesc.mBufferDesc);
        }
        else if (resource.mResourceType == RESOURCE_TYPE_TEXTURE)
        {
            if (descriptor_desc.mResourceDesc.mTextureDesc != resource_desc.mTextureDesc)
                new_resource.mResourceID = inDevice.CreateTextureView(TextureID(device_resource_index), descriptor_desc.mResourceDesc.mTextureDesc);
        }

        m_ResourceViews.push_back(new_resource);
    }
}



BufferID RenderGraphResources::GetBuffer(RenderGraphResourceID inResource) const
{
    const RenderGraphResource& resource = m_Resources[inResource];
    return resource.mResourceType == RESOURCE_TYPE_BUFFER ? BufferID(resource.mResourceID) : BufferID();
}



TextureID RenderGraphResources::GetTexture(RenderGraphResourceID inResource) const
{
    const RenderGraphResource& resource = m_Resources[inResource];
    assert(resource.mResourceType == RESOURCE_TYPE_TEXTURE);
    return resource.mResourceType == RESOURCE_TYPE_TEXTURE ? TextureID(resource.mResourceID) : TextureID();
}



ResourceID RenderGraphResources::GetResource(RenderGraphResourceID inResource) const
{
    return m_Resources[inResource].mResourceID;
}



BufferID RenderGraphResources::GetBufferView(RenderGraphResourceViewID inResource) const
{
    const RenderGraphResource& resource = m_ResourceViews[inResource];
    assert(resource.mResourceType == RESOURCE_TYPE_BUFFER);
    return BufferID(resource.mResourceID);
}



TextureID RenderGraphResources::GetTextureView(RenderGraphResourceViewID inResource) const
{
    const RenderGraphResource& resource = m_ResourceViews[inResource];
    assert(resource.mResourceType == RESOURCE_TYPE_TEXTURE);
    return TextureID(resource.mResourceID);
}



ResourceID RenderGraphResources::GetResourceView(RenderGraphResourceViewID inResource) const
{
    return m_ResourceViews[inResource].mResourceID;
}



bool IRenderPass::IsCreated(RenderGraphResourceID inResource) const
{
    for (RenderGraphResourceID resource : m_CreatedResources)
        if (resource == inResource)
            return true;
    return false;
}



bool IRenderPass::IsRead(RenderGraphResourceViewID inResource) const
{
    for (RenderGraphResourceViewID resource : m_ReadResources)
        if (resource == inResource)
            return true;
    return false;
}



bool IRenderPass::IsWritten(RenderGraphResourceViewID inResource) const
{
    for (RenderGraphResourceViewID resource : m_WrittenResources)
        if (resource == inResource)
            return true;
    return false;
}



void IRenderPass::FlushBarriers(Device& inDevice, CommandList& inCmdList, const Slice<D3D12_RESOURCE_BARRIER>& inBarriers) const
{
    if (!inBarriers.IsEmpty())
        inCmdList->ResourceBarrier(inBarriers.Length(), inBarriers.GetPtr());
}



void IRenderPass::SetRenderTargets(Device& inDevice, const RenderGraphResources& inRenderResources, CommandList& inCmdList) const
{
    auto& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto& dsv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // depth stencil attachment is invalid until a target texture (descriptor) is found and assigned
    auto dsv_descriptor = DescriptorID(DescriptorID::INVALID);

    auto rtv_handle_count = 0u;
    static auto rtv_handles = std::array<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT>{};

    for (const auto resource_id : m_WrittenResources)
    {
        if (!inRenderResources.IsTexture(resource_id))
            continue;

        const auto& texture = inDevice.GetTexture(inRenderResources.GetTextureView(resource_id));

        switch (texture.GetDesc().usage)
        {
            case Texture::RENDER_TARGET:
            {
                rtv_handles[rtv_handle_count++] = rtv_heap.GetCPUDescriptorHandle(texture.GetView());
            } break;

            case Texture::DEPTH_STENCIL_TARGET:
            {
                assert(!dsv_descriptor.IsValid()); // if you define multiple depth targets for a renderpass you're going to have a bad time, mmkay
                dsv_descriptor = texture.GetView();
            } break;
        }
    }

    if (dsv_descriptor.IsValid())
    {
        auto dsv_handle = dsv_heap.GetCPUDescriptorHandle(dsv_descriptor);
        inCmdList->OMSetRenderTargets(rtv_handle_count, rtv_handles.data(), FALSE, &dsv_handle);
    }
    else
        inCmdList->OMSetRenderTargets(rtv_handle_count, rtv_handles.data(), FALSE, nullptr);
}



RenderGraph::RenderGraph(Device& inDevice, const Viewport& inViewport, uint32_t inFrameCount) :
    m_Viewport(inViewport),
    m_FrameCount(inFrameCount)
{
}



void RenderGraph::Clear(Device& inDevice)
{
    m_RenderPasses.clear();
    m_RenderGraphBuilder.Clear();
    m_RenderGraphResources.Clear(inDevice);
    m_PerPassAllocator.DestroyBuffer(inDevice);
    m_GlobalConstantsAllocator.DestroyBuffer(inDevice);
}



bool RenderGraph::Compile(Device& inDevice)
{
    m_RenderGraphResources.Compile(inDevice, m_RenderGraphBuilder);

    /*
        PASS VALIDATION
        Does not do much at the moment, it validates:
        that we're not doing read_only and read_write for a resource in the same pass.
    */
    for (auto& renderpass : m_RenderPasses)
    {
        for (const auto& resource : renderpass->m_WrittenResources)
        {
            if (renderpass->IsRead(resource))
            {
                 std::cout << std::format("RenderGraph Error: Texture {} is both written to and read from in renderpass {}\n", gGetDebugName(inDevice.GetD3D12Resource(m_RenderGraphResources.GetTextureView(resource))), renderpass->GetName());
                return false;
            }
        }
    }



    /*
        GRAPH CONSTRUCTION
        The render graph is structured as follows:
        resources like textures and buffers are vertices
        Edges go from vertices to render passes (which aren't also  vertices, so fake graph theory!)
        an edge also describes the state the resource should be in during the corresponding render pass
        example:
                                RENDER TARGET
        GBUFFER_DEPTH_TEXTURE ------edge------> GBUFFER RENDER PASS


                                SHADER READ ONLY
        GBUFFER_DEPTH_TEXTURE ------edge------> SHADOW RENDER PASS

        The order of the node's edge array is guaranteed to follow the graph from start to finish as we linearly loop through it, there's no pass re-ordering.
    */
    struct GraphEdge
    {
        uint32_t mSubResource;
        uint32_t mRenderPassIndex;
        D3D12_RESOURCE_STATES mUsage;
    };

    struct GraphNode
    {
        uint32_t mBaseSubResource;
        uint32_t mSubResourceCount;
        EResourceType mResourceType;
        std::vector<GraphEdge> mEdges;
    };

    struct ResourceStates
    {
        ResourceStates(const GraphNode& inNode) : mSubResourceStates(inNode.mSubResourceCount, inNode.mEdges[0].mUsage) {}

        std::vector<D3D12_RESOURCE_STATES> mSubResourceStates;
    };

    std::unordered_map<RenderGraphResourceID, GraphNode> graph;

    // initialize all the graph nodes (all the created/imported resources)
    for (const auto& [resource_id, resource] : gEnumerate(m_RenderGraphResources.m_Resources))
    {
        auto& node = graph[RenderGraphResourceID(resource_id)];
        node.mResourceType = resource.mResourceType;

        if (node.mResourceType == EResourceType::RESOURCE_TYPE_BUFFER)
        {
            const auto& buffer = inDevice.GetBuffer(BufferID(resource.mResourceID));

            node.mBaseSubResource = buffer.GetBaseSubresource();
            node.mSubResourceCount = buffer.GetSubresourceCount();
        }
        else if (node.mResourceType == EResourceType::RESOURCE_TYPE_TEXTURE)
        {
            const auto& texture = inDevice.GetTexture(TextureID(resource.mResourceID));

            node.mBaseSubResource = texture.GetBaseSubresource();
            node.mSubResourceCount = texture.GetSubresourceCount();
        }
    }

    // go through all the written/read views and add edges to nodes
    for (const auto& [renderpass_index, renderpass] : gEnumerate(m_RenderPasses))
    {
        for (const auto& resource_id : renderpass->m_WrittenResources)
        {
            const auto& view_desc = m_RenderGraphBuilder.GetResourceViewDesc(resource_id);

            auto& node = graph[view_desc.mGraphResourceID];

            auto usage = D3D12_RESOURCE_STATE_COMMON;

            auto subresource_index = 0u;
            auto subresource_count = 1u;

            if (node.mResourceType == RESOURCE_TYPE_BUFFER)
            {
                usage = gGetResourceStates(inDevice.GetBuffer(m_RenderGraphResources.GetBufferView(resource_id)).GetDesc().usage);
            }
            else if (node.mResourceType == RESOURCE_TYPE_TEXTURE)
            {
                usage = gGetResourceStates(inDevice.GetTexture(m_RenderGraphResources.GetTextureView(resource_id)).GetDesc().usage);

                subresource_index = view_desc.mResourceDesc.mTextureDesc.baseMip;
                subresource_count = view_desc.mResourceDesc.mTextureDesc.mipLevels;
            }

            for (auto subresource = subresource_index; subresource < subresource_index + subresource_count; subresource++)
            {
                node.mEdges.emplace_back(GraphEdge { .mSubResource = subresource, .mRenderPassIndex = uint32_t(renderpass_index), .mUsage = usage });
            }
        }

        for (const auto& resource_id : renderpass->m_ReadResources)
        {
            const auto& view_desc = m_RenderGraphBuilder.GetResourceViewDesc(resource_id);

            auto& node = graph[view_desc.mGraphResourceID];

            auto usage = D3D12_RESOURCE_STATE_COMMON;

            auto subresource_index = 0u;
            auto subresource_count = 1u;

            if (node.mResourceType == RESOURCE_TYPE_BUFFER)
            {
                usage = gGetResourceStates(inDevice.GetBuffer(m_RenderGraphResources.GetBufferView(resource_id)).GetDesc().usage);
            }
            else if (node.mResourceType == RESOURCE_TYPE_TEXTURE)
            {
                usage = gGetResourceStates(inDevice.GetTexture(m_RenderGraphResources.GetTextureView(resource_id)).GetDesc().usage);

                subresource_index = view_desc.mResourceDesc.mTextureDesc.baseMip;
                subresource_count = view_desc.mResourceDesc.mTextureDesc.mipLevels;
            }

            for (auto subresource = subresource_index; subresource < subresource_index + subresource_count; subresource++)
            {
                if (node.mResourceType == RESOURCE_TYPE_TEXTURE)
                {
                    assert(subresource < inDevice.GetTexture(m_RenderGraphResources.GetTexture(view_desc.mGraphResourceID)).GetSubresourceCount());
                }

                node.mEdges.emplace_back(GraphEdge { .mSubResource = subresource, .mRenderPassIndex = uint32_t(renderpass_index), .mUsage = usage });
            }
        }
    }

    /*
        BARRIER GENERATION
        The state tracking algorithm here is quite dumb:

        Go through all the resources in the graph.
        if current pass usage and prev pass usage were both unordered access state, add an exit UAV barrier to prev pass.
        if a resource has 1 edge the initial state should match the edge state, no barriers needed. TODO: add validation for this single edge case.
        else loop linearly through all the edges, if state changes look up the previous edge's render pass and add an end-of render pass barrier.
        at the end of the loop we're left with the resource's final state for the graph
        if final state differs from the state in which it was created, add a begin-of render pass barrier to the pass that creates it. These barriers are skipped the first frame.

        TODO: Ideally we refactor this to use aliasing as that would reset the state of the resource, so we wouldn't need the begin-of render pass barriers.
                Probably also a good idea to submit begin-of barriers earlier, either all batched together at the end of the frame
    */
    for (const auto& [resource_id, node] : graph)
    {
        if (node.mEdges.size() < 2)
            continue;

        auto resource_ptr = (ID3D12Resource*)nullptr;

        if (node.mResourceType == RESOURCE_TYPE_BUFFER)
            resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetBuffer(resource_id));

        else if (node.mResourceType == RESOURCE_TYPE_TEXTURE)
            resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetTexture(resource_id));

        assert(resource_ptr);

        auto tracked_state = ResourceStates(node);
        auto previous_renderpass_index = node.mEdges[0].mRenderPassIndex;

        bool uav_barrier_added = false;

        for (auto edge_index = 1; edge_index < node.mEdges.size(); edge_index++)
        {
            const auto& prev_edge = node.mEdges[edge_index - 1];
            const auto& curr_edge = node.mEdges[edge_index];

            if (curr_edge.mRenderPassIndex != prev_edge.mRenderPassIndex)
            {
                previous_renderpass_index = prev_edge.mRenderPassIndex;
                uav_barrier_added = false; // for a given resource (GraphNode), we can only ever add 1 UAV barrier per renderpass, so everytime the renderpass changes we reset the flag
            }

            auto& prev_pass = m_RenderPasses[previous_renderpass_index];
            auto& curr_pass = m_RenderPasses[curr_edge.mRenderPassIndex];

            auto& old_state = tracked_state.mSubResourceStates[curr_edge.mSubResource];
            auto& new_state = curr_edge.mUsage;

            // TODO: fix multiple UAV barriers 
            if (( old_state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) && ( new_state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) &&  !uav_barrier_added)
            {
                prev_pass->AddExitBarrier( CD3DX12_RESOURCE_BARRIER::UAV(resource_ptr) );
                uav_barrier_added = true;
            }

            if (old_state == new_state)
                continue;

            assert(curr_edge.mSubResource != prev_edge.mSubResource || curr_edge.mRenderPassIndex != prev_edge.mRenderPassIndex);

            prev_pass->AddExitBarrier( CD3DX12_RESOURCE_BARRIER::Transition(resource_ptr, old_state, new_state, curr_edge.mSubResource) );

            old_state = new_state;
        }

        for (const auto& edge : node.mEdges)
        {
            auto& pass = m_RenderPasses[edge.mRenderPassIndex];
            if (!pass->IsCreated(resource_id))
                continue;

            auto& old_state = tracked_state.mSubResourceStates[edge.mSubResource];
            auto& new_state = edge.mUsage;
            
            if (old_state == new_state)
                continue;

            pass->AddEntryBarrier( CD3DX12_RESOURCE_BARRIER::Transition(resource_ptr, old_state, new_state, edge.mSubResource) );
        }
    }

    auto total_constants_size = 0;
    for (const auto& pass : m_RenderPasses)
        total_constants_size += pass->m_ConstantsSize;

    if (!m_PerPassAllocator.GetBuffer().IsValid())
        m_PerPassAllocator.CreateBuffer(inDevice, std::max(total_constants_size * sFrameCount, 1u));

    if (!m_GlobalConstantsAllocator.GetBuffer().IsValid())
        m_GlobalConstantsAllocator.CreateBuffer(inDevice);
    
    if (!m_PerFrameAllocator.GetBuffer().IsValid())
        m_PerFrameAllocator.CreateBuffer(inDevice, sizeof(FrameConstants) * sFrameCount);

    m_PerFrameAllocatorOffset = 0;

    return true;
}



void RenderGraph::Execute(Device& inDevice, CommandList& inCmdList, uint64_t inFrameCounter)
{
    inDevice.BindDrawDefaults(inCmdList);
    
    inCmdList.BindToSlot(inDevice.GetBuffer(m_GlobalConstantsAllocator.GetBuffer()), EBindSlot::CBV0);
    inCmdList.BindToSlot(inDevice.GetBuffer(m_PerFrameAllocator.GetBuffer()), EBindSlot::SRV0, m_PerFrameAllocatorOffset);
    inCmdList.BindToSlot(inDevice.GetBuffer(m_PerPassAllocator.GetBuffer()), EBindSlot::SRV1);

    for (const auto& renderpass : m_RenderPasses)
    {
        PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), renderpass->GetName().c_str());

        if (renderpass->IsGraphics())
            renderpass->SetRenderTargets(inDevice, m_RenderGraphResources, inCmdList);

        if (inFrameCounter > 0)
            renderpass->FlushBarriers(inDevice, inCmdList, renderpass->m_EntryBarriers);

        renderpass->Execute(m_RenderGraphResources, inCmdList);

        renderpass->FlushBarriers(inDevice, inCmdList, renderpass->m_ExitBarriers);
    }
}



std::string RenderGraph::ToGraphVizText(const Device& inDevice, TextureID inBackBuffer) const
{
    auto ofs = std::stringstream();
    ofs << R"(digraph G {
bgcolor="#181A1B";
rankdir="LR";
overlap = false;
splines = curves;
outputorder="edgesfirst"
graph [pad="0.5", nodesep="1", ranksep="1.5"];
node [margin=.5 fontcolor="#E8E6E3" fontsize=32 width=0 shape=rectangle style=filled, fontname="Arial"]
)";

    std::set<std::string> seen_resources;

    /* Stringify the declaration of every pass and resource. */
    for (const auto& [index, pass] : gEnumerate(m_RenderPasses))
    {
        ofs << '\"' << pass->GetName() << R"("[color="#CC8400"])" << '\n';

        for (const auto& resource_id : pass->m_WrittenResources)
        {
            auto resource_ptr = ( const ID3D12Resource* )nullptr;

            if (m_RenderGraphResources.IsBuffer(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetBufferView(resource_id));

            else if (m_RenderGraphResources.IsTexture(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetTextureView(resource_id));

            assert(resource_ptr);

            auto str_name = gGetDebugName(const_cast<ID3D12Resource*>(resource_ptr));

            if (seen_resources.contains(str_name))
                continue;

            ofs << '\"' << str_name << R"("[color="#1B4958"][group=g)" << std::to_string(index) << "]\n";
            
            seen_resources.emplace(str_name);
        }

        for (const auto& resource_id : pass->m_ReadResources)
        {
            auto resource_ptr = ( const ID3D12Resource* )nullptr;

            if (m_RenderGraphResources.IsBuffer(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetBufferView(resource_id));

            else if (m_RenderGraphResources.IsTexture(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetTextureView(resource_id));

            assert(resource_ptr);

            auto str_name = gGetDebugName(const_cast<ID3D12Resource*>( resource_ptr ));

            if (seen_resources.contains(str_name))
                continue;

            ofs << '\"' << str_name << R"("[color="#1B4958"][group=g)" << std::to_string(index) << "]\n";

            seen_resources.emplace(str_name);
        }

        ofs << '\n';
    }

    seen_resources.clear();

    /* Group written resources. */

    for (const auto& [index, pass] : gEnumerate(m_RenderPasses))
    {
        ofs << "\n{\n" << "rank = same;\n";

        for (const auto& resource_id : pass->m_WrittenResources)
        {
            auto resource_ptr = ( const ID3D12Resource* )nullptr;

            if (m_RenderGraphResources.IsBuffer(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetBufferView(resource_id));

            else if (m_RenderGraphResources.IsTexture(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetTextureView(resource_id));

            assert(resource_ptr);

            auto str_name = gGetDebugName(const_cast<ID3D12Resource*>( resource_ptr ));

            if (seen_resources.contains(str_name))
                continue;

            ofs << '\"' << str_name << "\";\n";

            seen_resources.emplace(str_name);
        }

        ofs << "\n}\n";
    }

    /* Group root passes. */
    ofs << "\n{\n" << "rank = min;\n";
    for (const auto& pass : m_RenderPasses)
    {
        if (pass->m_ReadResources.empty())
            ofs << '\"' << pass->GetName() << "\";\n";
    }
    ofs << "\n}\n";

    /* Stringify the graph logic, basically all the connections e.g "pass" -> "write1" */
    for (const auto& pass : m_RenderPasses)
    {
        for (const auto& resource_id : pass->m_ReadResources)
        {
            auto resource_ptr = ( const ID3D12Resource* )nullptr;

            if (m_RenderGraphResources.IsBuffer(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetBufferView(resource_id));

            else if (m_RenderGraphResources.IsTexture(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetTextureView(resource_id));

            assert(resource_ptr);

            auto str_name = gGetDebugName(const_cast<ID3D12Resource*>( resource_ptr ));

            ofs << '\"' << str_name << "\":e -> \"" << pass->GetName()  << "\":w [color=\"green\"][penwidth=3]\n";
        }

        for (const auto& resource_id : pass->m_WrittenResources)
        {
            auto resource_ptr = ( const ID3D12Resource* )nullptr;

            if (m_RenderGraphResources.IsBuffer(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetBufferView(resource_id));

            else if (m_RenderGraphResources.IsTexture(resource_id))
                resource_ptr = inDevice.GetD3D12Resource(m_RenderGraphResources.GetTextureView(resource_id));

            assert(resource_ptr);

            auto str_name = gGetDebugName(const_cast<ID3D12Resource*>( resource_ptr ));

            ofs << '\"' << pass->GetName() << "\":e -> \"" << str_name << "\":w [color=\"red\"][penwidth=3]\n";
        }

        ofs << '\n';
    }

    ofs << "}";

    return ofs.str();
}


} // raekor

