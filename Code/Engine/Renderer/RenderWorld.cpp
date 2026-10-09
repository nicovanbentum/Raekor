#include "PCH.h"
#include "RenderWorld.h"

#include "Device.h"

#include "Scene.h"
#include "Profiler.h"
#include "Components.h"

namespace RK::DX12 {

static ERenderBlendMode sGetRenderBlendMode(EMaterialBlendMode inBlendMode)
{
    switch (inBlendMode)
    {
        case MATERIAL_BLEND_MODE_MASKED:  return RENDER_BLEND_MODE_MASKED;
        case MATERIAL_BLEND_MODE_BLENDED: return RENDER_BLEND_MODE_TRANSPARENT;
        default:                          return RENDER_BLEND_MODE_OPAQUE;
    }
}


static RTMaterial sGetRenderMaterial(const Material& inMaterial, const Device& inDevice, float inExposure, bool inDisableAlbedo)
{
    auto GetTexture = [&inDevice](uint32_t inTexture, uint32_t inDefaultTexture)
    {
        return inDevice.GetBindlessHeapIndex(TextureID(inTexture ? inTexture : inDefaultTexture));
    };

    RTMaterial material =
    {
        .mMetallic          = inMaterial.metallic,
        .mRoughness         = inMaterial.roughness,
        .mAlbedoTexture     = GetTexture(inMaterial.gpuAlbedoMap, Material::Default.gpuAlbedoMap),
        .mNormalsTexture    = GetTexture(inMaterial.gpuNormalMap, Material::Default.gpuNormalMap),
        .mEmissiveTexture   = GetTexture(inMaterial.gpuEmissiveMap, Material::Default.gpuEmissiveMap),
        .mMetallicTexture   = GetTexture(inMaterial.gpuMetallicMap, Material::Default.gpuMetallicMap),
        .mRoughnessTexture  = GetTexture(inMaterial.gpuRoughnessMap, Material::Default.gpuRoughnessMap),
        .mAlphaCutoff       = inMaterial.alphaCutoff,
        .mAlbedo            = inMaterial.albedo,
        .mEmissive          = Vec4(inMaterial.emissive * inExposure, 1.0f)
    };

    if (inDisableAlbedo)
    {
        material.mAlbedo = Vec4(1.0f);
        material.mAlbedoTexture = GetTexture(Material::Default.gpuAlbedoMap, Material::Default.gpuAlbedoMap);
    }

    return material;
}


void RenderWorld::Extract(const Scene& inScene, const Device& inDevice, float inExposure, bool inDisableAlbedo)
{
    PROFILE_FUNCTION_CPU();

    m_Instances.clear();
    m_Batches.clear();
    m_SkinnedMeshes.clear();
    m_BoneMatrices.clear();
    m_Geometries.clear();
    m_Materials.clear();
    m_Lights.clear();
    m_Names.clear();
    m_Names.push_back('\0');

    Slice<const Material> materials = inScene.GetStorage<Material>();
    m_Materials.reserve(materials.size() + 1);

    for (const Material& material : materials)
        m_Materials.push_back(sGetRenderMaterial(material, inDevice, inExposure, inDisableAlbedo));

    const uint32_t default_material_index = uint32_t(m_Materials.size());
    m_Materials.push_back(sGetRenderMaterial(Material::Default, inDevice, inExposure, inDisableAlbedo));

    m_Instances.reserve(inScene.Count<Mesh>());
    m_Geometries.reserve(inScene.Count<Mesh>());

    for (const auto& [entity, mesh] : inScene.Each<Mesh>())
    {
        if (!mesh.IsLoaded())
            continue;

        const Transform* transform = inScene.GetPtr<Transform>(entity);

        if (transform == nullptr)
            continue;

        const Material* material = inScene.GetPtr<Material>(mesh.material);
        const Skeleton* skeleton = inScene.GetPtr<Skeleton>(entity);
        const Name* name = inScene.GetPtr<Name>(entity);

        RenderInstance& instance = m_Instances.emplace_back();
        instance.mEntity = entity;
        instance.mIndexBuffer = gToBufferID(mesh.indexBuffer);
        instance.mVertexBuffer = gToBufferID(skeleton ? skeleton->skinnedVertexBuffer : mesh.vertexBuffer);
        instance.mBottomLevelAS = gToBufferID(mesh.BottomLevelAS);
        instance.mIndexCount = uint32_t(mesh.indices.size());
        instance.mMaterialIndex = material ? inScene.GetPackedIndex<Material>(mesh.material) : default_material_index;
        instance.mNameOffset = AddName(!mesh.name.empty() ? mesh.name.c_str() : name ? name->name.c_str() : nullptr);
        instance.mBlendMode = sGetRenderBlendMode(material ? material->blendMode : Material::Default.blendMode);
        instance.mVertexShader = material ? material->vertexShader : 0;
        instance.mPixelShader = material ? material->pixelShader : 0;
        instance.mBoundsCenter = mesh.bbox.Transformed(transform->worldTransform).GetCenter();
        instance.mWorldTransform = transform->worldTransform;
        instance.mPrevWorldTransform = transform->prevWorldTransform;
    }

    const auto GetBatchKey = [](const RenderInstance& inInstance)
    {
        return std::make_tuple(inInstance.mBlendMode, inInstance.mVertexShader, inInstance.mPixelShader, inInstance.mIndexBuffer.GetValue(), inInstance.mVertexBuffer.GetValue(), uint32_t(inInstance.mEntity));
    };

    std::sort(m_Instances.begin(), m_Instances.end(), [&](const RenderInstance& inLeft, const RenderInstance& inRight) { return GetBatchKey(inLeft) < GetBatchKey(inRight); });

    for (uint32_t instance_index = 0; instance_index < m_Instances.size(); instance_index++)
    {
        const RenderInstance& instance = m_Instances[instance_index];

        m_Geometries.push_back(RTGeometry
        {
            .mEntity             = instance.mEntity,
            .mIndexBuffer        = inDevice.GetBindlessHeapIndex(instance.mIndexBuffer),
            .mVertexBuffer       = inDevice.GetBindlessHeapIndex(instance.mVertexBuffer),
            .mMaterialIndex      = instance.mMaterialIndex,
            .mWorldTransform     = instance.mWorldTransform,
            .mPrevWorldTransform = instance.mPrevWorldTransform
        });

        if (!m_Batches.empty())
        {
            const RenderInstance& first = m_Instances[m_Batches.back().mFirstInstance];

            const bool can_batch = !instance.HasCustomShaders() && !first.HasCustomShaders() &&
                instance.mBlendMode == first.mBlendMode &&
                instance.mIndexBuffer == first.mIndexBuffer &&
                instance.mVertexBuffer == first.mVertexBuffer &&
                instance.mIndexCount == first.mIndexCount;

            if (can_batch)
            {
                m_Batches.back().mInstanceCount++;
                continue;
            }
        }

        m_Batches.push_back(RenderBatch { .mFirstInstance = instance_index, .mInstanceCount = 1 });
    }

    for (const auto& [entity, mesh, skeleton] : inScene.Each<Mesh, Skeleton>())
    {
        if (!skeleton.gpuBuffersUploaded || !mesh.IsLoaded())
            continue;

        const Name* name = inScene.GetPtr<Name>(entity);

        m_SkinnedMeshes.push_back(RenderSkinnedMesh
        {
            .mEntity                = entity,
            .mIndexBuffer           = gToBufferID(mesh.indexBuffer),
            .mMeshVertexBuffer      = gToBufferID(mesh.vertexBuffer),
            .mSkinnedVertexBuffer   = gToBufferID(skeleton.skinnedVertexBuffer),
            .mBoneIndexBuffer       = gToBufferID(skeleton.boneIndexBuffer),
            .mBoneWeightBuffer      = gToBufferID(skeleton.boneWeightBuffer),
            .mBoneTransformsBuffer  = gToBufferID(skeleton.boneTransformsBuffer),
            .mBottomLevelAS         = gToBufferID(mesh.BottomLevelAS),
            .mScratchBuffer         = gToBufferID(skeleton.blasScratchBuffer),
            .mVertexCount           = uint32_t(mesh.positions.size()),
            .mIndexCount            = uint32_t(mesh.indices.size()),
            .mBoneMatrixOffset      = uint32_t(m_BoneMatrices.size()),
            .mBoneMatrixCount       = uint32_t(skeleton.boneTransformMatrices.size()),
            .mNameOffset            = AddName(name ? name->name.c_str() : nullptr)
        });

        m_BoneMatrices.insert(m_BoneMatrices.end(), skeleton.boneTransformMatrices.begin(), skeleton.boneTransformMatrices.end());
    }

    static_assert( sizeof(RTLight) == sizeof(Light) );

    Slice<const Light> lights = inScene.GetStorage<Light>();
    m_Lights.resize(lights.size());
    std::memcpy(m_Lights.data(), lights.data(), lights.size_bytes());

    for (RTLight& light : m_Lights)
        light.mColor.a *= inExposure;

    m_Sun = RenderSun();
    m_Sun.mDirection = inScene.GetSunLightDirection();

    if (const DirectionalLight* sun_light = inScene.GetSunLight())
    {
        m_Sun.mEnabled = true;
        m_Sun.mColor = sun_light->GetColor();
        m_Sun.mCubeMap = gToTextureID(sun_light->cubeMap);
    }

    m_DDGIVolume = RenderDDGIVolume();

    for (const auto& [entity, ddgi_settings, transform] : inScene.Each<DDGISceneSettings, Transform>())
    {
        m_DDGIVolume.mEnabled = true;
        m_DDGIVolume.mProbeCount = ddgi_settings.mDDGIProbeCount;
        m_DDGIVolume.mProbeSpacing = ddgi_settings.mDDGIProbeSpacing;
        m_DDGIVolume.mCornerPosition = transform.position;
        m_DDGIVolume.mFollowCamera = ddgi_settings.mFollowCamera;
        m_DDGIVolume.mCascadeCount = ddgi_settings.mCascadeCount;
        break;
    }
}


uint32_t RenderWorld::AddName(const char* inName)
{
    if (inName == nullptr || *inName == '\0')
        return 0;

    const uint32_t offset = uint32_t(m_Names.size());
    m_Names.insert(m_Names.end(), inName, inName + std::strlen(inName) + 1);
    return offset;
}

} // namespace RK::DX12
