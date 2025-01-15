#include "pch.h"
#include "RayTracedScene.h"

#include "Shared.h"
#include "CommandList.h"

#include "Iter.h"
#include "Primitives.h"
#include "Components.h"
#include "Application.h"

namespace RK::DX12 {

void RayTracedScene::UpdateBLAS(Application* inApp, Device& inDevice, Mesh& inMesh, Skeleton* inSkeleton, CommandList& inCmdList)
{
    D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
    geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    geom.Triangles.IndexBuffer = inDevice.GetBuffer(BufferID(inMesh.indexBuffer))->GetGPUVirtualAddress();
    geom.Triangles.IndexCount = inMesh.indices.size();
    geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

    geom.Triangles.VertexBuffer.StartAddress = inDevice.GetBuffer(BufferID(inSkeleton->skinnedVertexBuffer))->GetGPUVirtualAddress();
    geom.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    geom.Triangles.VertexCount = inMesh.positions.size();
    geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | 
                   D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE | 
                   D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &geom;
    inputs.NumDescs = 1;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    inDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    BufferID blas_buffer_id = BufferID(inMesh.BottomLevelAS);

    const BufferID scratch_buffer_id = inDevice.CreateBuffer(Buffer::Desc
    {
        .size = prebuild_info.ScratchDataSizeInBytes,
        .debugName = "SCRATCH_BUFFER_BLAS_RT"
    });

    Buffer& blas_buffer = inDevice.GetBuffer(blas_buffer_id);
    Buffer& scratch_buffer = inDevice.GetBuffer(scratch_buffer_id);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.Inputs = inputs; 
    desc.DestAccelerationStructureData = blas_buffer->GetGPUVirtualAddress();
    desc.SourceAccelerationStructureData = blas_buffer->GetGPUVirtualAddress();
    desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();

    inCmdList.TrackResource(blas_buffer);
    inCmdList.TrackResource(scratch_buffer);

    inCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    inDevice.ReleaseBuffer(scratch_buffer_id);
}


void RayTracedScene::UploadMesh(Application* inApp, Device& inDevice, Mesh& inMesh, Skeleton* inSkeleton, CommandList& inCmdList)
{
    const uint64_t indices_size = inMesh.indices.size() * sizeof(inMesh.indices[0]);
    const uint64_t vertices_size = inMesh.vertices.size() * sizeof(inMesh.vertices[0]);

    if (!vertices_size || !indices_size)
        return;

    D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
    geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    geom.Triangles.IndexBuffer = inDevice.GetBuffer(BufferID(inMesh.indexBuffer))->GetGPUVirtualAddress();
    geom.Triangles.IndexCount = inMesh.indices.size();
    geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

    geom.Triangles.VertexBuffer.StartAddress = inDevice.GetBuffer(BufferID(inMesh.vertexBuffer))->GetGPUVirtualAddress();
    geom.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    geom.Triangles.VertexCount = inMesh.positions.size();
    geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &geom;
    inputs.NumDescs = 1;

    if (inSkeleton != nullptr)
        inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    inDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    const BufferID blas_buffer_id = inDevice.CreateBuffer(Buffer::Desc
    {
        .size  = prebuild_info.ResultDataMaxSizeInBytes,
        .usage = Buffer::Usage::ACCELERATION_STRUCTURE,
        .debugName = "BLAS_BUFFER"
    });

    inMesh.BottomLevelAS = blas_buffer_id.GetValue();

    const BufferID scratch_buffer_id = inDevice.CreateBuffer(Buffer::Desc
    {
        .size = prebuild_info.ScratchDataSizeInBytes,
        .debugName = "SCRATCH_BUFFER_BLAS_RT"
    });

    Buffer& blas_buffer = inDevice.GetBuffer(blas_buffer_id);
    Buffer& scratch_buffer = inDevice.GetBuffer(scratch_buffer_id);
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();
    desc.DestAccelerationStructureData = blas_buffer->GetGPUVirtualAddress();
    desc.Inputs = inputs;

    inCmdList.TrackResource(blas_buffer);
    inCmdList.TrackResource(scratch_buffer);
        
    const Buffer& gpu_index_buffer = inDevice.GetBuffer(BufferID(inMesh.indexBuffer));
    const Buffer& gpu_vertex_buffer = inDevice.GetBuffer(BufferID(inMesh.vertexBuffer));

    inDevice.UploadBufferData(inCmdList, gpu_index_buffer, 0, inMesh.indices.data(), indices_size);
    inDevice.UploadBufferData(inCmdList, gpu_vertex_buffer, 0, inMesh.vertices.data(), vertices_size);

    const std::array barriers = 
    {
        CD3DX12_RESOURCE_BARRIER::Transition(gpu_index_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(gpu_vertex_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)
    };

    inCmdList->ResourceBarrier(barriers.size(), barriers.data());

    inCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    // TODO: validation layer warning??
    // inDevice.ReleaseBuffer(scratch_buffer_id);
}


void RayTracedScene::UploadSkeleton(Application* inApp, Device& inDevice, Skeleton& inSkeleton, CommandList& inCmdList)
{
    const Buffer& bone_index_buffer = inDevice.GetBuffer(BufferID(inSkeleton.boneIndexBuffer));
    const Buffer& bone_weights_buffer = inDevice.GetBuffer(BufferID(inSkeleton.boneWeightBuffer));

    inDevice.UploadBufferData(inCmdList, bone_index_buffer, 0, inSkeleton.boneIndices.data(), inSkeleton.boneIndices.size() * sizeof(IVec4));
    inDevice.UploadBufferData(inCmdList, bone_weights_buffer, 0, inSkeleton.boneWeights.data(), inSkeleton.boneWeights.size() * sizeof(Vec4));

    const std::array barriers =
    {
        CD3DX12_RESOURCE_BARRIER::Transition(bone_index_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(bone_weights_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)
    };

    inCmdList->ResourceBarrier(barriers.size(), barriers.data());
}



void RayTracedScene::UploadTLAS(Application* inApp, Device& inDevice, CommandList& inCmdList)
{
    if (!m_Scene.Count<Mesh>())
        return;

    PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), "UPLOAD TLAS");

    Array<D3D12_RAYTRACING_INSTANCE_DESC> rt_instances;
    rt_instances.reserve(m_Scene.Count<Mesh>());

    for (const auto& [entity, mesh] : m_Scene.Each<Mesh>())
    {
        if (!mesh.IsLoaded())
            continue;

        const Transform* transform = m_Scene.GetPtr<Transform>(entity);
        if (!transform)
            continue;

        if (!BufferID(mesh.BottomLevelAS).IsValid())
            continue;

        // vertex animation and custom pixel shaders are unsupported for now
        if (const Material* material = m_Scene.GetPtr<Material>(mesh.material))
        {
            if (material->vertexShader || material->pixelShader)
                continue;
        }

        const Buffer& blas_buffer = inDevice.GetBuffer(BufferID(mesh.BottomLevelAS));

        int instance_index = m_Scene.GetPackedIndex<Mesh>(entity);
        assert(instance_index != -1);

        D3D12_RAYTRACING_INSTANCE_DESC instance =
        {
            .InstanceID = uint32_t(instance_index),
            .InstanceMask = 0xFF,
            .Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE,
            .AccelerationStructure = blas_buffer->GetGPUVirtualAddress(),
        };

        const Mat4x4 transpose = glm::transpose(transform->worldTransform); // TODO: transform buffer
        memcpy(instance.Transform, glm::value_ptr(transpose), sizeof(instance.Transform));

        rt_instances.push_back(instance);
    }

    m_D3D12InstancesBuffer = GrowBuffer(inDevice, m_D3D12InstancesBuffer, Buffer::Desc
    {
        .size = rt_instances.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        .debugName = "D3D12_RAYTRACING_INSTANCE Buffer"
    });

    Buffer& instance_buffer = inDevice.GetBuffer(m_D3D12InstancesBuffer);
    inDevice.UploadBufferData(inCmdList, instance_buffer, 0, rt_instances.data(), instance_buffer.GetSize());
    
    const auto after_copy_barrier = CD3DX12_RESOURCE_BARRIER::Transition(instance_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    inCmdList->ResourceBarrier(1, &after_copy_barrier);

    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs =
    {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = uint32_t(rt_instances.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .InstanceDescs = instance_buffer->GetGPUVirtualAddress()
    };

    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS no_inputs =
    {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY
    };

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    inDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO empty_prebuild_info = {};
    inDevice->GetRaytracingAccelerationStructurePrebuildInfo(&no_inputs, &empty_prebuild_info);

    m_TLASBuffer = GrowBuffer(inDevice, m_TLASBuffer, Buffer::Desc
    {
        .size = prebuild_info.ResultDataMaxSizeInBytes,
        .usage = Buffer::Usage::ACCELERATION_STRUCTURE,
        .debugName = "TLAS_FULL_SCENE"
    });

    m_EmptyTLASBuffer = GrowBuffer(inDevice, m_EmptyTLASBuffer, Buffer::Desc
    {
        .size = empty_prebuild_info.ResultDataMaxSizeInBytes,
        .usage = Buffer::Usage::ACCELERATION_STRUCTURE,
        .debugName = "TLAS_EMPTY"
    });

    m_TLASDescriptor = inDevice.GetBuffer(m_TLASBuffer).GetDescriptor();
    m_EmptyTLASDescriptor = inDevice.GetBuffer(m_EmptyTLASBuffer).GetDescriptor();

    m_ScratchBuffer = GrowBuffer(inDevice, m_ScratchBuffer, Buffer::Desc
    {
        .size = prebuild_info.ScratchDataSizeInBytes,
        .debugName = "TLAS_SCRATCH_BUFFER"
    });

    m_EmptyScratchBuffer = GrowBuffer(inDevice, m_EmptyScratchBuffer, Buffer::Desc
    {
        .size = empty_prebuild_info.ScratchDataSizeInBytes,
        .debugName = "TLAS_SCRATCH_BUFFER"
    });

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.Inputs = inputs;
    desc.DestAccelerationStructureData = inDevice.GetBuffer(m_TLASBuffer)->GetGPUVirtualAddress();
    desc.ScratchAccelerationStructureData = inDevice.GetBuffer(m_ScratchBuffer)->GetGPUVirtualAddress();

    inCmdList.TrackResource(inDevice.GetBuffer(m_TLASBuffer));
    inCmdList.TrackResource(inDevice.GetBuffer(m_ScratchBuffer));
    inCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC empty_desc = {};
    empty_desc.Inputs = no_inputs;
    empty_desc.DestAccelerationStructureData = inDevice.GetBuffer(m_EmptyTLASBuffer)->GetGPUVirtualAddress();
    empty_desc.ScratchAccelerationStructureData = inDevice.GetBuffer(m_EmptyScratchBuffer)->GetGPUVirtualAddress();

    inCmdList.TrackResource(inDevice.GetBuffer(m_EmptyTLASBuffer));
    inCmdList.TrackResource(inDevice.GetBuffer(m_EmptyScratchBuffer));
    inCmdList->BuildRaytracingAccelerationStructure(&empty_desc, 0, nullptr);
}


void RayTracedScene::UploadLights(Application* inApp, Device& inDevice, CommandList& inCmdList)
{
    static_assert( sizeof(RTLight) == sizeof(Light) );

    if (!m_Scene.Count<Light>())
        return;

    Slice<const Light> lights = m_Scene.GetComponentStorage<Light>()->GetComponents();

    m_LightsBuffer = GrowBuffer(inDevice, m_LightsBuffer, Buffer::Desc
    {
        .size   = sizeof(lights[0]) * lights.size(),
        .stride = sizeof(lights[0]),
        .usage  = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = "RT_LIGHTS_BUFFER"
    });

    m_LightsDescriptor = inDevice.GetBuffer(m_LightsBuffer).GetDescriptor();

    const Buffer& lights_buffer = inDevice.GetBuffer(m_LightsBuffer);
    
    inDevice.UploadBufferData(inCmdList, lights_buffer, 0, lights.data(), lights_buffer.GetSize());
}


void RayTracedScene::UploadInstances(Application* inApp, Device& inDevice, CommandList& inCmdList)
{
    if (!m_Scene.Count<Mesh>())
        return;

    PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), "UPLOAD INSTANCES");

    const uint32_t nr_of_meshes = m_Scene.Count<Mesh>();
    Array<RTGeometry> rt_geometries;
    rt_geometries.reserve(nr_of_meshes);

    for (const auto& [entity, mesh] : m_Scene.Each<Mesh>())
    {
        if (!mesh.IsLoaded())
            continue;

        const Transform* transform = m_Scene.GetPtr<Transform>(entity);

        if (!transform)
            continue;

        if (!BufferID(mesh.BottomLevelAS).IsValid())
            continue;

        int material_index = m_Scene.GetPackedIndex<Material>(mesh.material);
        material_index = material_index == -1 ? 0 : material_index;

        uint32_t vertex_buffer = mesh.vertexBuffer;
        if (Skeleton* skeleton = m_Scene.GetPtr<Skeleton>(entity))
            vertex_buffer = skeleton->skinnedVertexBuffer;

        rt_geometries.emplace_back(RTGeometry
        {
            .mIndexBuffer = inDevice.GetBindlessHeapIndex(BufferID(mesh.indexBuffer)),
            .mVertexBuffer = inDevice.GetBindlessHeapIndex(BufferID(vertex_buffer)),
            .mMaterialIndex = uint32_t(material_index),
            .mWorldTransform = transform->worldTransform,
        });
    }

    m_InstancesBuffer = GrowBuffer(inDevice, m_InstancesBuffer, Buffer::Desc
    {
        .size      = sizeof(RTGeometry) * nr_of_meshes,
        .stride    = sizeof(RTGeometry),
        .usage     = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = "RT_INSTANCE_BUFFER"
    });

    m_InstancesDescriptor = inDevice.GetBuffer(m_InstancesBuffer).GetDescriptor();

    const Buffer& instance_buffer = inDevice.GetBuffer(m_InstancesBuffer);
    inDevice.UploadBufferData(inCmdList, instance_buffer, 0, rt_geometries.data(), instance_buffer.GetSize());
}


void RayTracedScene::UploadMaterials(Application* inApp, Device& inDevice, CommandList& inCmdList, bool inDisableAlbedo)
{
    PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), "UPLOAD MATERIALS");

    Slice<const Material> materials = m_Scene.GetComponentStorage<Material>()->GetComponents();

    if (materials.empty())
        materials = Slice(&Material::Default, 1);

    Array<RTMaterial> rt_materials;
    rt_materials.resize(materials.size());

    for (const auto& [index, material] : gEnumerate(materials))
    {
        RTMaterial& rt_material = rt_materials[index];
        rt_material.mAlbedo = material.albedo;
        rt_material.mMetallic = material.metallic;
        rt_material.mRoughness = material.roughness;
        rt_material.mEmissive = Vec4(material.emissive, 1.0);

        rt_material.mAlbedoTexture = inDevice.GetBindlessHeapIndex(TextureID(material.gpuAlbedoMap ? material.gpuAlbedoMap : Material::Default.gpuAlbedoMap));
        rt_material.mNormalsTexture = inDevice.GetBindlessHeapIndex(TextureID(material.gpuNormalMap ? material.gpuNormalMap : Material::Default.gpuNormalMap));
        rt_material.mEmissiveTexture = inDevice.GetBindlessHeapIndex(TextureID(material.gpuEmissiveMap ? material.gpuEmissiveMap : Material::Default.gpuEmissiveMap));
        rt_material.mMetallicTexture = inDevice.GetBindlessHeapIndex(TextureID(material.gpuMetallicMap ? material.gpuMetallicMap : Material::Default.gpuMetallicMap));
        rt_material.mRoughnessTexture = inDevice.GetBindlessHeapIndex(TextureID(material.gpuRoughnessMap ? material.gpuRoughnessMap : Material::Default.gpuRoughnessMap));

        if (inDisableAlbedo)
        {
            rt_material.mAlbedo = Material::Default.albedo;
            rt_material.mAlbedoTexture = inDevice.GetBindlessHeapIndex(TextureID(Material::Default.gpuAlbedoMap));
        }

        // 0 is maybe fine? haven't seen this trigger before though
        assert(rt_material.mAlbedoTexture != 0 && rt_material.mNormalsTexture != 0 && rt_material.mMetallicTexture != 0);
    }

    m_MaterialsBuffer = GrowBuffer(inDevice, m_MaterialsBuffer, Buffer::Desc
    {
        .size      = sizeof(RTMaterial) * rt_materials.size(),
        .stride    = sizeof(RTMaterial),
        .usage     = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = "MaterialsBuffer"
    });

    m_MaterialsDescriptor = inDevice.GetBuffer(m_MaterialsBuffer).GetDescriptor();

    const Buffer& materials_buffer = inDevice.GetBuffer(m_MaterialsBuffer);
    inDevice.UploadBufferData(inCmdList, materials_buffer, 0, rt_materials.data(), materials_buffer.GetSize());
}


BufferID RayTracedScene::GrowBuffer(Device& inDevice, BufferID inBuffer, const Buffer::Desc& inDesc)
{
    if (inBuffer.IsValid())
    {
        if (inDesc.size <= inDevice.GetBuffer(inBuffer).GetSize())
            return inBuffer;

        inDevice.ReleaseBuffer(inBuffer);
    }

    return inDevice.CreateBuffer(inDesc);
}

} // namespace Raekor