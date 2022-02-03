#pragma once

#include "Raekor/application.h"
#include "Raekor/scene.h"
#include "Raekor/assets.h"
#include "VKRenderer.h"

namespace Raekor::VK {

class PathTracer : public Application {
public:
    PathTracer();
    virtual ~PathTracer();

    virtual void onUpdate(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override;

private:
    Scene scene;
    Assets assets;
    VK::Renderer renderer;
    
    bool isImGuiEnabled = true;
    bool shouldRecreateSwapchain = true;

};

}