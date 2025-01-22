#include "PCH.h"
#include "GPUProfiler.h"
#include "CommandList.h"
#include "Device.h"
#include "Iter.h"
#include "Hash.h"

namespace RK::DX12 {

GPUProfiler* g_GPUProfiler = nullptr;


GPUProfiler::GPUProfiler(Device& inDevice)
{
	for (int i = 0; i < sFrameCount; i++)
	{
		D3D12_QUERY_HEAP_DESC query_heap_desc = {};
		query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		query_heap_desc.Count = MAX_QUERIES;

		inDevice->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(m_TimestampQueryHeaps[i].GetAddressOf()));

		m_TimestampReadbackBuffers[i] = inDevice.CreateBuffer(Buffer::Desc
		{
			.size      = sizeof(uint64_t) * query_heap_desc.Count,
			.usage     = Buffer::READBACK,
			.mappable  = true,
			.debugName = "TimestampReadbackBuffer"
		});
	}

	inDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_TimestampFence.GetAddressOf()));
	m_TimestampFence->SetName(L"TimestampFence");
}

void GPUProfiler::Reset(Device& inDevice)
{
	if (m_IsEnabled)
	{
		m_QueryCount = 0;
		std::scoped_lock lock(m_SectionsMutex);
		m_HistoryGPUSections[inDevice.GetFrameIndex()] = m_GPUSections;
		m_GPUSections.clear();
	}
}


void GPUProfiler::Resolve(Device& inDevice, CommandList& inCmdList)
{
	if (!m_IsEnabled)
		return;

	if (m_QueryCount == 0)
		return;

	BufferID timestamp_buffer = m_TimestampReadbackBuffers[inCmdList.GetFrameIndex()];
	ComPtr<ID3D12QueryHeap> timestamp_heap = m_TimestampQueryHeaps[inCmdList.GetFrameIndex()];

	ID3D12Resource* timestamp_resource = inDevice.GetD3D12Resource(timestamp_buffer);

	inCmdList->ResolveQueryData(timestamp_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, m_QueryCount, timestamp_resource, 0);
	inDevice.GetGraphicsQueue()->Signal(m_TimestampFence.Get(), inDevice.GetFrameCounter());
}


void GPUProfiler::Readback(Device& inDevice, uint32_t inFrameIndex)
{
	if (!m_IsEnabled)
		return;

	uint64_t fence_value = m_TimestampFence->GetCompletedValue();
	uint64_t frame_value = inDevice.GetFrameCounter() - sFrameCount;

	if (fence_value >= frame_value)
	{
		Buffer& buffer = inDevice.GetBuffer(m_TimestampReadbackBuffers[inFrameIndex]);
		m_GPUReadbackSections[inFrameIndex] = m_HistoryGPUSections[inFrameIndex];
		Array<GPUProfileSection>& sections = m_GPUReadbackSections[inFrameIndex];

		uint64_t* timestamps;
		buffer->Map(0, nullptr, (void**)&timestamps);

		for (GPUProfileSection& section : sections)
		{
			section.mStartTick = timestamps[section.mBeginQueryIndex];
			section.mEndTick = timestamps[section.mEndQueryIndex];
		}

		m_ReadbackIndex = inFrameIndex;
	}
}


void GPUProfiler::BeginQuery(GPUProfileSection& inSection, CommandList& inCmdList)
{
	inSection.mBeginQueryIndex = m_QueryCount++;
	inCmdList->EndQuery(m_TimestampQueryHeaps[inCmdList.GetFrameIndex()].Get(), D3D12_QUERY_TYPE_TIMESTAMP, inSection.mBeginQueryIndex);
}


void GPUProfiler::EndQuery(GPUProfileSection& inSection, CommandList& inCmdList)
{
	inSection.mEndQueryIndex = m_QueryCount++;
	inCmdList->EndQuery(m_TimestampQueryHeaps[inCmdList.GetFrameIndex()].Get(), D3D12_QUERY_TYPE_TIMESTAMP, inSection.mEndQueryIndex);
}


int GPUProfiler::AllocateGPU()
{
	std::scoped_lock lock(m_SectionsMutex);

	int index = m_GPUSections.size();
	m_GPUSections.emplace_back();
	
	assert(index < MAX_QUERIES);
	return index;
}


GPUProfileSectionScoped::GPUProfileSectionScoped(CommandList& inCmdList, const char* inName) :
	mCmdList(inCmdList)
{
	if (g_GPUProfiler->IsEnabled())
	{
		PIXBeginEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), inName);

		mIndex = g_GPUProfiler->AllocateGPU();
		GPUProfileSection& section = g_GPUProfiler->GetSectionGPU(mIndex);
		
		section.mName = inName;
		section.mDepth = g_GPUProfiler->m_Depth++;
		g_GPUProfiler->BeginQuery(section, inCmdList);
	}
}


GPUProfileSectionScoped::~GPUProfileSectionScoped()
{
	if (g_GPUProfiler->IsEnabled())
	{
		PIXEndEvent(static_cast<ID3D12GraphicsCommandList*>( mCmdList ));
		
		g_GPUProfiler->m_Depth--;
		g_GPUProfiler->EndQuery(g_GPUProfiler->GetSectionGPU(mIndex), mCmdList);
	}
}

}