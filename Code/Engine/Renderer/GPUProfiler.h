#pragma once

#include "Profiler.h"
#include "Resource.h"
#include "CommandList.h"

namespace RK::DX12 {

#define EVENT_SCOPE_GPU(cmdlist, name) GPUEventScoped TOKENPASTE2(gpu_event_, __LINE__)(cmdlist, name, 0)

#define PROFILE_SCOPE_GPU(cmdlist, name) GPUProfileSectionScoped TOKENPASTE2(gpu_profile_section_, __LINE__)(cmdlist, name)

#define PROFILE_FUNCTION_GPU(cmdlist) GPUProfileSectionScoped TOKENPASTE2(gpu_profile_section_, __LINE__)(cmdlist, __FUNCTION__)


struct GPUProfileSection : public ProfileSection
{
    uint32_t mBeginQueryIndex = 0;
    uint32_t mEndQueryIndex = 0;

    float GetSeconds() const final { return Timer::sGetTicksToSeconds(mEndTick - mStartTick); }
};

class GPUProfiler
{
public:
    static constexpr int MAX_QUERIES = 2048;

public:
    friend class GPUProfileSection;
    friend class GPUProfileSectionScoped;

    GPUProfiler(Device& inDevice);
    ~GPUProfiler() = default;

    void Reset(Device& inDevice);
    void Resolve(Device& inDevice, CommandList& inCmdList);
    void Readback(Device& inDevice, uint32_t inFrameIndex);

    bool IsEnabled() const { return m_IsEnabled; }
    void SetEnabled(bool inEnabled) { m_IsEnabled = inEnabled; }

    void BeginQuery(GPUProfileSection& inSection, CommandList& inCmdList);
    void EndQuery(GPUProfileSection& inSection, CommandList& inCmdList);

    int AllocateGPU();
    GPUProfileSection& GetSectionGPU(int inIndex) { return m_GPUSections[inIndex]; }
    const Array<GPUProfileSection>& GetGPUProfileSections() const { return m_GPUReadbackSections[m_ReadbackIndex]; }

protected:
    int m_Depth = 0;
    int m_QueryCount = 0;
    int m_ReadbackIndex = 0;
    bool m_IsEnabled = true;

    uint32_t m_TimestampCount = 0;
    ComPtr<ID3D12Fence> m_TimestampFence = nullptr;
    BufferID m_TimestampReadbackBuffers[sFrameCount];
    ComPtr<ID3D12QueryHeap> m_TimestampQueryHeaps[sFrameCount];

    Mutex m_SectionsMutex;
    Array<GPUProfileSection> m_GPUSections;
    Array<GPUProfileSection> m_HistoryGPUSections[sFrameCount];
    Array<GPUProfileSection> m_GPUReadbackSections[sFrameCount];
};

extern GPUProfiler* g_GPUProfiler;


class GPUProfileSectionScoped
{
public:
    GPUProfileSectionScoped(CommandList& inCmdList, const char* inName);
    ~GPUProfileSectionScoped();

private:
    int m_Index = 0;
    CommandList& m_CmdList;
};


class GPUEventScoped
{
public:
    GPUEventScoped(CommandList& inCmdList, const char* inName, uint32_t inColor);
    ~GPUEventScoped();

private:
    CommandList& m_CmdList;
};

}