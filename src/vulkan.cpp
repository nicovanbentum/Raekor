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

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

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
    VkInstanceCreateInfo create_info = {};
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    VkInstance instance;
    VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
    m_assert(result == VK_SUCCESS, "failed to create vulkan instance");



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
}

} // namespace Raekor