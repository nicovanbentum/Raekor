#include "pch.h"
#include "GPUScene.h"

#include "Shared.h"
#include "RenderWorld.h"
#include "CommandList.h"
#include "GPUProfiler.h"

#include "Primitives.h"

namespace RK::DX12 {

static Buffer::Desc sScratchBufferDesc(uint64_t inSize, const char* inDebugName)
{
    constexpr uint64_t alignment = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT;
    return Buffer::RWByteAddressBuffer(std::max(gAlignUp(inSize, alignment), alignment), inDebugName);
}


static D3D12_RAYTRACING_GEOMETRY_DESC sGetGeometryDesc(Device& inDevice, BufferID inIndexBuffer, uint32_t inIndexCount, BufferID inVertexBuffer, uint32_t inVertexCount)
{
    D3D12_RAYTRACING_GEOMETRY_DESC geometry = {};
    geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    geometry.Triangles.IndexBuffer = inIndexBuffer.IsValid() ? inDevice.GetBuffer(inIndexBuffer)->GetGPUVirtualAddress() : 0;
    geometry.Triangles.IndexCount = inIndexCount;
    geometry.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

    geometry.Triangles.VertexBuffer.StartAddress = inVertexBuffer.IsValid() ? inDevice.GetBuffer(inVertexBuffer)->GetGPUVirtualAddress() : 0;
    geometry.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    geometry.Triangles.VertexCount = inVertexCount;
    geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

    return geometry;
}


static D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS sGetBottomLevelInputs(const D3D12_RAYTRACING_GEOMETRY_DESC& inGeometry, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS inFlags)
{
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.Flags = inFlags;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &inGeometry;
    inputs.NumDescs = 1;
    return inputs;
}


static D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS sGetBottomLevelBuildFlags(bool inAllowUpdate)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    if (inAllowUpdate)
        flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

    return flags;
}


BufferID GPUScene::CreateBottomLevelAS(Device& inDevice, const BottomLevelASBuild& inBuild)
{
    const D3D12_RAYTRACING_GEOMETRY_DESC geometry = sGetGeometryDesc(inDevice, BufferID(), inBuild.mIndexCount, BufferID(), inBuild.mVertexCount);
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = sGetBottomLevelInputs(geometry, sGetBottomLevelBuildFlags(inBuild.mAllowUpdate));

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    inDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    const BufferID bottom_level_as = inDevice.CreateBuffer(Buffer::Desc
    {
        .size  = prebuild_info.ResultDataMaxSizeInBytes,
        .usage = Buffer::Usage::ACCELERATION_STRUCTURE,
        .debugName = "BLAS_BUFFER"
    });

    BottomLevelASBuild build = inBuild;
    build.mBottomLevelAS = bottom_level_as;

    std::scoped_lock lock(m_PendingBuildsMutex);
    m_PendingBuilds.push_back(build);

    return bottom_level_as;
}


void GPUScene::ReleaseBottomLevelAS(Device& inDevice, BufferID inBottomLevelAS)
{
    if (!inBottomLevelAS.IsValid())
        return;

    {
        std::scoped_lock lock(m_PendingBuildsMutex);
        std::erase_if(m_PendingBuilds, [inBottomLevelAS](const BottomLevelASBuild& inBuild) { return inBuild.mBottomLevelAS == inBottomLevelAS; });
    }

    inDevice.ReleaseBuffer(inBottomLevelAS);
}


uint64_t GPUScene::sGetBottomLevelASUpdateScratchSize(Device& inDevice, uint32_t inIndexCount, uint32_t inVertexCount)
{
    const D3D12_RAYTRACING_GEOMETRY_DESC geometry = sGetGeometryDesc(inDevice, BufferID(), inIndexCount, BufferID(), inVertexCount);
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = sGetBottomLevelInputs(geometry, sGetBottomLevelBuildFlags(true));

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    inDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    return prebuild_info.UpdateScratchDataSizeInBytes;
}


void GPUScene::BuildPendingBottomLevelAS(Device& inDevice, CommandList& inCmdList)
{
    Array<BottomLevelASBuild> builds;

    {
        std::scoped_lock lock(m_PendingBuildsMutex);
        builds.swap(m_PendingBuilds);
    }

    if (builds.empty())
        return;

    EVENT_SCOPE_GPU(inCmdList, "BUILD BLAS");

    for (const BottomLevelASBuild& build : builds)
    {
        const D3D12_RAYTRACING_GEOMETRY_DESC geometry = sGetGeometryDesc(inDevice, build.mIndexBuffer, build.mIndexCount, build.mVertexBuffer, build.mVertexCount);
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = sGetBottomLevelInputs(geometry, sGetBottomLevelBuildFlags(build.mAllowUpdate));

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
        inDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

        const BufferID scratch_buffer = inDevice.CreateBuffer(sScratchBufferDesc(prebuild_info.ScratchDataSizeInBytes, "SCRATCH_BUFFER_BLAS_BUILD"));

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
        desc.Inputs = inputs;
        desc.DestAccelerationStructureData = inDevice.GetBuffer(build.mBottomLevelAS)->GetGPUVirtualAddress();
        desc.ScratchAccelerationStructureData = inDevice.GetBuffer(scratch_buffer)->GetGPUVirtualAddress();

        inCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

        inDevice.ReleaseBuffer(scratch_buffer);
    }
}


void GPUScene::RefitBottomLevelAS(Device& inDevice, CommandList& inCmdList, const RenderSkinnedMesh& inSkinnedMesh)
{
    if (!inSkinnedMesh.mBottomLevelAS.IsValid() || !inSkinnedMesh.mScratchBuffer.IsValid())
        return;

    const D3D12_RAYTRACING_GEOMETRY_DESC geometry = sGetGeometryDesc(inDevice, inSkinnedMesh.mIndexBuffer, inSkinnedMesh.mIndexCount, inSkinnedMesh.mSkinnedVertexBuffer, inSkinnedMesh.mVertexCount);
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = sGetBottomLevelInputs(geometry, sGetBottomLevelBuildFlags(true) | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE);

    const D3D12_GPU_VIRTUAL_ADDRESS bottom_level_as = inDevice.GetBuffer(inSkinnedMesh.mBottomLevelAS)->GetGPUVirtualAddress();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.Inputs = inputs;
    desc.DestAccelerationStructureData = bottom_level_as;
    desc.SourceAccelerationStructureData = bottom_level_as;
    desc.ScratchAccelerationStructureData = inDevice.GetBuffer(inSkinnedMesh.mScratchBuffer)->GetGPUVirtualAddress();

    inCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
}


void GPUScene::Upload(Device& inDevice, CommandList& inCmdList, const RenderWorld& inWorld)
{
    BuildPendingBottomLevelAS(inDevice, inCmdList);

    Slice<const RTGeometry> geometries = inWorld.GetGeometries();

    if (!geometries.empty())
    {
        EVENT_SCOPE_GPU(inCmdList, "UPLOAD INSTANCES");

        m_InstancesBuffer = GrowBuffer(inDevice, m_InstancesBuffer, Buffer::Desc
        {
            .size      = geometries.size_bytes(),
            .stride    = sizeof(RTGeometry),
            .usage     = Buffer::Usage::SHADER_READ_ONLY,
            .debugName = "RT_INSTANCE_BUFFER"
        });

        Buffer& instances_buffer = inDevice.GetBuffer(m_InstancesBuffer);
        m_InstancesDescriptor = instances_buffer.GetDescriptor();

        inDevice.UploadBufferData(inCmdList, instances_buffer, 0, geometries.data(), uint32_t(geometries.size_bytes()));
    }

    Slice<const RTMaterial> materials = inWorld.GetMaterials();

    {
        EVENT_SCOPE_GPU(inCmdList, "UPLOAD MATERIALS");

        m_MaterialsBuffer = GrowBuffer(inDevice, m_MaterialsBuffer, Buffer::Desc
        {
            .size      = materials.size_bytes(),
            .stride    = sizeof(RTMaterial),
            .usage     = Buffer::Usage::SHADER_READ_ONLY,
            .debugName = "MaterialsBuffer"
        });

        Buffer& materials_buffer = inDevice.GetBuffer(m_MaterialsBuffer);
        m_MaterialsDescriptor = materials_buffer.GetDescriptor();

        inDevice.UploadBufferData(inCmdList, materials_buffer, 0, materials.data(), uint32_t(materials.size_bytes()));
    }

    Slice<const RTLight> lights = inWorld.GetLights();
    m_LightsCount = uint32_t(lights.size());

    if (!lights.empty())
    {
        m_LightsBuffer = GrowBuffer(inDevice, m_LightsBuffer, Buffer::Desc
        {
            .size   = lights.size_bytes(),
            .stride = sizeof(RTLight),
            .usage  = Buffer::Usage::SHADER_READ_ONLY,
            .debugName = "RT_LIGHTS_BUFFER"
        });

        Buffer& lights_buffer = inDevice.GetBuffer(m_LightsBuffer);
        m_LightsDescriptor = lights_buffer.GetDescriptor();

        inDevice.UploadBufferData(inCmdList, lights_buffer, 0, lights.data(), uint32_t(lights.size_bytes()));
    }

    UploadTLASInstances(inDevice, inCmdList, inWorld);
}


void GPUScene::UploadTLASInstances(Device& inDevice, CommandList& inCmdList, const RenderWorld& inWorld)
{
    m_TLASInstanceCount = 0;

    Slice<const RenderInstance> instances = inWorld.GetInstances();

    if (instances.empty())
        return;

    EVENT_SCOPE_GPU(inCmdList, "UPLOAD TLAS INSTANCES");

    Array<D3D12_RAYTRACING_INSTANCE_DESC> rt_instances;
    rt_instances.reserve(instances.size());

    for (uint32_t instance_index = 0; instance_index < instances.size(); instance_index++)
    {
        const RenderInstance& instance = instances[instance_index];

        if (!instance.IsRayTraced() || !instance.mBottomLevelAS.IsValid())
            continue;

        D3D12_RAYTRACING_INSTANCE_DESC rt_instance =
        {
            .InstanceID = instance_index,
            .InstanceMask = 0xFF,
            .Flags = UINT(instance.mBlendMode == RENDER_BLEND_MODE_MASKED ? D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE : D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE),
            .AccelerationStructure = inDevice.GetBuffer(instance.mBottomLevelAS)->GetGPUVirtualAddress(),
        };

        const Mat4x4 transpose = glm::transpose(instance.mWorldTransform);
        std::memcpy(rt_instance.Transform, glm::value_ptr(transpose), sizeof(rt_instance.Transform));

        rt_instances.push_back(rt_instance);
    }

    m_D3D12InstancesBuffer = GrowBuffer(inDevice, m_D3D12InstancesBuffer, Buffer::Desc
    {
        .size = std::max(rt_instances.size(), size_t(1)) * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        .debugName = "D3D12_RAYTRACING_INSTANCE Buffer"
    });

    m_TLASInstanceCount = uint32_t(rt_instances.size());

    Buffer& instance_buffer = inDevice.GetBuffer(m_D3D12InstancesBuffer);

    if (!rt_instances.empty())
    {
        inDevice.UploadBufferData(inCmdList, instance_buffer, 0, rt_instances.data(), rt_instances.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

        const auto after_copy_barrier = CD3DX12_RESOURCE_BARRIER::Transition(instance_buffer.GetD3D12Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        inCmdList->ResourceBarrier(1, &after_copy_barrier);
    }

    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs =
    {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = m_TLASInstanceCount,
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

    m_ScratchBuffer = GrowBuffer(inDevice, m_ScratchBuffer, sScratchBufferDesc(prebuild_info.ScratchDataSizeInBytes, "TLAS_SCRATCH_BUFFER"));

    m_EmptyScratchBuffer = GrowBuffer(inDevice, m_EmptyScratchBuffer, sScratchBufferDesc(empty_prebuild_info.ScratchDataSizeInBytes, "TLAS_EMPTY_SCRATCH_BUFFER"));
}


void GPUScene::BuildTLAS(Device& inDevice, CommandList& inCmdList)
{
    if (!m_TLASBuffer.IsValid() || !m_EmptyTLASBuffer.IsValid())
        return;

    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs =
    {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = m_TLASInstanceCount,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .InstanceDescs = inDevice.GetBuffer(m_D3D12InstancesBuffer)->GetGPUVirtualAddress()
    };

    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS no_inputs =
    {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY
    };

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.Inputs = inputs;
    desc.DestAccelerationStructureData = inDevice.GetBuffer(m_TLASBuffer)->GetGPUVirtualAddress();
    desc.ScratchAccelerationStructureData = inDevice.GetBuffer(m_ScratchBuffer)->GetGPUVirtualAddress();

    inCmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC empty_desc = {};
    empty_desc.Inputs = no_inputs;
    empty_desc.DestAccelerationStructureData = inDevice.GetBuffer(m_EmptyTLASBuffer)->GetGPUVirtualAddress();
    empty_desc.ScratchAccelerationStructureData = inDevice.GetBuffer(m_EmptyScratchBuffer)->GetGPUVirtualAddress();

    inCmdList->BuildRaytracingAccelerationStructure(&empty_desc, 0, nullptr);
}


BufferID GPUScene::GrowBuffer(Device& inDevice, BufferID inBuffer, const Buffer::Desc& inDesc)
{
    if (inBuffer.IsValid())
    {
        if (inDesc.size <= inDevice.GetBuffer(inBuffer).GetSize())
            return inBuffer;

        inDevice.ReleaseBuffer(inBuffer);
    }

    return inDevice.CreateBuffer(inDesc);
}

} // namespace RK::DX12
