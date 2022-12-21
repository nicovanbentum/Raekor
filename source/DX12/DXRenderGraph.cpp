#include "pch.h"
#include "DXRenderGraph.h"
#include "shared.h"

namespace Raekor::DX {

bool IRenderPass::IsRead(TextureID inTexture) {
	for (const auto& texture : m_ReadTextures)
		if (texture.mCreatedTexture == inTexture)
			return true;
	return false;
}


bool IRenderPass::IsWritten(TextureID inTexture) {
	for (const auto& texture : m_WrittenTextures)
		if (texture.mCreatedTexture == inTexture)
			return true;
	return false;
}


bool IRenderPass::IsCreated(TextureID inTexture) {
	for (const auto& texture : m_CreatedTextures)
		if (texture == inTexture)
			return true;
	return false;
}


RenderGraph::RenderGraph(Device& inDevice, const Viewport& inViewport) : 
	m_Viewport(inViewport)
{}


void RenderGraph::Clear(Device& inDevice) {
	for (auto& pass : m_RenderPasses) {
		for (auto resource_id : pass->m_WrittenTextures)
			inDevice.ReleaseTexture(resource_id.mResourceTexture);
		for (auto resource_id : pass->m_ReadTextures)
			inDevice.ReleaseTexture(resource_id.mResourceTexture);
		for (auto texture_id : pass->m_CreatedTextures)
			inDevice.ReleaseTexture(texture_id);
	}

	m_RenderPasses.clear();
}


bool RenderGraph::Compile(Device& inDevice) {
	assert(m_BackBuffer.Isvalid());

	////////////////////////
	// GRAPH VALIDATION
	// Does not do much at the moment, it validates: 
	// that we're not doing read_only and read_write for a resource in the same pass.
	// that we're not creating and reading from a resource in the same pass (I can't think of a use case in favor of allowing it).
	////////////////////////
	for (auto& renderpass : m_RenderPasses) {
		for (const auto& resource : renderpass->m_WrittenTextures) {
			if (renderpass->IsRead(resource.mCreatedTexture)) {
				std::cout << std::format("RenderGraph Error: Texture {} is both written to and read from in renderpass {}\n", resource.mCreatedTexture.ToIndex(), renderpass->GetName());
				return false;
			}
		}

		for (const auto& resource : renderpass->m_ReadTextures) {
			if (renderpass->IsCreated(resource.mCreatedTexture)) {
				std::cout << std::format("RenderGraph Error: Texture {} is both created and read from in renderpass {}\n", resource.mCreatedTexture.ToIndex(), renderpass->GetName());
				return false;
			}
		}
	}

	////////////////////////
	// GRAPH CONSTRUCTION
	// The render graph is structured as follows:
	// resources like textures and buffers are vertices
	// Edges go from resources to render passes (which aren't actually vertices, so fake graph theory!)
	// an edge also describes the state the resource should be in during the corresponding render pass
	// example:
	//						 WRITE
	// GBUFFER_DEPTH_TEXTURE ----> GBUFFER RENDER PASS
	// 
	//						 READ
	// GBUFFER_DEPTH_TEXTURE ----> SHADOW RENDER PASS
	// 
	// The order is guaranteed to follow the graph from start to finish as we linearly loop through it, there's no pass re-ordering.
	////////////////////////
	struct GraphEdge {
		Texture::Usage mUsage;
		uint32_t mRenderPassIndex;
	};

	struct GraphNode {
		std::vector<GraphEdge> mEdges;
	};

	std::unordered_map<TextureID, GraphNode> graph;

	for (const auto& [renderpass_index, renderpass] : gEnumerate(m_RenderPasses)) {
		for (const auto& resource : renderpass->m_WrittenTextures) {
			auto& node = graph[resource.mCreatedTexture];
			const auto usage = inDevice.GetTexture(resource.mResourceTexture).GetDesc().usage;
			node.mEdges.emplace_back(GraphEdge{ .mUsage = usage, .mRenderPassIndex = uint32_t(renderpass_index) });
		}

		for (const auto& resource : renderpass->m_ReadTextures) {
			auto& node = graph[resource.mCreatedTexture];
			const auto usage = inDevice.GetTexture(resource.mResourceTexture).GetDesc().usage;
			node.mEdges.emplace_back(GraphEdge{ .mUsage = usage, .mRenderPassIndex = uint32_t(renderpass_index) });
		}
	}

	////////////////////////
	// BARRIER GENERATION
	// The state tracking algorithm here is quite dumb:
	// 
	// Go through all the resources in the graph.
	// if a resource has 1 edge the initial state should match the edge state, no barriers needed. TODO: add validation for single edge case.
	// else loop linearly through all the edges, if state changes look up the previous edge's render pass and add an end-of render pass barrier.
	// at the end of the loop we're left with the resource's final state for the graph
	// if final state differs from the state in which it was created, add a begin-of render pass barrier (which are skipped the first frame).
	// 
	// TODO: Ideally we refactor this to use aliasing as that would reset the state of the resource, so we wouldn't need the begin-of render pass barriers.
	////////////////////////

	for (const auto& [resource_id, node] : graph) {
		if (node.mEdges.size() < 2)
			continue;

		auto tracked_state = gGetResourceStates(node.mEdges[0].mUsage);

		for (auto edge_index = 1; edge_index < node.mEdges.size(); edge_index++) {
			auto new_state = gGetResourceStates(node.mEdges[edge_index].mUsage);

			if (tracked_state == new_state)
				continue;

			auto& prev_pass = m_RenderPasses[node.mEdges[edge_index - 1].mRenderPassIndex];

			prev_pass->AddExitBarrier(ResourceBarrier {
				.mTexture = resource_id,
				.mDebugName = GetDebugName(inDevice.GetResourcePtr(resource_id)),
				.mBarrier = D3D12_RESOURCE_BARRIER {
					.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
					.Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
						.pResource = inDevice.GetResourcePtr(resource_id),
						.StateBefore = tracked_state,
						.StateAfter = new_state
					}
				}
			});

			tracked_state = new_state;
		}

		for (const auto& edge : node.mEdges) {
			auto& pass = m_RenderPasses[edge.mRenderPassIndex];
			if (!pass->IsCreated(resource_id))
				continue;

			auto edge_state = gGetResourceStates(edge.mUsage);
			if (tracked_state == edge_state)
				continue;

			pass->AddEntryBarrier(ResourceBarrier {
				.mTexture = resource_id,
				.mDebugName = GetDebugName(inDevice.GetResourcePtr(resource_id)),
				.mBarrier = D3D12_RESOURCE_BARRIER {
					.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
					.Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
						.pResource = inDevice.GetResourcePtr(resource_id),
						.StateBefore = tracked_state,
						.StateAfter = edge_state
					}
				}
			});
		}
	}

	return true;
}
	


void IRenderPass::FlushBarriers(Device& inDevice, CommandList& inCmdList, const Slice<ResourceBarrier>& inBarriers) const {
	auto barriers = std::vector<D3D12_RESOURCE_BARRIER>(inBarriers.Length());
	
	for (const auto& [index, barrier] : gEnumerate(inBarriers))
		barriers[index] = barrier.mBarrier;

	if(!barriers.empty())
		inCmdList->ResourceBarrier(barriers.size(), barriers.data());
}



void IRenderPass::SetRenderTargets(Device& inDevice, CommandList& inCmdList) const {
	auto& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto& dsv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// depth stencil attachment is invalid until a target texture (descriptor) is found and assigned
	auto dsv_descriptor = DescriptorID(DescriptorID::INVALID); 
	
	auto rtv_handle_count = 0u;
	static auto rtv_handles = std::array<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT>{};

	for (const auto resource : m_WrittenTextures) {
		const auto& texture = inDevice.GetTexture(resource.mResourceTexture);

		switch (texture.GetDesc().usage) {
			case Texture::RENDER_TARGET: {
				rtv_handles[rtv_handle_count++] = rtv_heap.GetCPUDescriptorHandle(texture.GetView());
			} break;
			case Texture::DEPTH_STENCIL_TARGET: {
				assert(!dsv_descriptor.Isvalid()); // if you define multiple depth targets for a renderpass you're going to have a bad time, mmkay
				dsv_descriptor = texture.GetView();
			} break;
		}
	}

	if (dsv_descriptor.Isvalid()) {
		auto dsv_handle = dsv_heap.GetCPUDescriptorHandle(dsv_descriptor);
		inCmdList->OMSetRenderTargets(rtv_handle_count, rtv_handles.data(), FALSE, &dsv_handle);
	}
	else
		inCmdList->OMSetRenderTargets(rtv_handle_count, rtv_handles.data(), FALSE, nullptr);
}


	
void RenderGraph::Execute(Device& inDevice, CommandList& inCmdList, bool isFirstFrame) {
	inDevice.BindDrawDefaults(inCmdList);

	for (const auto& renderpass : m_RenderPasses) {
		PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>(inCmdList), PIX_COLOR(0, 255, 0), renderpass->GetName().c_str());

		if (renderpass->GetRenderPassType() == RenderPassType::GRAPHICS_PASS)
			renderpass->SetRenderTargets(inDevice, inCmdList);

		if (!isFirstFrame)
			renderpass->FlushBarriers(inDevice, inCmdList, renderpass->m_EntryBarriers);

		renderpass->Execute(inCmdList);

		renderpass->FlushBarriers(inDevice, inCmdList, renderpass->m_ExitBarriers);
	}
}



void RenderGraph::SetBackBuffer(TextureID inTexture) {
	for (auto& renderpass : m_RenderPasses)
		for (auto& resource : renderpass->m_WrittenTextures)
			if (resource.mCreatedTexture == m_BackBuffer && resource.mResourceTexture == m_BackBuffer)
				resource = TextureResource{ .mCreatedTexture = inTexture, .mResourceTexture = inTexture };

	m_BackBuffer = inTexture;
}



std::string RenderGraph::GetGraphViz(const Device& inDevice) const {
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
	for (const auto& [index, pass] : gEnumerate(m_RenderPasses)) {
		ofs << '\"' << pass->GetName() << R"("[color="#CC8400"])" << '\n';

		for (const auto& resource : pass->m_WrittenTextures) {
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
	for (const auto& pass : m_RenderPasses) {
		for (const auto& resource : pass->m_WrittenTextures)
			ofs << '\"' << pass->GetName() << "\" -> \"" << GetDebugName(inDevice.GetTexture(resource.mCreatedTexture).GetResource().Get()) << "\" [color=\"red\"][penwidth=3]\n";

		for (const auto& resource : pass->m_ReadTextures)
			ofs << '\"' << GetDebugName(inDevice.GetTexture(resource.mCreatedTexture).GetResource().Get()) << "\" -> \"" << pass->GetName() << "\" [color=\"green\"][penwidth=3]\n";
	}

	/* Stringify final groupings, e.g. "pass" -> {"write1" "write2" "write3" } */
	for (const auto& [index, pass] : gEnumerate(m_RenderPasses)) {
		if (pass->m_WrittenTextures.size() < 2)
			continue;

		ofs << '\"' << pass->GetName() << R"(" -> {")";

		for (const auto& [index, resource] : gEnumerate(pass->m_WrittenTextures)) {
			wchar_t name[128] = {};
			UINT size = sizeof(name);
			inDevice.GetTexture(resource.mCreatedTexture).GetResource()->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
			auto str_name = gWCharToString(name);

			ofs << gWCharToString(name);

			if (index != pass->m_WrittenTextures.size() - 1)
				ofs << R"(" ")";
		}

		ofs << R"("}[style=invis])" << '\n';
	}

	ofs << "}";

	return ofs.str();
}


} // raekor

