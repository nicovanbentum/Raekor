#include "pch.h"
#include "VKRenderer.h"

#include "VKUtil.h"
#include "VKDevice.h"
#include "VKExtensions.h"

#include "Raekor/assets.h"
#include "Raekor/primitives.h"

namespace RK::VK {

Renderer::Renderer(SDL_Window* window) :
	m_Device(window)
{
	int w, h;
	SDL_GetWindowSize(window, &w, &h);

	VkPresentModeKHR mode = m_SyncInterval ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

	m_Swapchain.Create(m_Device, { w, h }, mode);

	m_ImageAcquiredSemaphores.resize(sMaxFramesInFlight);
	m_RenderFinishedSemaphores.resize(sMaxFramesInFlight);
	m_CommandsFinishedFences.resize(sMaxFramesInFlight);

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (unsigned int i = 0; i < sMaxFramesInFlight; i++)
	{
		vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAcquiredSemaphores[i]);
		vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]);
		vkCreateFence(m_Device, &fenceInfo, nullptr, &m_CommandsFinishedFences[i]);
	}

	m_BindlessTextureSet.Create(m_Device, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}



Renderer::~Renderer()
{
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	vkWaitForFences(m_Device, m_CommandsFinishedFences.size(), m_CommandsFinishedFences.data(), VK_TRUE, UINT64_MAX);
	vkQueueWaitIdle(m_Device.GetQueue());
	vkDeviceWaitIdle(m_Device);

	for (auto& texture : m_Textures)
		m_Device.DestroyTexture(texture);

	for (auto& sampler : m_Samplers)
		m_Device.DestroySampler(sampler);

	m_Device.DestroyAccelStruct(m_TLAS);
	m_Swapchain.Destroy(m_Device);
	m_ImGuiPass.Destroy(m_Device);
	m_PathTracePass.Destroy(m_Device);
	m_BindlessTextureSet.Destroy(m_Device);
	m_Device.DestroyBuffer(m_InstanceBuffer);
	m_Device.DestroyBuffer(m_MaterialBuffer);

	for (uint32_t i = 0; i < sMaxFramesInFlight; i++)
	{
		vkDestroySemaphore(m_Device, m_ImageAcquiredSemaphores[i], nullptr);
		vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_Device, m_CommandsFinishedFences[i], nullptr);
	}
}


void Renderer::Init(Scene& scene)
{
	m_PathTracePass.Init(m_Device, m_Swapchain, m_TLAS, m_InstanceBuffer, m_MaterialBuffer, m_BindlessTextureSet);
	m_ImGuiPass.Init(m_Device, m_Swapchain, m_PathTracePass, m_BindlessTextureSet);
}


void Renderer::ResetAccumulation()
{
	m_PathTracePass.m_PushConstants.frameCounter = 0;
}

uint32_t Renderer::GetFrameCounter()
{
	return m_PathTracePass.m_PushConstants.frameCounter;
}



void Renderer::UpdateMaterials(Assets& assets, Scene& scene)
{
	std::vector<RTMaterial> materials(scene.Count<Material>());

	uint32_t i = 0;
	for (const auto& [entity, material] : scene.Each<Material>())
	{
		auto& buffer = materials[i];
		buffer.albedo = material.albedo;

		buffer.textures.x  = UploadTexture(m_Device, assets.GetAsset<TextureAsset>(material.albedoFile), VK_FORMAT_BC3_SRGB_BLOCK, material.gpuAlbedoMapSwizzle);
		buffer.textures.y  = UploadTexture(m_Device, assets.GetAsset<TextureAsset>(material.normalFile), VK_FORMAT_BC3_UNORM_BLOCK, material.gpuNormalMapSwizzle);
		buffer.textures2.x = UploadTexture(m_Device, assets.GetAsset<TextureAsset>(material.emissiveFile), VK_FORMAT_BC3_UNORM_BLOCK, material.gpuEmissiveMapSwizzle);
		buffer.textures.z  = UploadTexture(m_Device, assets.GetAsset<TextureAsset>(material.metallicFile), VK_FORMAT_BC3_UNORM_BLOCK, material.gpuMetallicMapSwizzle);
		buffer.textures.w  = UploadTexture(m_Device, assets.GetAsset<TextureAsset>(material.roughnessFile), VK_FORMAT_BC3_UNORM_BLOCK, material.gpuRoughnessMapSwizzle);

		buffer.properties.x = material.metallic;
		buffer.properties.y = material.roughness;
		buffer.properties.z = 1.0f;

		buffer.emissive = glm::vec4(material.emissive, 1.0f);

		i++;
	}

	const auto materialBufferSize = materials.size() * sizeof(materials[0]);

	m_MaterialBuffer = m_Device.CreateBuffer(
		materialBufferSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	memcpy(m_Device.GetMappedPointer<void*>(m_MaterialBuffer), materials.data(), materialBufferSize);
}


void Renderer::UpdateBVH(Scene& scene)
{
	const auto nr_of_meshes = scene.Count<VK::RTGeometry>();
	if (nr_of_meshes < 1)
		return;

	std::vector<VkAccelerationStructureInstanceKHR> deviceInstances;
	deviceInstances.reserve(nr_of_meshes);

	uint32_t customIndex = 0;
	for (const auto& [entity, mesh, transform, geometry] : scene.Each<Mesh, Transform, RTGeometry>())
	{
		auto deviceAddress = m_Device.GetDeviceAddress(geometry.accelStruct.accelerationStructure);

		VkAccelerationStructureInstanceKHR instance = {};
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.mask = 0xFF;
		instance.instanceCustomIndex = customIndex++;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.accelerationStructureReference = deviceAddress;
		instance.transform = gVkTransformMatrix(transform.worldTransform);

		deviceInstances.push_back(instance);
	}

	m_Device.DestroyAccelStruct(m_TLAS);
	m_TLAS = CreateTLAS(deviceInstances);

	struct Instance
	{
		glm::ivec4 materialIndex;
		glm::mat4 localToWorldTransform;
		VkDeviceAddress indexBufferAddress;
		VkDeviceAddress vertexBufferAddress;
	};

	std::vector<Instance> hostInstances;
	hostInstances.reserve(nr_of_meshes);

	auto materialIndex = [&](Entity m) -> int32_t
	{
		int32_t i = 0;

		for (const auto& [entity, material] : scene.Each<Material>())
		{
			if (entity == m)
				return i;

			i++;
		}

		return 0;
	};

	for (const auto& [entity, mesh, transform, geometry] : scene.Each<Mesh, Transform, RTGeometry>())
	{
		if (mesh.material == Entity::Null)
			continue;

		Instance instance = {};
		instance.materialIndex.x = materialIndex(mesh.material);
		instance.localToWorldTransform = transform.worldTransform;
		instance.indexBufferAddress = m_Device.GetDeviceAddress(geometry.indices);
		instance.vertexBufferAddress = m_Device.GetDeviceAddress(geometry.vertices);

		hostInstances.push_back(instance);
	}

	m_InstanceBuffer = m_Device.CreateBuffer(
		sizeof(Instance) * nr_of_meshes,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	auto mappedPtr = m_Device.GetMappedPointer<void*>(m_InstanceBuffer);
	memcpy(mappedPtr, hostInstances.data(), hostInstances.size() * sizeof(Instance));
}


void Renderer::RenderScene(SDL_Window* window, const Viewport& viewport, Scene& scene)
{
	m_CurrentFrame = ( m_CurrentFrame + 1 ) % sMaxFramesInFlight;
	vkWaitForFences(m_Device, 1, &m_CommandsFinishedFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(m_Device, 1, &m_CommandsFinishedFences[m_CurrentFrame]);

	VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_ImageAcquiredSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_ImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain(window);
		return;
	}

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	auto commandBuffer = m_Swapchain.submitBuffers[m_CurrentFrame];

	gThrowIfFailed(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	{
		VkImageMemoryBarrier2KHR barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR |
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		VkDependencyInfoKHR dep = {};
		dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &barrier;

		barrier.image = m_PathTracePass.finalTexture.image;
		EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);

		barrier.image = m_PathTracePass.accumTexture.image;
		EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);
	}

	m_PathTracePass.Record(m_Device, viewport, commandBuffer, m_BindlessTextureSet);

	{
		VkImageMemoryBarrier2KHR barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
		barrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
		barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.image = m_PathTracePass.finalTexture.image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		VkDependencyInfoKHR dep = {};
		dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &barrier;

		EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);
	}

	m_ImGuiPass.Record(m_Device, commandBuffer, ImGui::GetDrawData(), m_BindlessTextureSet, viewport.GetRenderSize().x, viewport.GetRenderSize().y, m_PathTracePass);

	{
		VkImageMemoryBarrier2KHR barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
		barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.image = m_PathTracePass.finalTexture.image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		VkDependencyInfoKHR dep = {};
		dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &barrier;

		EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);
	}

	{
		VkImageMemoryBarrier2KHR barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.image = m_Swapchain.images[m_ImageIndex];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		VkDependencyInfoKHR dep = {};
		dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &barrier;

		EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);
	}

	VkImageBlit region = {};

	region.srcOffsets[1] = { int32_t(viewport.GetRenderSize().x), int32_t(viewport.GetRenderSize().y), 1 };
	region.srcSubresource.layerCount = 1;
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	region.dstOffsets[1] = { int32_t(viewport.GetRenderSize().x), int32_t(viewport.GetRenderSize().y), 1 };
	region.dstSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	vkCmdBlitImage(commandBuffer, m_PathTracePass.finalTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Swapchain.images[m_ImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);

	{
		VkImageMemoryBarrier2KHR barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
		barrier.image = m_Swapchain.images[m_ImageIndex];
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		VkDependencyInfoKHR dep = {};
		dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &barrier;

		EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);
	}

	gThrowIfFailed(vkEndCommandBuffer(commandBuffer));

	VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR; // For now

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_ImageAcquiredSemaphores[m_CurrentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_RenderFinishedSemaphores[m_ImageIndex];
	submitInfo.pWaitDstStageMask = &waitStages;

	gThrowIfFailed(vkQueueSubmit(m_Device.GetQueue(), 1, &submitInfo, m_CommandsFinishedFences[m_CurrentFrame]));

	VkSwapchainKHR swapChains[] = { m_Swapchain };

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_ImageIndex;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_ImageIndex];

	result = vkQueuePresentKHR(m_Device.GetQueue(), &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		RecreateSwapchain(window);
	else
		gThrowIfFailed(result);
}


void Renderer::Screenshot(const std::string& filepath)
{
	Texture::Desc desc;
	desc.width = m_Swapchain.extent.width;
	desc.height = m_Swapchain.extent.height;
	desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	desc.mappable = true;

	auto linearTexture = m_Device.CreateTexture(desc);
	m_Device.SetDebugName(linearTexture, "SCREENSHOT_IMAGE");

	auto commands = m_Device.StartSingleSubmit();

	{
		VkImageMemoryBarrier2KHR srcBarrier = {};
		srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
		srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
		srcBarrier.image = m_Swapchain.images[m_ImageIndex];
		srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		srcBarrier.subresourceRange.levelCount = 1;
		srcBarrier.subresourceRange.layerCount = 1;

		VkImageMemoryBarrier2KHR dstBarrier = srcBarrier;
		dstBarrier.image = linearTexture.image;
		dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		std::array barriers = { srcBarrier, dstBarrier };

		VkDependencyInfoKHR dep = {};
		dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
		dep.imageMemoryBarrierCount = barriers.size();
		dep.pImageMemoryBarriers = barriers.data();

		EXT::vkCmdPipelineBarrier2KHR(commands, &dep);
	}

	VkImageCopy copy = {};
	copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.srcSubresource.layerCount = 1;
	copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.dstSubresource.layerCount = 1;
	copy.extent.width = desc.width;
	copy.extent.height = desc.height;
	copy.extent.depth = 1;

	vkCmdCopyImage(
		commands,
		m_Swapchain.images[m_ImageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		linearTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copy
	);

	m_Device.FlushSingleSubmit(commands);

	auto pixels = static_cast<uint8_t*>( m_Device.MapPointer(linearTexture) );

	// BGR -> RGB channel flip
	for (uint64_t i = 0; i < desc.width * desc.height * 4; i += 4)
	{
		uint8_t temp = pixels[i];
		pixels[i] = pixels[i + 2];
		pixels[i + 2] = temp;
	}

	// Apparently the rows are 32 byte aligned, 10/10 guess work
	const int strideInBytes = gAlignUp(desc.width * 4, 32);

	if (!stbi_write_png(filepath.c_str(), desc.width, desc.height, 4, pixels, strideInBytes))
		std::cerr << "Failed to save screenshot. \n";

	m_Device.UnmapPointer(linearTexture);
	m_Device.DestroyTexture(linearTexture);
}


void Renderer::RecreateSwapchain(SDL_Window* window)
{
	vkQueueWaitIdle(m_Device.GetQueue());

	uint32_t flags = SDL_GetWindowFlags(window);
	while (flags & SDL_WINDOW_MINIMIZED)
	{
		flags = SDL_GetWindowFlags(window);
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {}
	}

	VkPresentModeKHR mode = m_SyncInterval ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

	int w, h;
	SDL_GetWindowSize(window, &w, &h);

	m_Swapchain.Destroy(m_Device);
	m_Swapchain.Create(m_Device, { w, h }, mode);

	m_PathTracePass.DestroyRenderTargets(m_Device);
	m_PathTracePass.CreateRenderTargets(m_Device, glm::uvec2(w, h));
	m_PathTracePass.UpdateDescriptorSet(m_Device, m_TLAS, m_InstanceBuffer, m_MaterialBuffer);

	m_ImGuiPass.DestroyFramebuffer(m_Device);
	m_ImGuiPass.CreateFramebuffer(m_Device, m_PathTracePass, uint32_t(w), uint32_t(h));
}


int32_t Renderer::UploadTexture(Device& device, const TextureAsset::Ptr& asset, VkFormat format, uint8_t inSwizzle)
{
	if (!asset)
		return -1;

	Texture::Desc desc;
	desc.format = format;
	desc.width = asset->GetHeader()->dwWidth;
	desc.height = asset->GetHeader()->dwHeight;
	desc.mipLevels = asset->GetHeader()->dwMipMapCount;

	auto texture = device.CreateTexture(desc);
	auto view = device.CreateView(texture, inSwizzle);

	Sampler::Desc samplerDesc;
	auto sampler = m_Samplers.emplace_back(device.CreateSampler(samplerDesc));

	auto buffer = device.CreateBuffer(
		gAlignUp(asset->GetDataSize(), 16), // the disk size is not aligned to 16
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY
	);

	auto mappedPtr = device.GetMappedPointer<void*>(buffer);
	memcpy(mappedPtr, asset->GetData(), asset->GetDataSize());

	device.TransitionImageLayout(
		texture,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	std::vector<VkBufferImageCopy> regions(desc.mipLevels);

	uint32_t mip = 0, bufferOffset = 0;

	for (auto& region : regions)
	{
		glm::uvec2 mipDimensions = {
			std::max(desc.width >> mip, 1u),
			std::max(desc.height >> mip, 1u)
		};

		region.bufferOffset = bufferOffset;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = mip++;
		region.imageSubresource.layerCount = 1;

		region.imageExtent = { mipDimensions.x, mipDimensions.y, 1 };

		bufferOffset += mipDimensions.x * mipDimensions.y;
	}

	auto commandBuffer = device.StartSingleSubmit();

	vkCmdCopyBufferToImage(
		commandBuffer, buffer.buffer, texture.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		uint32_t(regions.size()), regions.data()
	);

	device.FlushSingleSubmit(commandBuffer);

	device.TransitionImageLayout(
		texture,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	device.DestroyBuffer(buffer);
	m_Textures.push_back(texture);

	VkDescriptorImageInfo descriptorInfo = {};
	descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorInfo.imageView = view;
	descriptorInfo.sampler = sampler.sampler;

	return m_BindlessTextureSet.Insert(device, descriptorInfo);
}


void Renderer::ReloadShaders()
{
	m_PathTracePass.ReloadShaders(m_Device);
}


void Renderer::SetSyncInterval(bool on)
{
	m_SyncInterval = on;
}


RTGeometry Renderer::CreateBLAS(Mesh& mesh, const Material& material)
{
	const auto& vertices = mesh.GetInterleavedVertices();
	const auto sizeOfVertexBuffer = vertices.size() * sizeof(vertices[0]);
	const auto sizeOfIndexBuffer = mesh.indices.size() * sizeof(mesh.indices[0]);

	auto vertexStageBuffer = m_Device.CreateBuffer(sizeOfVertexBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	auto indexStageBuffer = m_Device.CreateBuffer(sizeOfIndexBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	memcpy(m_Device.GetMappedPointer<void*>(vertexStageBuffer), vertices.data(), sizeOfVertexBuffer);
	memcpy(m_Device.GetMappedPointer<void*>(indexStageBuffer), mesh.indices.data(), sizeOfIndexBuffer);

	constexpr auto bufferUsages = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	RTGeometry component = {};

	component.vertices = m_Device.CreateBuffer(
		sizeOfVertexBuffer,
		bufferUsages | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);

	component.indices = m_Device.CreateBuffer(
		sizeOfIndexBuffer,
		bufferUsages | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);

	VkCommandBuffer commandBuffer = m_Device.StartSingleSubmit();

	VkBufferCopy vertexRegion = {};
	vertexRegion.size = sizeOfVertexBuffer;
	vkCmdCopyBuffer(commandBuffer, vertexStageBuffer.buffer, component.vertices.buffer, 1, &vertexRegion);

	VkBufferCopy indexRegion = {};
	indexRegion.size = sizeOfIndexBuffer;
	vkCmdCopyBuffer(commandBuffer, indexStageBuffer.buffer, component.indices.buffer, 1, &indexRegion);

	m_Device.FlushSingleSubmit(commandBuffer);

	m_Device.DestroyBuffer(indexStageBuffer);
	m_Device.DestroyBuffer(vertexStageBuffer);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles = {};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.maxVertex = uint32_t(mesh.positions.size());
	triangles.vertexStride = uint32_t(sizeof(Vertex));
	triangles.indexData.deviceAddress = m_Device.GetDeviceAddress(component.indices);
	triangles.vertexData.deviceAddress = m_Device.GetDeviceAddress(component.vertices);

	VkAccelerationStructureGeometryKHR geometry = {};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles = triangles;

	if (material.isTransparent)
		geometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;

	component.accelStruct = m_Device.CreateAccelStruct(buildInfo, uint32_t(mesh.indices.size() / 3u));

	return component;
}


void Renderer::DestroyBLAS(RTGeometry& geometry)
{
	m_Device.DestroyBuffer(geometry.vertices);
	m_Device.DestroyBuffer(geometry.indices);
	m_Device.DestroyAccelStruct(geometry.accelStruct);
}


AccelStruct Renderer::CreateTLAS(Slice<VkAccelerationStructureInstanceKHR> inInstances)
{
	const size_t bufferSize = sizeof(VkAccelerationStructureInstanceKHR) * inInstances.Length();
	assert(bufferSize != 0);

	auto buffer = m_Device.CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	memcpy(m_Device.GetMappedPointer<void*>(buffer), &inInstances[0], bufferSize);

	VkAccelerationStructureGeometryInstancesDataKHR instanceData = {};
	instanceData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instanceData.data.deviceAddress = m_Device.GetDeviceAddress(buffer);

	VkAccelerationStructureGeometryKHR geometry = {};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances = instanceData;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;

	auto tlas = m_Device.CreateAccelStruct(buildInfo, uint32_t(inInstances.Length()));

	m_Device.DestroyBuffer(buffer);

	return tlas;
}

} // raekor