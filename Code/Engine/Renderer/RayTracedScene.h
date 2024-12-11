#pragma once

#include "Scene.h"
#include "Device.h"
#include "Defines.h"
#include "Resource.h"
#include "Components.h"

namespace RK {

class Application;

}

namespace RK::DX12 {

/* 
    Since I don't feel like changing Scene to be some base pointer to a derived RayTracedScene class and having to pass pointers everywhere, 
    this class is a single instance type just like Scene. It holds a reference to the Editor's scene, and stores additional data for ray tracing and handles mesh/material uploads.
*/
class RayTracedScene
{
public:
    NO_COPY_NO_MOVE(RayTracedScene);

    explicit RayTracedScene(Scene& inScene) : m_Scene(inScene) {}

    inline operator Scene& ( )              { return m_Scene; }
    inline operator const Scene& ( ) const  { return m_Scene; }
    Scene* operator-> ()                    { return &m_Scene; }
    const Scene* operator-> () const        { return &m_Scene; }

    bool HasTLAS() const { return m_TLASBuffer.IsValid() && GetInstancesCount() > 0; }
    bool HasLights() const { return m_LightsBuffer.IsValid() && GetLightsCount() > 0; }
    bool HasInstances() const { return m_InstancesBuffer.IsValid() && GetInstancesCount() > 0; }
    bool HasMaterials() const { return m_MaterialsBuffer.IsValid() && GetMaterialsCount() > 0; }

    uint32_t GetLightsCount() const { return m_Scene.Count<Light>(); }
    uint32_t GetInstancesCount() const { return m_Scene.Count<Mesh>(); }
    uint32_t GetMaterialsCount() const { return m_Scene.Count<Material>(); }

    uint32_t GetTLASDescriptorIndex() const { return m_TLASDescriptor.GetIndex(); }
    uint32_t GetLightsDescriptorIndex() const { return m_LightsDescriptor.GetIndex(); }
    uint32_t GetInstancesDescriptorIndex() const { return m_InstancesDescriptor.GetIndex(); }
    uint32_t GetMaterialsDescriptorIndex() const { return m_MaterialsDescriptor.GetIndex(); }

    void UploadMesh(Application* inApp, Device& inDevice, Mesh& inMesh, Skeleton* inSkeleton, CommandList& inCmdList);
    void UpdateBLAS(Application* inApp, Device& inDevice, Mesh& inMesh, Skeleton* inSkeleton, CommandList& inCmdList);
    void UploadTexture(Application* inApp, Device& inDevice, TextureUpload& inUpload, CommandList& inCmdList);
    void UploadSkeleton(Application* inApp, Device& inDevice, Skeleton& inSkeleton, CommandList& inCmdList);
    void UploadMaterial(Application* inApp, Device& inDevice, Material& inMaterial, CommandList& inCmdList);

    void UploadTLAS(Application* inApp, Device& inDevice, CommandList& inCmdList);
    void UploadLights(Application* inApp, Device& inDevice, CommandList& inCmdList);
    void UploadInstances(Application* inApp, Device& inDevice, CommandList& inCmdList);
    void UploadMaterials(Application* inApp, Device& inDevice, CommandList& inCmdList, bool inDisableAlbedo);

    BufferID GrowBuffer(Device& inDevice, BufferID inBuffer, const Buffer::Desc& inDesc);

private:
    Scene& m_Scene;
    
    BufferID m_TLASBuffer;
    BufferID m_LightsBuffer;
    BufferID m_ScratchBuffer;
    BufferID m_InstancesBuffer;
    BufferID m_MaterialsBuffer;
    BufferID m_D3D12InstancesBuffer;

    DescriptorID m_TLASDescriptor;
    DescriptorID m_LightsDescriptor;
    DescriptorID m_InstancesDescriptor;
    DescriptorID m_MaterialsDescriptor;
};

}