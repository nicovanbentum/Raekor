#include "PCH.h"
#include "RenderWorld.h"

#include "Device.h"
#include "Samplers.h"

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

    RK_ASSERT(inMaterial.gpuSamplerIndex < SAMPLER_COUNT);

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
        .mSamplerIndex      = inMaterial.gpuSamplerIndex,
        .mAlbedo          = inMaterial.albedo,
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
    m_SkinnedMeshes.clear();
    m_BoneMatrices.clear();
    m_Geometries.clear();
    m_Materials.clear();
    m_Lights.clear();
    m_Names.clear();
    m_Names.push_back('\0');

    
    {
        PROFILE_SCOPE_CPU("Extract Materials");

        Slice<const Material> materials = inScene.GetStorage<Material>();
        m_Materials.reserve(materials.size() + 1);

        for (const Material& material : materials)
            m_Materials.push_back(sGetRenderMaterial(material, inDevice, inExposure, inDisableAlbedo));
    }
    
    const uint32_t default_material_index = uint32_t(m_Materials.size());
    m_Materials.push_back(sGetRenderMaterial(Material::Default, inDevice, inExposure, inDisableAlbedo));

    {
        PROFILE_SCOPE_CPU("Extract Instances");

        m_Instances.reserve(inScene.Count<Mesh>());
        m_Geometries.reserve(inScene.Count<Mesh>());

        for (const auto& [entity, mesh] : inScene.Each<Mesh>())
        {
            const MeshGeometry* geometry = mesh.GetGeometry();

            if (geometry == nullptr || !geometry->IsUploaded())
                continue;

            const Transform* transform = inScene.GetPtr<Transform>(entity);

            if (transform == nullptr)
                continue;

            const Material* material = inScene.GetPtr<Material>(mesh.material);
            const Skeleton* skeleton = inScene.GetPtr<Skeleton>(entity);
            const Name* name = inScene.GetPtr<Name>(entity);

            const bool is_skinned = skeleton && skeleton->gpuBuffersUploaded;

            RenderInstance& instance = m_Instances.emplace_back();
            instance.mEntity = entity;
            instance.mIndexBuffer = gToBufferID(geometry->indexBuffer);
            instance.mVertexBuffer = gToBufferID(is_skinned ? skeleton->skinnedVertexBuffer : geometry->vertexBuffer);
            instance.mBottomLevelAS = gToBufferID(is_skinned && skeleton->skinnedBottomLevelAS ? skeleton->skinnedBottomLevelAS : geometry->BottomLevelAS);
            instance.mIndexCount = uint32_t(geometry->indices.size());
            instance.mMaterialIndex = material ? inScene.GetPackedIndex<Material>(mesh.material) : default_material_index;
            instance.mNameOffset = AddName(name ? name->name.c_str() : geometry->name.c_str());
            instance.mBlendMode = sGetRenderBlendMode(material ? material->blendMode : Material::Default.blendMode);
            instance.mVertexShader = material ? material->vertexShader : 0;
            instance.mPixelShader = material ? material->pixelShader : 0;
            instance.mBoundsCenter = geometry->bbox.Transformed(transform->worldTransform).GetCenter();
            instance.mWorldTransform = transform->worldTransform;
            instance.mPrevWorldTransform = transform->prevWorldTransform;

            m_Geometries.push_back(RTGeometry
            {
                .mEntity             = entity,
                .mIndexBuffer        = inDevice.GetBindlessHeapIndex(instance.mIndexBuffer),
                .mVertexBuffer       = inDevice.GetBindlessHeapIndex(instance.mVertexBuffer),
                .mMaterialIndex      = instance.mMaterialIndex,
                .mWorldTransform     = instance.mWorldTransform,
                .mPrevWorldTransform = instance.mPrevWorldTransform
            });
        }
    }


    {
        PROFILE_SCOPE_CPU("Extract Skeletons");

        for (const auto& [entity, mesh, skeleton] : inScene.Each<Mesh, Skeleton>())
        {
            const MeshGeometry* geometry = mesh.GetGeometry();

            if (!skeleton.gpuBuffersUploaded || geometry == nullptr || !geometry->IsUploaded())
                continue;

            const Name* name = inScene.GetPtr<Name>(entity);

            m_SkinnedMeshes.push_back(RenderSkinnedMesh
            {
                .mEntity                = entity,
                .mIndexBuffer           = gToBufferID(geometry->indexBuffer),
                .mMeshVertexBuffer      = gToBufferID(geometry->vertexBuffer),
                .mSkinnedVertexBuffer   = gToBufferID(skeleton.skinnedVertexBuffer),
                .mBoneIndexBuffer       = gToBufferID(skeleton.boneIndexBuffer),
                .mBoneWeightBuffer      = gToBufferID(skeleton.boneWeightBuffer),
                .mBoneTransformsBuffer  = gToBufferID(skeleton.boneTransformsBuffer),
                .mBottomLevelAS         = gToBufferID(skeleton.skinnedBottomLevelAS),
                .mScratchBuffer         = gToBufferID(skeleton.blasScratchBuffer),
                .mVertexCount           = uint32_t(geometry->positions.size()),
                .mIndexCount            = uint32_t(geometry->indices.size()),
                .mBoneMatrixOffset      = uint32_t(m_BoneMatrices.size()),
                .mBoneMatrixCount       = uint32_t(skeleton.boneTransformMatrices.size()),
                .mNameOffset            = AddName(name ? name->name.c_str() : nullptr)
            });

            m_BoneMatrices.insert(m_BoneMatrices.end(), skeleton.boneTransformMatrices.begin(), skeleton.boneTransformMatrices.end());
        }
    }

    {
        PROFILE_SCOPE_CPU("Extract Lights");

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
    }

    {
        PROFILE_SCOPE_CPU("Extract DDGI Volume");

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
