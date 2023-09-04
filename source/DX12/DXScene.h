#pragma once

#include "DXDevice.h"
#include "DXResource.h"
#include "Raekor/util.h"
#include "Raekor/scene.h"

namespace Raekor {

class Application;

}

namespace Raekor::DX12 {

class RayTracedScene
{
public:
    NO_COPY_NO_MOVE(RayTracedScene);

    explicit RayTracedScene(Scene& inScene) : m_Scene(inScene) {}

    inline operator Scene& ( )              { return m_Scene; }
    inline operator const Scene& ( ) const  { return m_Scene; }
    Scene* operator-> ()                    { return &m_Scene; }
    const Scene* operator-> () const        { return &m_Scene; }

    DescriptorID GetTLASDescriptor(Device& inDevice) const { return inDevice.GetBuffer(m_TLASBuffer).GetDescriptor(); }
    DescriptorID GetInstancesDescriptor(Device& inDevice) const { return inDevice.GetBuffer(m_InstancesBuffer).GetDescriptor(); }
    DescriptorID GetMaterialsDescriptor(Device& inDevice) const { return inDevice.GetBuffer(m_MaterialsBuffer).GetDescriptor(); }

    void UploadTLAS(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, CommandList& inCmdList);
    void UploadInstances(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, CommandList& inCmdList);
    void UploadMaterials(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, CommandList& inCmdList);

    uint32_t GetMaterialIndex(Entity inEntity);

private:
    Scene& m_Scene;
    BufferID m_TLASBuffer;
    BufferID m_InstancesBuffer;
    BufferID m_MaterialsBuffer;
};

}