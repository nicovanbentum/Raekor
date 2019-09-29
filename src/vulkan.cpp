#include "pch.h"
#include "app.h"
#include "util.h"
#include "timer.h"
#include "entry.h"
#include "camera.h"
#include "PlatformContext.h"

namespace Raekor {

void Application::vulkan_main() {
    auto context = Raekor::PlatformContext();

    // retrieve the application settings from the config file
    serialize_settings("config.json");

    m_assert(SDL_Init(SDL_INIT_VIDEO) == 0, "failed to init sdl");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    auto index = display > displays.size() - 1 ? 0 : display;
    auto window = SDL_CreateWindow(name.c_str(),
        displays[index].x,
        displays[index].y,
        displays[index].w,
        displays[index].h,
        wflags);

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    Camera camera = Camera({ 0, 0, 0 }, 45.0f);

    uint32_t vk_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, nullptr);
    std::cout << vk_extension_count << '\n';

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &appInfo;

    unsigned int count;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr)) {
        std::cout << "failed to get instance extensions" << std::endl;
    }

    std::vector<const char*> extensions = {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME // Sample additional extension
    };
    size_t additional_extension_count = extensions.size();
    extensions.resize(additional_extension_count + count);

    m_assert(SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data() + additional_extension_count),
        "failed to get vk extensions");

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
    };

    if (enableValidationLayers) {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool found = false;
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    found = true;
                    break;
                }
            }
            m_assert(found, "requested a vulkan validation layer that is not supported");
        }
    }

    // Now we can make the Vulkan instance
    instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instance_info.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        instance_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        instance_info.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        instance_info.enabledLayerCount = 0;
    }

    VkInstance instance;
    VkResult result = vkCreateInstance(&instance_info, nullptr, &instance);
    m_assert(result == VK_SUCCESS, "failed to create vulkan instance");

    // query SDL's machine info for the window's HWND
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(window, &wminfo);
    auto vk_hwnd = wminfo.info.win.window;

    VkSurfaceKHR vk_surface;
    bool res = SDL_Vulkan_CreateSurface(window, instance, &vk_surface);
    m_assert(res, "failed to create vulkan surface");

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    m_assert(deviceCount != 0, "failed to find GPUs with vulkan support");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // prefer dedicated GPU
    VkPhysicalDevice vk_gpu = VK_NULL_HANDLE;
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(device, &props);
        vkGetPhysicalDeviceFeatures(device, &features);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            vk_gpu = device;
            break;
        }
    }
    // else we just get the first adapter found
    // TODO: implement actual device picking and scoring
    if (!vk_gpu) vk_gpu = *devices.begin();

    const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(vk_gpu, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(vk_gpu, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    struct queue_indices {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        bool isComplete() {
            return graphics.has_value() && present.has_value();
        }
    };

    queue_indices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk_gpu, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vk_gpu, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        } 
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(vk_gpu, i, vk_surface, &presentSupport);
        if (queueFamily.queueCount > 0 && presentSupport) {
            indices.present = i;
        }
        i++;
    }

    m_assert(indices.isComplete(), "device does not support graphics queue family");
    m_assert(requiredExtensions.empty(), "vk extensions not supported");

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphics.value();
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDevice vk_device;
    VkQueue present_queue;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphics.value(), indices.present.value() };

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    device_info.pQueueCreateInfos = queueCreateInfos.data();
    device_info.pEnabledFeatures = &deviceFeatures;
    device_info.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    device_info.ppEnabledExtensionNames = deviceExtensions.data();


    if (enableValidationLayers) {
        device_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        device_info.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        device_info.enabledLayerCount = 0;
    }

    result = vkCreateDevice(vk_gpu, &device_info, nullptr, &vk_device);
    m_assert(result == VK_SUCCESS, "failed to create logical device");

    VkQueue graphics_queue;
    vkGetDeviceQueue(vk_device, indices.graphics.value(), 0, &graphics_queue);
    vkGetDeviceQueue(vk_device, indices.present.value(), 0, &present_queue);

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_gpu, vk_surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_gpu, vk_surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk_gpu, vk_surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vk_gpu, vk_surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk_gpu, vk_surface, &presentModeCount, details.presentModes.data());
    }

    bool swapChainAdequate = false;
    if (extensions.empty()) {
        swapChainAdequate = !details.formats.empty() && !details.presentModes.empty();
    }

    const std::vector<VkSurfaceFormatKHR> availableFormats = details.formats;
    const std::vector<VkPresentModeKHR> availablePresentModes = details.presentModes;
    const VkSurfaceCapabilitiesKHR capabilities = details.capabilities;
    //TODO: fill the vector
    VkSurfaceFormatKHR surface_format = availableFormats[0];
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D swap_extent = {};

    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = availableFormat;
        }
    }

    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = availablePresentMode;
        }
    }

    if (capabilities.currentExtent.width != UINT32_MAX) {
        swap_extent = capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = { displays[index].w, displays[index].h };
        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
        swap_extent = actualExtent;
    }
    uint32_t imageCount = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
        imageCount = details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vk_surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surface_format.format;
    createInfo.imageColorSpace = surface_format.colorSpace;
    createInfo.imageExtent = swap_extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { indices.graphics.value(), indices.present.value() };

    if (indices.graphics != indices.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.preTransform = details.capabilities.currentTransform;
    createInfo.presentMode = present_mode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR vk_swapchain;
    result = vkCreateSwapchainKHR(vk_device, &createInfo, nullptr, &vk_swapchain);
    m_assert(result == VK_SUCCESS, "failed to create vulkan swap chain");

    std::puts("job well done.");

    SDL_SetWindowInputFocus(window);

    bool running = true;
    
    Timer dt_timer;
    double dt = 0;
    //main application loop
    while (running) {
        dt_timer.start();
        //handle sdl and imgui events
        handle_sdl_gui_events({ window }, camera, dt);


        dt_timer.stop();
        dt = dt_timer.elapsed_ms();
    }

    vkDestroyInstance(instance, nullptr);
    vkDestroyDevice(vk_device, nullptr);
    vkDestroySurfaceKHR(instance, vk_surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
}

} // namespace Raekor