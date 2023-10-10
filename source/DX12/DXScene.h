#pragma once

#include "DXDevice.h"
#include "DXResource.h"
#include "Raekor/util.h"
#include "Raekor/scene.h"

namespace Raekor {

class Application;

}

namespace Raekor::DX12 {

/* 
    Since I don't feel like changing Scene to be some base pointer to a derived RayTracedScene class and having to pass pointers everywhere, 
    this class is a single instance type just like Scene. It holds a reference to the Editor's scene, and stores additional data for ray tracing and handles mesh/material uploads.
*/
class RayTracedScene
{
private:
    struct Settings
    {
        bool mDisableAlbedo = false;
    } m_Settings;

public:
    NO_COPY_NO_MOVE(RayTracedScene);

    explicit RayTracedScene(Scene& inScene) : m_Scene(inScene) {}

    inline operator Scene& ( )              { return m_Scene; }
    inline operator const Scene& ( ) const  { return m_Scene; }
    Scene* operator-> ()                    { return &m_Scene; }
    const Scene* operator-> () const        { return &m_Scene; }

    inline Settings& GetSettings() { return m_Settings; }
    inline const Settings& GetSettings() const { return m_Settings; }

    DescriptorID GetTLASDescriptor(Device& inDevice) const { return inDevice.GetBuffer(m_TLASBuffer).GetDescriptor(); }
    DescriptorID GetInstancesDescriptor(Device& inDevice) const { return inDevice.GetBuffer(m_InstancesBuffer).GetDescriptor(); }
    DescriptorID GetMaterialsDescriptor(Device& inDevice) const { return inDevice.GetBuffer(m_MaterialsBuffer).GetDescriptor(); }

    void UploadMesh(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, Mesh& inMesh, CommandList& inCmdList);
    void UploadTexture(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, TextureUpload& inUpload, CommandList& inCmdList);
    void UploadMaterial(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, Material& inMaterial, CommandList& inCmdList);

    void UploadTLAS(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, CommandList& inCmdList);
    void UploadInstances(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, CommandList& inCmdList);
    void UploadMaterials(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, CommandList& inCmdList);

    uint32_t GetMaterialIndex(Entity inEntity);

private:
    BufferID GrowBuffer(Device& inDevice, BufferID inBuffer, const Buffer::Desc& inDesc);

private:
    Scene& m_Scene;
    BufferID m_TLASBuffer;
    BufferID m_ScratchBuffer;
    BufferID m_MaterialsBuffer;
    BufferID m_InstancesBuffer;
    BufferID m_D3D12InstancesBuffer;
};

}