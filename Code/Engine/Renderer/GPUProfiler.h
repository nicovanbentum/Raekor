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
};

class GPUProfiler
{
public:
    static constexpr int MAX_QUERIES = 2048;

public:
    GPUProfiler(Device& inDevice);
    ~GPUProfiler() = default;

    void Resolve(Device& inDevice, CommandList& inCmdList);
    void Readback(Device& inDevice, uint32_t inFrameIndex);

    bool IsEnabled() const { return g_Profiler->IsEnabled(); }

    int BeginSection(CommandList& inCmdList, const char* inName);
    void EndSection(CommandList& inCmdList, int inIndex);

protected:
    int m_Depth = 0;
    int m_QueryCount = 0;
    uint64_t m_TimestampFrequency = 0;

    BufferID m_TimestampReadbackBuffers[sFrameCount];
    ComPtr<ID3D12QueryHeap> m_TimestampQueryHeaps[sFrameCount];

    Mutex m_SectionsMutex;
    Array<GPUProfileSection> m_GPUSections;
    Array<GPUProfileSection> m_FrameSections[sFrameCount];
    Array<ProfileSection> m_ReadbackSections;
};

extern RK_API GPUProfiler* g_GPUProfiler;


class GPUProfileSectionScoped
{
public:
    GPUProfileSectionScoped(CommandList& inCmdList, const char* inName);
    ~GPUProfileSectionScoped();

private:
    int m_Index = -1;
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
