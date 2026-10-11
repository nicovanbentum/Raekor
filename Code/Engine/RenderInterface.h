#pragma once

#include "ECS.h"
#include "Assets.h"
#include "Defines.h"

namespace RK {

class Application;

struct Mesh;
struct Material;
struct Skeleton;


struct GPUInfo
{
	String mVendor;
	String mProduct;
	String mActiveAPI;
};


struct GPUStats
{
	uint64_t mFrameCounter = 0;
	uint64_t mTotalVideoMemory = 0;
	uint64_t mAvailableVideoMemory = 0;
	Atomic<uint64_t> mLiveBuffers = 0;
	Atomic<uint64_t> mLiveTextures = 0;

	Atomic<uint64_t> mLiveRTVHeap = 0;
	Atomic<uint64_t> mLiveDSVHeap = 0;
	Atomic<uint64_t> mLiveSamplerHeap = 0;
	Atomic<uint64_t> mLiveResourceHeap = 0;
};


class IRenderInterface
{
public:
	virtual ~IRenderInterface() = default;

	const GPUInfo& GetGPUInfo() const { return m_GPUInfo; }
	const GPUStats& GetGPUStats() const { return m_GPUStats; }

	virtual uint32_t GetWhiteTexture() const = 0;
	virtual uint32_t GetBlackTexture() const = 0;

	virtual uint64_t GetLightTexture() = 0;
	virtual uint64_t GetCameraTexture() = 0;
	virtual uint64_t GetDisplayTexture() = 0;
	virtual uint64_t GetImGuiTextureID(uint32_t inTexture) = 0;

	virtual uint32_t GetDebugTextureIndex() const = 0;
	virtual void SetDebugTextureIndex(uint32_t inIndex) = 0;
	virtual uint32_t GetDebugTextureCount() const = 0;
	virtual const char* GetDebugTextureName(uint32_t inIndex) const = 0;

	virtual void RequestScreenshot(const Path& inFile) = 0;

	virtual uint64_t RequestEntityPick(uint32_t inPixelX, uint32_t inPixelY) = 0;
	virtual bool GetEntityPickResult(uint64_t inRequestID, Entity& outEntity) = 0;

	virtual void UploadMeshBuffers(Entity inEntity, Mesh& inMesh) = 0;
	virtual void DestroyMeshBuffers(Entity inEntity, Mesh& inMesh) = 0;

	virtual void UploadSkeletonBuffers(Entity inEntity, Skeleton& inSkeleton, Mesh& inMesh) = 0;
	virtual void DestroySkeletonBuffers(Entity inEntity, Skeleton& inSkeleton) = 0;

	virtual void UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets);
	virtual void DestroyMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets) {}

	virtual void CompileMaterialShaders(Entity inEntity, Material& inMaterial) {}
	virtual void ReleaseMaterialShaders(Entity inEntity, Material& inMaterial) {}

	virtual uint32_t UploadTextureFromAsset(TextureAsset::Ptr inAsset, bool inIsSRGB = false, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA) = 0;

	virtual void DrawDebugSettings(Application* inApp) = 0;

protected:
	GPUInfo m_GPUInfo;
	GPUStats m_GPUStats;
};

} // namespace RK
