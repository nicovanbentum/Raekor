#pragma once

#include "Device.h"
#include "Defines.h"
#include "Resource.h"

namespace RK::DX12 {

class RenderWorld;
struct RenderSkinnedMesh;


struct BottomLevelASBuild
{
    BufferID mBottomLevelAS;
    BufferID mIndexBuffer;
    BufferID mVertexBuffer;
    uint32_t mIndexCount = 0;
    uint32_t mVertexCount = 0;
    bool     mAllowUpdate = false;
};


class GPUScene
{
public:
    NO_COPY_NO_MOVE(GPUScene);

    GPUScene() = default;

    bool HasTLAS() const { return m_TLASBuffer.IsValid() && m_TLASInstanceCount > 0; }
    bool HasLights() const { return m_LightsBuffer.IsValid() && m_LightsCount > 0; }

    uint32_t GetLightsCount() const { return m_LightsCount; }

    uint32_t GetTLASDescriptorIndex() const { return m_TLASDescriptor.GetIndex(); }
    uint32_t GetEmptyTLASDescriptorIndex() const { return m_EmptyTLASDescriptor.GetIndex(); }

    uint32_t GetLightsDescriptorIndex() const { return m_LightsDescriptor.GetIndex(); }
    uint32_t GetInstancesDescriptorIndex() const { return m_InstancesDescriptor.GetIndex(); }
    uint32_t GetMaterialsDescriptorIndex() const { return m_MaterialsDescriptor.GetIndex(); }

    BufferID CreateBottomLevelAS(Device& inDevice, const BottomLevelASBuild& inBuild);
    void ReleaseBottomLevelAS(Device& inDevice, BufferID inBottomLevelAS);

    static uint64_t sGetBottomLevelASUpdateScratchSize(Device& inDevice, uint32_t inIndexCount, uint32_t inVertexCount);

    void Upload(Device& inDevice, CommandList& inCmdList, const RenderWorld& inWorld);
    void RefitBottomLevelAS(Device& inDevice, CommandList& inCmdList, const RenderSkinnedMesh& inSkinnedMesh);
    void BuildTLAS(Device& inDevice, CommandList& inCmdList);

private:
    void BuildPendingBottomLevelAS(Device& inDevice, CommandList& inCmdList);
    void UploadTLASInstances(Device& inDevice, CommandList& inCmdList, const RenderWorld& inWorld);

    BufferID GrowBuffer(Device& inDevice, BufferID inBuffer, const Buffer::Desc& inDesc);

private:
    Mutex m_PendingBuildsMutex;
    Array<BottomLevelASBuild> m_PendingBuilds;

    BufferID m_TLASBuffer;
    BufferID m_EmptyTLASBuffer;

    BufferID m_ScratchBuffer;
    BufferID m_EmptyScratchBuffer;

    DescriptorID m_TLASDescriptor;
    DescriptorID m_EmptyTLASDescriptor;

    BufferID m_LightsBuffer;
    BufferID m_InstancesBuffer;
    BufferID m_MaterialsBuffer;
    BufferID m_D3D12InstancesBuffer;
    uint32_t m_TLASInstanceCount = 0;
    uint32_t m_LightsCount = 0;

    DescriptorID m_LightsDescriptor;
    DescriptorID m_InstancesDescriptor;
    DescriptorID m_MaterialsDescriptor;
};

} // namespace RK::DX12
