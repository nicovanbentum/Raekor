#pragma once

#include "gui.h"
#include "application.h"

#include "timer.h"
#include "../VK/VKRenderer.h"

#include "gui/widget.h"

namespace Raekor {

class RayTraceApp : public Application {
public:
    RayTraceApp();
    virtual ~RayTraceApp() = default;

    virtual void onUpdate(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

    bool drawSphereProperties(Sphere& sphere);

private:
    bool sceneChanged = false;

    unsigned int activeSphere = 0;
    unsigned int activeScreenTexture = 0;

    GLRenderer renderer;
    std::vector<std::shared_ptr<IWidget>> widgets;
    std::unique_ptr<RayTracingOneWeekend> rayTracePass;
};

} // raekor
