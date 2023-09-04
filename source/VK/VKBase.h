#pragma once

#include "Raekor/util.h"

namespace Raekor::VK {

class Instance
{
	friend class Device;

public:
	Instance() = delete;
	explicit Instance(SDL_Window* window);
	~Instance();

	operator VkInstance() const { return m_Instance; }
	VkSurfaceKHR GetSurface() const { return m_Surface; }

private:
	VkInstance m_Instance = VK_NULL_HANDLE;
	VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
	PFN_vkSetDebugUtilsObjectNameEXT m_VkSetDebugUtilsObjectNameEXT = nullptr;
};



class PhysicalDevice : public INoCopyNoMove
{
	friend class Device;

public:
	PhysicalDevice() = delete;
	explicit PhysicalDevice(const Instance& instance);

	operator VkPhysicalDevice() const { return m_PhysicalDevice; }
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

private:
	struct Properties
	{
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
		};
		VkPhysicalDeviceDescriptorIndexingProperties descriptorIndexingProperties = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES
		};
	};

	Properties m_Properties;
	VkPhysicalDeviceLimits m_Limits;
	VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
};

} // raekor