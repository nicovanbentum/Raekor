#pragma once

#include "ecs.h"
#include "rtti.h"
#include "assets.h"
#include "camera.h"

extern std::atomic_uint64_t sAllocationsPerFrame;

namespace Raekor {

class Mesh;
class Scene;
class Assets;
class Physics;
class IWidget;
class Skeleton;
class Material;
class Application;

struct GPUInfo
{
	std::string mVendor;
	std::string mProduct;
	std::string mActiveAPI;
};

enum GraphicsAPI : uint8_t
{
	OpenGL4_6 = 0b0001,
	Vulkan = 0b0010,
	DirectX11 = 0b0100,
	DirectX12 = 0b1000,

	DirectX = DirectX11 | DirectX12
};

struct GPUStats
{
	uint64_t mTotalVideoMemory = 0;
	uint64_t mAvailableVideoMemory = 0;
	std::atomic<uint64_t> mLiveBuffers = 0;
	std::atomic<uint64_t> mLiveTextures = 0;

	std::atomic<uint64_t> mLiveRTVHeap = 0;
	std::atomic<uint64_t> mLiveDSVHeap = 0;
	std::atomic<uint64_t> mLiveSamplerHeap = 0;
	std::atomic<uint64_t> mLiveResourceHeap = 0;

};

class IRenderInterface
{
public:
	struct Settings
	{
		int& vsync			= g_CVars.Create("r_vsync",				1);
		int& doBloom		= g_CVars.Create("r_bloom",				0);
		int& paused			= g_CVars.Create("r_paused",			0);
		int& debugVoxels	= g_CVars.Create("r_voxelize_debug",	0);
		int& debugCascades	= g_CVars.Create("r_debug_cascades",	0);
		int& disableTiming	= g_CVars.Create("r_disable_timings",	0);
		int& shouldVoxelize = g_CVars.Create("r_voxelize",			1);
		int& enableTAA		= g_CVars.Create("r_taa",				0);
		int& mDebugTexture	= g_CVars.Create("r_debug_texture",		0, true);
	} mSettings;

	IRenderInterface(GraphicsAPI inAPI) : m_GraphicsAPI(inAPI) {}

	GraphicsAPI GetGraphicsAPI() const { return m_GraphicsAPI; }

	Settings& GetSettings() { return mSettings; }
	GPUStats& GetGPUStats() { return m_GPUStats; }

	const GPUInfo& GetGPUInfo() const { return m_GPUInfo; }
	void SetGPUInfo(const GPUInfo& inInfo) { m_GPUInfo = inInfo; }

	virtual uint64_t GetDisplayTexture() = 0;

	virtual uint32_t    GetDebugTextureCount() const = 0;
	virtual const char* GetDebugTextureName(uint32_t inIndex) const = 0;

	/* OpenGL: Does nothing, returns inHandle.
	   DX12: Expects a ResourceID (index into the resource heap). Returns a GPU descriptor handle ptr. */
	virtual uint64_t GetImGuiTextureID(uint32_t inHandle) = 0;

	virtual uint32_t GetScreenshotBuffer(uint8_t* ioBuffer) = 0;
	virtual uint32_t GetSelectedEntity(uint32_t inScreenPosX, uint32_t inScreenPosY) = 0;

	virtual void UploadMeshBuffers(Entity inEntity, Mesh& inMesh) = 0;
	virtual void DestroyMeshBuffers(Entity inEntity, Mesh& inMesh) = 0;

	virtual void UploadSkeletonBuffers(Skeleton& inSkeleton, Mesh& inMesh) = 0;
	virtual void DestroySkeletonBuffers(Skeleton& inSkeleton) = 0;

	/* Implementation resides in Systems.cpp to avoid conflicts. */
	virtual void UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets);
	virtual void DestroyMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets) = 0;

	virtual uint32_t UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB = false, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA) = 0;

	virtual void OnResize(const Viewport& inViewport) = 0;
	virtual void DrawImGui(Scene& inScene, const Viewport& inViewport) = 0;

	virtual void AddDebugLine(Vec3 inP1, Vec3 inP2) {}
	virtual void AddDebugBox(Vec3 inMin, Vec3 inMax, const Mat4x4& inTransform = Mat4x4(1.0f)) {}

protected:
	GPUInfo m_GPUInfo;
	GPUStats m_GPUStats;
	GraphicsAPI m_GraphicsAPI;
};


struct ConfigSettings
{
	RTTI_DECLARE_TYPE(ConfigSettings);

	bool mShowUI = true;
	int mDisplayIndex = 0;
	bool mVsyncEnabled = true;
	std::string mAppName = "";
	Path mFontFile = "";
	Path mSceneFile = "";
	std::vector<Path> mRecentScenes;
};

enum WindowFlag
{
	NONE = 0,
	HIDDEN = SDL_WINDOW_HIDDEN,
	RESIZE = SDL_WINDOW_RESIZABLE,
	OPENGL = SDL_WINDOW_OPENGL,
	VULKAN = SDL_WINDOW_VULKAN,
	BORDERLESS = SDL_WINDOW_BORDERLESS,
};
using WindowFlags = uint32_t;


class Application
{
protected:
	Application(WindowFlags inFlags);

public:
	virtual ~Application();

	void Run();
	void Terminate() { m_Running = false; }

	virtual void OnUpdate(float dt) = 0;
	virtual void OnEvent(const SDL_Event& event) = 0;

	virtual Scene* GetScene() { return nullptr; }
	virtual Assets* GetAssets() { return nullptr; }
	virtual Physics* GetPhysics() { return nullptr; }
	virtual IRenderInterface* GetRenderInterface() { return nullptr; }

	virtual void SetActiveEntity(Entity inEntity) {}
	virtual Entity GetActiveEntity() { return NULL_ENTITY; }

	virtual void LogMessage(const std::string& inMessage) { std::cout << inMessage << '\n'; }

	uint64_t GetFrameCounter() const { return m_FrameCounter; }
	const ConfigSettings& GetSettings() const { return m_Settings; }

	void AddRecentScene(const Path& inPath);

	SDL_Window* GetWindow() { return m_Window; }
	const SDL_Window* GetWindow() const { return m_Window; }

	Viewport& GetViewport() { return m_Viewport; }
	const Viewport& GetViewport() const { return m_Viewport; }

protected:
	bool m_Running = true;
	uint64_t m_FrameCounter = 0;
	SDL_Window* m_Window = nullptr;

	Viewport m_Viewport;
	ConfigSettings m_Settings;
};

} // Namespace Raekor