#pragma once

#include "gui.h"
#include "application.h"

#include "timer.h"
#include "../VK/VKRenderer.h"

#include "gui/widget.h"

namespace Raekor {

class RayTraceApp : public WindowApplication {
public:
    RayTraceApp();
    virtual ~RayTraceApp() = default;

    virtual void update(float dt) override;

    bool drawSphereProperties(Sphere& sphere);

private:
    bool sceneChanged = false;

    unsigned int activeSphere = 0;
    unsigned int activeScreenTexture = 0;

    GLRenderer renderer;
    std::vector<std::shared_ptr<IWidget>> widgets;
    std::unique_ptr<RayCompute> rayTracePass;
};

class VulkanApp : public WindowApplication {
public:
    VulkanApp();
    virtual ~VulkanApp();

    virtual void update(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    VK::Renderer vk;

    HANDLE changes;
    std::filesystem::directory_iterator shaderDirectory;
    std::unordered_map<std::string, std::filesystem::file_time_type> files;

    std::shared_ptr<IWidget> viewportWindow;
    bool useVsync = true, shouldRecreateSwapchain = false;

};

} // raekor
