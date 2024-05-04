#pragma once

#include "VKPass.h"
#include "VKImGui.h"
#include "VKDevice.h"
#include "VKSwapchain.h"
#include "VKDescriptor.h"

#include "Raekor/scene.h"
#include "Raekor/assets.h"
#include "Raekor/camera.h"
#include "Raekor/components.h"

namespace RK::VK {

class Renderer
{
public:
	Renderer(SDL_Window* window);
	~Renderer();

	void Init(Scene& scene);
	void ResetAccumulation();
	uint32_t GetFrameCounter();

	void UpdateBVH(Scene& scene);
	void UpdateMaterials(Assets& assets, Scene& scene);

	void RenderScene(SDL_Window* window, const Viewport& viewport, Scene& scene);

	void Screenshot(const std::string& path);
	void RecreateSwapchain(SDL_Window* window);

	int32_t UploadTexture(Device& device, const TextureAsset::Ptr& asset, VkFormat format, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA);

	void ReloadShaders();
	void SetSyncInterval(bool interval);

	RTGeometry CreateBLAS(Mesh& mesh, const Material& material);
	void DestroyBLAS(RTGeometry& geometry);
	AccelStruct CreateTLAS(Slice<VkAccelerationStructureInstanceKHR> inInstances);

	PathTracePass::PushConstants& GetPushConstants() { return m_PathTracePass.m_PushConstants; }

private:
	bool m_SyncInterval = false;

	Device m_Device;
	SwapChain m_Swapchain;

	ImGuiPass m_ImGuiPass;
	PathTracePass m_PathTracePass;

	Array<VkFence> m_CommandsFinishedFences;
	Array<VkSemaphore> m_ImageAcquiredSemaphores;
	Array<VkSemaphore> m_RenderFinishedSemaphores;

	Array<Texture> m_Textures;
	Array<Sampler> m_Samplers;
	BindlessDescriptorSet m_BindlessTextureSet;

	uint32_t m_ImageIndex = 0;
	uint32_t m_CurrentFrame = 0;
	static constexpr uint32_t sMaxFramesInFlight = 3;

	AccelStruct m_TLAS;
	Buffer m_InstanceBuffer;
	Buffer m_MaterialBuffer;
};

} // Raekor::VK
