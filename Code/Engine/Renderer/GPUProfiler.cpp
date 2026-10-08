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
                .size = sizeof(uint64_t) * query_heap_desc.Count,
                .usage = Buffer::READBACK,
                .mappable = true,
                .debugName = "TimestampReadbackBuffer"
            });
    }

    gThrowIfFailed(inDevice.GetGraphicsQueue()->GetTimestampFrequency(&m_TimestampFrequency));
}


void GPUProfiler::Resolve(Device& inDevice, CommandList& inCmdList)
{
    std::scoped_lock lock(m_SectionsMutex);

    const uint32_t frame_index = inCmdList.GetFrameIndex();

    if (m_QueryCount > 0)
    {
        ID3D12Resource* timestamp_resource = inDevice.GetD3D12Resource(m_TimestampReadbackBuffers[frame_index]);
        inCmdList->ResolveQueryData(m_TimestampQueryHeaps[frame_index].Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, m_QueryCount, timestamp_resource, 0);
    }

    m_FrameSections[frame_index] = std::move(m_GPUSections);
    m_GPUSections.clear();

    m_Depth = 0;
    m_QueryCount = 0;
}


void GPUProfiler::Readback(Device& inDevice, uint32_t inFrameIndex)
{
    std::scoped_lock lock(m_SectionsMutex);

    Array<GPUProfileSection>& sections = m_FrameSections[inFrameIndex];

    if (sections.empty())
        return;

    if (g_Profiler->IsEnabled())
    {
        Buffer& buffer = inDevice.GetBuffer(m_TimestampReadbackBuffers[inFrameIndex]);

        uint32_t query_count = 0;
        for (const GPUProfileSection& section : sections)
            query_count = glm::max(query_count, section.mEndQueryIndex + 1);

        const D3D12_RANGE read_range = { 0, query_count * sizeof(uint64_t) };
        const D3D12_RANGE write_range = { 0, 0 };

        uint64_t* timestamps = nullptr;
        gThrowIfFailed(buffer->Map(0, &read_range, (void**)&timestamps));

        uint64_t base_timestamp = UINT64_MAX;
        for (const GPUProfileSection& section : sections)
        {
            if (section.mEndQueryIndex != 0)
                base_timestamp = glm::min(base_timestamp, timestamps[section.mBeginQueryIndex]);
        }

        const double ticks_per_timestamp = double(Timer::sGetTickFrequency()) / double(m_TimestampFrequency);

        m_ReadbackSections.clear();

        for (const GPUProfileSection& section : sections)
        {
            if (section.mEndQueryIndex == 0)
                continue;

            const uint64_t begin_timestamp = timestamps[section.mBeginQueryIndex];
            const uint64_t end_timestamp = glm::max(timestamps[section.mEndQueryIndex], begin_timestamp);

            ProfileSection& readback_section = m_ReadbackSections.emplace_back();
            readback_section.mName = section.mName;
            readback_section.mDepth = section.mDepth;
            readback_section.mStartTick = uint64_t(double(begin_timestamp - base_timestamp) * ticks_per_timestamp);
            readback_section.mEndTick = uint64_t(double(end_timestamp - base_timestamp) * ticks_per_timestamp);
        }

        buffer->Unmap(0, &write_range);

        g_Profiler->SetGPUSections(m_ReadbackSections);
    }

    sections.clear();
}


int GPUProfiler::BeginSection(CommandList& inCmdList, const char* inName)
{
    if (!IsEnabled())
        return -1;

    std::scoped_lock lock(m_SectionsMutex);

    if (m_QueryCount + 2 > MAX_QUERIES)
        return -1;

    const int index = int(m_GPUSections.size());

    GPUProfileSection& section = m_GPUSections.emplace_back();
    section.mName = g_Profiler->InternName(inName);
    section.mDepth = m_Depth++;
    section.mBeginQueryIndex = m_QueryCount++;

    inCmdList->EndQuery(m_TimestampQueryHeaps[inCmdList.GetFrameIndex()].Get(), D3D12_QUERY_TYPE_TIMESTAMP, section.mBeginQueryIndex);

    return index;
}


void GPUProfiler::EndSection(CommandList& inCmdList, int inIndex)
{
    std::scoped_lock lock(m_SectionsMutex);

    if (inIndex < 0 || inIndex >= int(m_GPUSections.size()))
        return;

    GPUProfileSection& section = m_GPUSections[inIndex];
    section.mEndQueryIndex = m_QueryCount++;

    m_Depth = glm::max(m_Depth - 1, 0);

    inCmdList->EndQuery(m_TimestampQueryHeaps[inCmdList.GetFrameIndex()].Get(), D3D12_QUERY_TYPE_TIMESTAMP, section.mEndQueryIndex);
}


GPUProfileSectionScoped::GPUProfileSectionScoped(CommandList& inCmdList, const char* inName) :
    m_CmdList(inCmdList)
{
    PIXBeginEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), inName);

    m_Index = g_GPUProfiler->BeginSection(inCmdList, inName);
}


GPUProfileSectionScoped::~GPUProfileSectionScoped()
{
    g_GPUProfiler->EndSection(m_CmdList, m_Index);

    PIXEndEvent(static_cast<ID3D12GraphicsCommandList*>( m_CmdList ));
}


GPUEventScoped::GPUEventScoped(CommandList& inCmdList, const char* inName, uint32_t inColor) :
    m_CmdList(inCmdList)
{
    inCmdList.PushMarker(inName, inColor);
}


GPUEventScoped::~GPUEventScoped()
{
    m_CmdList.PopMarker();
}

}