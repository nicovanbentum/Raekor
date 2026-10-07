#pragma once

#include "ECS.h"
#include "Shared.h"
#include "Resource.h"

namespace RK {

class Scene;

}

namespace RK::DX12 {

class Device;


enum ERenderBlendMode : uint8_t
{
    RENDER_BLEND_MODE_OPAQUE,
    RENDER_BLEND_MODE_MASKED,
    RENDER_BLEND_MODE_TRANSPARENT
};


struct RenderInstance
{
    Entity   mEntity = Entity::Null;
    BufferID mIndexBuffer;
    BufferID mVertexBuffer;
    BufferID mBottomLevelAS;
    uint32_t mIndexCount = 0;
    uint32_t mMaterialIndex = 0;
    uint32_t mNameOffset = 0;
    uint64_t mVertexShader = 0;
    uint64_t mPixelShader = 0;
    ERenderBlendMode mBlendMode = RENDER_BLEND_MODE_OPAQUE;
    Vec3     mBoundsCenter = Vec3(0.0f);
    Mat4x4   mWorldTransform = Mat4x4(1.0f);
    Mat4x4   mPrevWorldTransform = Mat4x4(1.0f);

    bool HasCustomShaders() const { return mVertexShader != 0 || mPixelShader != 0; }
    bool IsRayTraced() const { return !HasCustomShaders() && mBlendMode != RENDER_BLEND_MODE_TRANSPARENT; }
};


struct RenderSkinnedMesh
{
    Entity   mEntity = Entity::Null;
    BufferID mIndexBuffer;
    BufferID mMeshVertexBuffer;
    BufferID mSkinnedVertexBuffer;
    BufferID mBoneIndexBuffer;
    BufferID mBoneWeightBuffer;
    BufferID mBoneTransformsBuffer;
    BufferID mBottomLevelAS;
    BufferID mScratchBuffer;
    uint32_t mVertexCount = 0;
    uint32_t mIndexCount = 0;
    uint32_t mBoneMatrixOffset = 0;
    uint32_t mBoneMatrixCount = 0;
    uint32_t mNameOffset = 0;
};


struct RenderSun
{
    bool      mEnabled = false;
    Vec3      mDirection = Vec3(0.0f, -1.0f, 0.0f);
    Vec4      mSkyDirection = Vec4(0.0f, -1.0f, 0.0f, 0.0f);
    Vec4      mColor = Vec4(0.0f);
    TextureID mCubeMap;
};


struct RenderDDGIVolume
{
    bool  mEnabled = false;
    IVec3 mProbeCount = IVec3(1);
    Vec3  mProbeSpacing = Vec3(1.0f);
    Vec3  mCornerPosition = Vec3(0.0f);
    bool  mFollowCamera = false;
    int   mCascadeCount = 1;
};


class RenderWorld
{
public:
    void Extract(const Scene& inScene, const Device& inDevice, float inExposure, bool inDisableAlbedo);

    Slice<const RenderInstance> GetInstances() const { return m_Instances; }
    Slice<const RenderSkinnedMesh> GetSkinnedMeshes() const { return m_SkinnedMeshes; }
    Slice<const Mat4x4> GetBoneMatrices() const { return m_BoneMatrices; }

    Slice<const RTGeometry> GetGeometries() const { return m_Geometries; }
    Slice<const RTMaterial> GetMaterials() const { return m_Materials; }
    Slice<const RTLight> GetLights() const { return m_Lights; }

    const RenderSun& GetSun() const { return m_Sun; }
    const RenderDDGIVolume& GetDDGIVolume() const { return m_DDGIVolume; }

    const char* GetName(uint32_t inOffset) const { return m_Names.data() + inOffset; }

private:
    uint32_t AddName(const char* inName);

    Array<RenderInstance> m_Instances;
    Array<RenderSkinnedMesh> m_SkinnedMeshes;
    Array<Mat4x4> m_BoneMatrices;
    Array<RTGeometry> m_Geometries;
    Array<RTMaterial> m_Materials;
    Array<RTLight> m_Lights;
    Array<char> m_Names;
    RenderSun m_Sun;
    RenderDDGIVolume m_DDGIVolume;
};

} // namespace RK::DX12
