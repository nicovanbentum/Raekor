#pragma once

#include "application.h"
#include "scene.h"
#include "assets.h"
#include "VKRenderer.h"

namespace Raekor {

class VulkanPathTracer : public WindowApplication {
public:
    VulkanPathTracer();
    virtual ~VulkanPathTracer();

    virtual void update(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    Scene scene;
    Assets assets;
    VK::Renderer renderer;
    bool shouldRecreateSwapchain = true;

};

}