#include "pch.h"
#include "DXRenderGraph.h"
#include "shared.h"

namespace Raekor::DX12 {

bool IRenderPass::IsRead(TextureID inTexture) const
{
    for (const auto& texture : m_ReadTextures)
        if (texture.mCreatedTexture == inTexture)
            return true;
    return false;
}



bool IRenderPass::IsWritten(TextureID inTexture) const
{
    for (const auto& texture : m_WrittenTextures)
        if (texture.mCreatedTexture == inTexture)
            return true;
    return false;
}



bool IRenderPass::IsCreated(TextureID inTexture) const
{
    for (const auto& texture : m_CreatedTextures)
        if (texture == inTexture)
            return true;
    return false;
}



RenderGraph::RenderGraph(Device& inDevice, const Viewport& inViewport, uint32_t inFrameCount) :
    m_Viewport(inViewport),
    m_FrameCount(inFrameCount)
{
}



void RenderGraph::Clear(Device& inDevice)
{
    for (auto& pass : m_RenderPasses)
    {
        for (auto resource_id : pass->m_WrittenTextures)
            if (resource_id.mResourceTexture != m_BackBuffer)
                inDevice.ReleaseTexture(resource_id.mResourceTexture);

        for (auto resource_id : pass->m_ReadTextures)
            inDevice.ReleaseTexture(resource_id.mResourceTexture);

        for (auto texture_id : pass->m_CreatedTextures)
            inDevice.ReleaseTexture(texture_id);
    }

    m_RenderPasses.clear();
    m_PerPassAllocator.DestroyBuffer(inDevice);
    m_PerFrameAllocator.DestroyBuffer(inDevice);
}



bool RenderGraph::Compile(Device& inDevice)
{
    assert(m_BackBuffer.IsValid());
    /*
        PASS VALIDATION
        Does not do much at the moment, it validates:
        that we're not doing read_only and read_write for a resource in the same pass.
    */
    for (auto& renderpass : m_RenderPasses)
    {
        for (const auto& resource : renderpass->m_WrittenTextures)
        {
            if (renderpass->IsRead(resource.mCreatedTexture))
            {
                std::cout << std::format("RenderGraph Error: Texture {} is both written to and read from in renderpass {}\n", gGetDebugName(inDevice.GetResourcePtr(resource.mCreatedTexture)), renderpass->GetName());
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
        Texture::Usage mUsage;
        uint32_t mRenderPassIndex;
    };

    struct GraphNode
    {
        std::vector<GraphEdge> mEdges;
    };

    std::unordered_map<TextureID, GraphNode> graph;

    for (const auto& [renderpass_index, renderpass] : gEnumerate(m_RenderPasses))
    {
        for (const auto& resource : renderpass->m_WrittenTextures)
        {
            auto& node = graph[resource.mCreatedTexture];
            const auto usage = inDevice.GetTexture(resource.mResourceTexture).GetDesc().usage;
            node.mEdges.emplace_back(GraphEdge { .mUsage = usage, .mRenderPassIndex = uint32_t(renderpass_index) });
        }

        for (const auto& resource : renderpass->m_ReadTextures)
        {
            auto& node = graph[resource.mCreatedTexture];
            const auto usage = inDevice.GetTexture(resource.mResourceTexture).GetDesc().usage;
            node.mEdges.emplace_back(GraphEdge { .mUsage = usage, .mRenderPassIndex = uint32_t(renderpass_index) });
        }
    }

    /*
        RENDER PASS REMOVAL
    */

    /*std::set<uint32_t> renderpass_indices_to_delete;
    for (const auto& [renderpass_index, renderpass] : gEnumerate(m_RenderPasses)) {
        auto should_delete = true;

        for (const auto& resource : renderpass->m_WrittenTextures) {
            const auto& node = graph[resource.mCreatedTexture];

            for (const auto& edge : node.mEdges) {
                if (edge.mUsage)
            }
        }
    }*/


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

        auto tracked_state = gGetResourceStates(node.mEdges[0].mUsage);

        for (auto edge_index = 1; edge_index < node.mEdges.size(); edge_index++)
        {
            auto new_state = gGetResourceStates(node.mEdges[edge_index].mUsage);
            auto& prev_pass = m_RenderPasses[node.mEdges[edge_index - 1].mRenderPassIndex];

            if (( tracked_state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) && ( new_state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS ))
            {
                prev_pass->AddExitBarrier(ResourceBarrier
                    {
                        .mTexture = resource_id,
                        .mBarrier = CD3DX12_RESOURCE_BARRIER::UAV(inDevice.GetResourcePtr(resource_id))
                    });
            }

            if (tracked_state == new_state)
                continue;

            prev_pass->AddExitBarrier(ResourceBarrier
                {
                    .mTexture = resource_id,
                    .mBarrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(resource_id), tracked_state, new_state)
                });

            tracked_state = new_state;
        }

        for (const auto& edge : node.mEdges)
        {
            auto& pass = m_RenderPasses[edge.mRenderPassIndex];
            if (!pass->IsCreated(resource_id))
                continue;

            auto edge_state = gGetResourceStates(edge.mUsage);
            if (tracked_state == edge_state)
                continue;

            pass->AddEntryBarrier(ResourceBarrier {
                .mTexture = resource_id,
                .mBarrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(resource_id), tracked_state, edge_state)
                });
        }
    }

    auto total_constants_size = 0;
    for (const auto& pass : m_RenderPasses)
        total_constants_size += pass->m_ConstantsSize;

    m_PerPassAllocator.CreateBuffer(inDevice, std::max(total_constants_size * sFrameCount, 1u));
    m_PerFrameAllocator.CreateBuffer(inDevice, sizeof(FrameConstants) * sFrameCount);

    return true;
}



void IRenderPass::FlushBarriers(Device& inDevice, CommandList& inCmdList, const Slice<ResourceBarrier>& inBarriers) const
{
    auto barriers = std::vector<D3D12_RESOURCE_BARRIER>(inBarriers.Length());

    for (const auto& [index, barrier] : gEnumerate(inBarriers))
        barriers[index] = barrier.mBarrier;

    if (!barriers.empty())
        inCmdList->ResourceBarrier(barriers.size(), barriers.data());
}



void IRenderPass::SetRenderTargets(Device& inDevice, CommandList& inCmdList) const
{
    auto& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto& dsv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // depth stencil attachment is invalid until a target texture (descriptor) is found and assigned
    auto dsv_descriptor = DescriptorID(DescriptorID::INVALID);

    auto rtv_handle_count = 0u;
    static auto rtv_handles = std::array<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT>{};

    for (const auto resource : m_WrittenTextures)
    {
        const auto& texture = inDevice.GetTexture(resource.mResourceTexture);

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



void RenderGraph::Execute(Device& inDevice, CommandList& inCmdList, uint64_t inFrameCounter)
{
    inDevice.BindDrawDefaults(inCmdList);
    inCmdList.BindToSlot(inDevice.GetBuffer(m_PerFrameAllocator.GetBuffer()), EBindSlot::SRV0, m_PerFrameAllocatorOffset);
    inCmdList.BindToSlot(inDevice.GetBuffer(m_PerPassAllocator.GetBuffer()), EBindSlot::SRV1);

    for (const auto& renderpass : m_RenderPasses)
    {
        PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), renderpass->GetName().c_str());

        if (renderpass->IsGraphics())
            renderpass->SetRenderTargets(inDevice, inCmdList);

        if (inFrameCounter > 0)
            renderpass->FlushBarriers(inDevice, inCmdList, renderpass->m_EntryBarriers);

        renderpass->Execute(inCmdList);

        renderpass->FlushBarriers(inDevice, inCmdList, renderpass->m_ExitBarriers);
    }
}



void RenderGraph::SetBackBuffer(TextureID inTexture)
{
    for (auto& renderpass : m_RenderPasses)
        for (auto& resource : renderpass->m_WrittenTextures)
            if (resource.mCreatedTexture == m_BackBuffer && resource.mResourceTexture == m_BackBuffer)
                resource = TextureResource { .mCreatedTexture = inTexture, .mResourceTexture = inTexture };

    m_BackBuffer = inTexture;
}



std::string RenderGraph::ToGraphVizText(const Device& inDevice) const
{
    auto ofs = std::stringstream();
    ofs << R"(digraph G {
bgcolor="#181A1B"
overlap = false;
splines = ortho;
outputorder="edgesfirst"
graph [pad="0.5", nodesep="1", ranksep="1.5"];
node [margin=.5 fontcolor="#E8E6E3" fontsize=32 width=0 shape=rectangle style=filled, fontname="Arial"]
)";

    /* Stringify the declaration of every pass and resource. */
    for (const auto& [index, pass] : gEnumerate(m_RenderPasses))
    {
        ofs << '\"' << pass->GetName() << R"("[color="#CC8400"])" << '\n';

        for (const auto& resource : pass->m_WrittenTextures)
        {
            wchar_t name[128] = {};
            UINT size = sizeof(name);
            inDevice.GetTexture(resource.mCreatedTexture).GetResource()->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
            auto str_name = gWCharToString(name);

            if (resource.mCreatedTexture == GetBackBuffer())
                ofs << '\"' << str_name << R"("[color="red"][group=g)" << std::to_string(index) << "]\n";
            else
                ofs << '\"' << str_name << R"("[color="#1B4958"][group=g)" << std::to_string(index) << "]\n";
        }

        for (const auto& resource : pass->m_WrittenTextures)
        {
            wchar_t name[128] = {};
            UINT size = sizeof(name);
            inDevice.GetTexture(resource.mCreatedTexture).GetResource()->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
            auto str_name = gWCharToString(name);

            if (resource.mCreatedTexture == GetBackBuffer())
                ofs << '\"' << str_name << R"("[color="red"][group=g)" << std::to_string(index) << "]\n";
            else
                ofs << '\"' << str_name << R"("[color="#1B4958"][group=g)" << std::to_string(index) << "]\n";
        }
    }

    /* Stringify the graph logic, basically all the connections e.g "pass" -> "write1" */
    for (const auto& pass : m_RenderPasses)
    {
        for (const auto& resource : pass->m_WrittenTextures)
            ofs << '\"' << pass->GetName() << "\" -> \"" << gGetDebugName(inDevice.GetTexture(resource.mCreatedTexture).GetResource().Get()) << "\" [color=\"red\"][penwidth=3]\n";

        for (const auto& resource : pass->m_WrittenBuffers)
            ofs << '\"' << pass->GetName() << "\" -> \"" << gGetDebugName(inDevice.GetBuffer(resource.mCreatedBuffer).GetResource().Get()) << "\" [color=\"red\"][penwidth=3]\n";

        for (const auto& resource : pass->m_ReadTextures)
            ofs << '\"' << gGetDebugName(inDevice.GetTexture(resource.mCreatedTexture).GetResource().Get()) << "\" -> \"" << pass->GetName() << "\" [color=\"green\"][penwidth=3]\n";

        for (const auto& resource : pass->m_ReadBuffers)
            ofs << '\"' << gGetDebugName(inDevice.GetBuffer(resource.mCreatedBuffer).GetResource().Get()) << "\" -> \"" << pass->GetName() << "\" [color=\"green\"][penwidth=3]\n";
    }

    /* Stringify final groupings, e.g. "pass" -> {"write1" "write2" "write3" } */
    for (const auto& [index, pass] : gEnumerate(m_RenderPasses))
    {
        if (pass->m_WrittenTextures.size() < 2 || pass->m_WrittenBuffers.size() < 2)
            continue;

        ofs << '\"' << pass->GetName() << R"(" -> {")";

        for (const auto& [index, resource] : gEnumerate(pass->m_WrittenTextures))
        {
            wchar_t name[128] = {};
            UINT size = sizeof(name);
            inDevice.GetTexture(resource.mCreatedTexture).GetResource()->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
            auto str_name = gWCharToString(name);

            ofs << gWCharToString(name);

            if (index != pass->m_WrittenTextures.size() - 1)
                ofs << R"(" ")";
        }

        for (const auto& [index, resource] : gEnumerate(pass->m_WrittenBuffers))
        {
            wchar_t name[128] = {};
            UINT size = sizeof(name);
            inDevice.GetBuffer(resource.mCreatedBuffer).GetResource()->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
            auto str_name = gWCharToString(name);

            ofs << gWCharToString(name);

            if (index != pass->m_WrittenBuffers.size() - 1)
                ofs << R"(" ")";
        }

        ofs << R"("}[style=invis])" << '\n';
    }

    ofs << "}";

    return ofs.str();
}


} // raekor

