#include "pch.h"
#include "shared.h"
#include "DXScene.h"
#include "DXCommandList.h"
#include "Raekor/application.h"

namespace Raekor::DX12 {

void RayTracedScene::UploadTLAS(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, CommandList& inCmdList)
{
    auto rt_instances = std::vector<D3D12_RAYTRACING_INSTANCE_DESC> {};
    rt_instances.reserve(m_Scene.Count<Mesh>());

    auto instance_id = 0u;
    for (const auto& [entity, mesh, transform] : m_Scene.Each<Mesh, Transform>())
    {
        const auto& blas_buffer = inDevice.GetBuffer(BufferID(mesh.BottomLevelAS));

        auto instance = D3D12_RAYTRACING_INSTANCE_DESC
        {
            .InstanceID = instance_id++,
            .InstanceMask = 0xFF,
            .Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE,
            .AccelerationStructure = blas_buffer->GetGPUVirtualAddress(),
        };

        const auto transpose = glm::transpose(transform.worldTransform); // TODO: transform buffer
        memcpy(instance.Transform, glm::value_ptr(transpose), sizeof(instance.Transform));

        rt_instances.push_back(instance);
    }

    const auto instance_buffer_id = inDevice.CreateBuffer(Buffer::Desc
    {
        .size  = rt_instances.size() * sizeof(rt_instances[0]),
        .usage = Buffer::UPLOAD,
    });

    auto& instance_buffer = inDevice.GetBuffer(instance_buffer_id);

    {
        auto mapped_ptr = ( uint8_t* )nullptr;
        auto buffer_range = CD3DX12_RANGE(0, 0);
        gThrowIfFailed(instance_buffer->Map(0, &buffer_range, reinterpret_cast<void**>( &mapped_ptr )));
        memcpy(mapped_ptr, rt_instances.data(), rt_instances.size() * sizeof(rt_instances[0]));
        instance_buffer->Unmap(0, nullptr);
    }

    const auto inputs = D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS
    {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = uint32_t(rt_instances.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .InstanceDescs = instance_buffer->GetGPUVirtualAddress(),
    };

    auto prebuild_info = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO {};
    inDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    m_TLASBuffer = inDevice.CreateBuffer(Buffer::Desc
    {
        .size  = prebuild_info.ResultDataMaxSizeInBytes,
        .usage = Buffer::Usage::ACCELERATION_STRUCTURE,
        .debugName = L"TLAS_FULL_SCENE"
    });

    const auto result_buffer = inDevice.GetBuffer(m_TLASBuffer);

    auto scratch_buffer = inDevice.CreateBuffer(Buffer::Desc
    {
        .size = prebuild_info.ScratchDataSizeInBytes,
        .debugName = L"TLAS_SCRATCH_BUFFER"
    });

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.ScratchAccelerationStructureData = inDevice.GetBuffer(scratch_buffer)->GetGPUVirtualAddress();
    desc.DestAccelerationStructureData = result_buffer->GetGPUVirtualAddress();
    desc.Inputs = inputs;

    inCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    inDevice.ReleaseBuffer(scratch_buffer);
    inDevice.ReleaseBuffer(instance_buffer_id);
}



void RayTracedScene::UploadInstances(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, CommandList& inCmdList)
{
    const auto nr_of_meshes = m_Scene.Count<Mesh>();
    std::vector<RTGeometry> rt_geometries;
    rt_geometries.reserve(nr_of_meshes);

    for (const auto& [entity, mesh, transform] : m_Scene.Each<Mesh, Transform>())
    {
        if (!BufferID(mesh.BottomLevelAS).IsValid())
            continue;

        rt_geometries.emplace_back(RTGeometry
        {
            .mIndexBuffer = inDevice.GetBindlessHeapIndex(inDevice.GetBuffer(BufferID(mesh.indexBuffer)).GetDescriptor()),
            .mVertexBuffer = inDevice.GetBindlessHeapIndex(inDevice.GetBuffer(BufferID(mesh.vertexBuffer)).GetDescriptor()),
            .mMaterialIndex = GetMaterialIndex(mesh.material),
            .mLocalToWorldTransform = transform.worldTransform,
            .mInvLocalToWorldTransform = glm::inverse(transform.worldTransform)
        });
    }

    const auto instance_buffer_desc = Buffer::Desc
    {
        .size   = sizeof(RTGeometry) * nr_of_meshes,
        .stride = sizeof(RTGeometry),
        .usage  = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = L"RT_INSTANCE_BUFFER"
    };

    if (m_InstancesBuffer.IsValid())
    {
        const auto& buffer_desc = inDevice.GetBuffer(m_InstancesBuffer).GetDesc();

        if (instance_buffer_desc.size > buffer_desc.size)
        {
            const auto old_instance_buffer = m_InstancesBuffer;
            m_InstancesBuffer = inDevice.CreateBuffer(instance_buffer_desc);
            inDevice.ReleaseBuffer(old_instance_buffer);
        }
    }
    else 
        m_InstancesBuffer = inDevice.CreateBuffer(instance_buffer_desc);

    const auto& instance_buffer = inDevice.GetBuffer(m_InstancesBuffer);
    inStagingHeap.StageBuffer(inCmdList, instance_buffer.GetResource(), 0, rt_geometries.data(), instance_buffer_desc.size);
}



void RayTracedScene::UploadMaterials(Application* inApp, Device& inDevice, StagingHeap& inStagingHeap, CommandList& inCmdList)
{
    std::vector<RTMaterial> rt_materials(m_Scene.Count<Material>());

    auto material_index = 0u;
    for (const auto& [entity, material] : m_Scene.Each<Material>())
    {
        auto& rt_material = rt_materials[material_index];
        rt_material.mAlbedo = material.albedo;
        rt_material.mMetallic = material.metallic;
        rt_material.mRoughness = material.roughness;
        rt_material.mEmissive = Vec4(material.emissive, 1.0);

        rt_material.mAlbedoTexture = material.gpuAlbedoMap;
        rt_material.mNormalsTexture = material.gpuNormalMap;
        rt_material.mMetalRoughTexture = material.gpuMetallicRoughnessMap;

        // 0 is maybe fine? haven't seen this trigger before though
        assert(rt_material.mAlbedoTexture != 0 && rt_material.mNormalsTexture != 0 && rt_material.mMetalRoughTexture != 0);

        material_index++;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.StructureByteStride = sizeof(RTMaterial);
    srv_desc.Buffer.NumElements = rt_materials.size();

    const auto material_buffer_desc = Buffer::Desc
    {
        .size     = sizeof(RTMaterial) * rt_materials.size(),
        .stride   = sizeof(RTMaterial),
        .usage    = Buffer::Usage::SHADER_READ_ONLY,
        .viewDesc = &srv_desc,
        .debugName = L"RT_MATERIAL_BUFFER"
    };

    if (m_MaterialsBuffer.IsValid())
    {
        const auto& buffer_desc = inDevice.GetBuffer(m_MaterialsBuffer).GetDesc();
        
        if (material_buffer_desc.size > buffer_desc.size)
        {
            const auto old_material_buffer = m_MaterialsBuffer;
            m_MaterialsBuffer = inDevice.CreateBuffer(material_buffer_desc);
            inDevice.ReleaseBuffer(old_material_buffer);
        }
    }
    else 
        m_MaterialsBuffer = inDevice.CreateBuffer(material_buffer_desc);

    const auto& materials_buffer = inDevice.GetBuffer(m_MaterialsBuffer);
    inStagingHeap.StageBuffer(inCmdList, materials_buffer.GetResource(), 0, rt_materials.data(), material_buffer_desc.size);
}



uint32_t RayTracedScene::GetMaterialIndex(Entity inEntity)
{
    if (m_Scene.IsValid(inEntity) && m_Scene.Has<Material>(inEntity))
        return m_Scene.GetSparseEntities<Material>()[inEntity];
    else
        return 0;
}

} // namespace Raekor