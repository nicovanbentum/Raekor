#pragma once

#include "ECS.h"
#include "RTTI.h"
#include "Assets.h"
#include "Camera.h"
#include "Defines.h"
#include "DiscordRPC.h"

extern std::atomic_uint64_t sAllocationsPerFrame;

namespace RK {

class Mesh;
class Scene;
class Assets;
class Physics;
class IWidget;
class Skeleton;
class Material;
class Application;
class IRenderInterface;

struct ConfigSettings
{
	RTTI_DECLARE_TYPE(ConfigSettings);

	bool mShowUI = true;
	int mDisplayIndex = 0;
	bool mVsyncEnabled = true;
	String mAppName = "RK Renderer";
	Path mFontFile = "Assets/Fonts/Inter-Medium.ttf";
	Path mSceneFile = "";
	Array<Path> mRecentScenes;
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


enum EGameState
{
	GAME_STOPPED = 0,
	GAME_PAUSED,
	GAME_RUNNING
};


enum IPC
{
	LOG_MESSAGE_SENT = 1,
	LOG_MESSAGE_RECEIVED = 2,
};

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

	static int OnNativeEvent(void* inUserData, SDL_Event* inEvent);

	bool IsWindowBorderless() const;
	bool IsWindowExclusiveFullscreen() const;

	virtual Scene* GetScene() { return nullptr; }
	virtual Assets* GetAssets() { return nullptr; }
	virtual Physics* GetPhysics() { return nullptr; }
	virtual IRenderInterface* GetRenderInterface() { return nullptr; }

	virtual void SetActiveEntity(Entity inEntity) {}
	virtual Entity GetActiveEntity() const { return Entity::Null; }

	virtual void LogMessage(const String& inMessage) { std::cout << inMessage << '\n'; }

	void SetGameState(EGameState inState) { m_GameState = inState; }
	EGameState GetGameState() const { return m_GameState; }

	uint64_t GetFrameCounter() const { return m_FrameCounter; }
	const ConfigSettings& GetSettings() const { return m_Settings; }

	void AddRecentScene(const Path& inPath);

	SDL_Window* GetWindow() { return m_Window; }
	const SDL_Window* GetWindow() const { return m_Window; }

	Viewport& GetViewport() { return m_Viewport; }
	const Viewport& GetViewport() const { return m_Viewport; }

	DiscordRPC& GetDiscordRPC() { return m_DiscordRPC; }
	const DiscordRPC& GetDiscordRPC() const { return m_DiscordRPC; }


protected:
	bool m_Running = true;
	EGameState m_GameState = GAME_STOPPED;
	uint64_t m_FrameCounter = 0;
	SDL_Window* m_Window = nullptr;

	Viewport m_Viewport;
	DiscordRPC m_DiscordRPC;
	ConfigSettings m_Settings;
};



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
	struct Settings
	{
		int& vsync			= g_CVariables->Create("r_vsync",			1);
		int& doBloom		= g_CVariables->Create("r_bloom",			0);
		int& paused			= g_CVariables->Create("r_paused",			0);
		int& debugVoxels	= g_CVariables->Create("r_voxelize_debug",	0);
		int& debugCascades	= g_CVariables->Create("r_debug_cascades",	0);
		int& disableTiming	= g_CVariables->Create("r_disable_timings",	0);
		int& shouldVoxelize = g_CVariables->Create("r_voxelize",			0);
		int& enableTAA		= g_CVariables->Create("r_taa",				0);
		int& mDebugTexture	= g_CVariables->Create("r_debug_texture",	0, true);
	} m_Settings;

	Settings& GetSettings() { return m_Settings; }
	GPUStats& GetGPUStats() { return m_GPUStats; }
	const Settings& GetSettings() const { return m_Settings; }
	const GPUStats& GetGPUStats() const { return m_GPUStats; }

	const GPUInfo& GetGPUInfo() const { return m_GPUInfo; }
	void SetGPUInfo(const GPUInfo& inInfo) { m_GPUInfo = inInfo; }

	virtual void SetWhiteTexture(uint32_t inTexture) { m_WhiteTexture = inTexture; }
	virtual void SetBlackTexture(uint32_t inTexture) { m_BlackTexture = inTexture; }
	virtual uint32_t GetWhiteTexture() const { return m_WhiteTexture; }
	virtual uint32_t GetBlackTexture() const { return m_BlackTexture; }
	
	virtual uint64_t GetLightTexture() { return 0; }
	virtual uint64_t GetCameraTexture() { return 0; }
	virtual uint64_t GetDisplayTexture() = 0;

	virtual uint64_t GetDebugTextureIndex() const = 0;
	virtual void SetDebugTextureIndex(int inIndex) = 0;

	virtual uint32_t    GetDebugTextureCount() const = 0;
	virtual const char* GetDebugTextureName(uint32_t inIndex) const = 0;

	/* OpenGL: Does nothing, returns inHandle.
	   DX12: Expects a ResourceID (index into the resource heap). Returns a GPU descriptor handle ptr. */
	virtual uint64_t GetImGuiTextureID(uint32_t inHandle) = 0;

	virtual uint32_t GetScreenshotBuffer(uint8_t* ioBuffer) = 0;
	virtual uint32_t GetSelectedEntity(const Scene& inScene, uint32_t inScreenPosX, uint32_t inScreenPosY) = 0;

	virtual void UploadMeshBuffers(Entity inEntity, Mesh& inMesh) = 0;
	virtual void DestroyMeshBuffers(Entity inEntity, Mesh& inMesh) = 0;

	virtual void CompileMaterialShaders(Entity inEntity, Material& inMaterial) {};
	virtual void ReleaseMaterialShaders(Entity inEntity, Material& inMaterial) {};

	virtual void UploadSkeletonBuffers(Entity inEntity, Skeleton& inSkeleton, Mesh& inMesh) = 0;
	virtual void DestroySkeletonBuffers(Entity inEntity, Skeleton& inSkeleton) = 0;

	virtual void UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets);
	virtual void DestroyMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets) = 0;

	virtual uint32_t UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB = false, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA) = 0;

	virtual void OnResize(const Viewport& inViewport) = 0;
	virtual void DrawDebugSettings(Application* inApp, Scene& inScene, const Viewport& inViewport) = 0;

protected:
	uint32_t	m_WhiteTexture;
	uint32_t	m_BlackTexture;
	GPUInfo		m_GPUInfo;
	GPUStats	m_GPUStats;
};

} // Namespace Raekor