#pragma once

#include "gui.h"
#include "application.h"

#include "timer.h"
#include "../VK/VKRenderer.h"

namespace Raekor {

class RayTraceApp : public WindowApplication {
public:
    RayTraceApp();
    virtual ~RayTraceApp() = default;

    virtual void update(float dt) override;

    bool drawSphereProperties(RenderPass::Sphere& sphere);

private:
    bool sceneChanged = false;

    unsigned int activeSphere = 0;
    unsigned int activeScreenTexture = 0;

    GLRenderer renderer;

    gui::Guizmo gizmo;
    gui::Dockspace dockspace;
    gui::ViewportWindow viewportWindow;
    gui::CameraSettings cameraSettingsWindow;
    std::unique_ptr<RenderPass::RayCompute> rayTracePass;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct mod {
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 position = { 0.0, 0.0f, 0.0f }, scale = { 1.0f, 1.0f, 1.0f }, rotation = { 0, 0, 0 };

    void transform() {
        model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        auto rotation_quat = static_cast<glm::quat>(rotation);
        model = model * glm::toMat4(rotation_quat);
        model = glm::scale(model, scale);
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class VulkanApp : public WindowApplication {
public:
    VulkanApp();
    virtual ~VulkanApp();

    virtual void update(float dt) override;

private:
    int active = 0;
    VK::Renderer vk;
    std::vector<mod> mods;

    bool useVsync = true, shouldRecreateSwapchain = false;

    gui::Dockspace dockspace;
    gui::ViewportWindow viewportWindow;
};

} // raekor
