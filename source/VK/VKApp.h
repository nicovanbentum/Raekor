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

    virtual void OnUpdate(float dt) override;
    virtual void OnEvent(const SDL_Event& ev) override;

private:
    Scene m_Scene;
    Assets m_Assets;
    VK::Renderer m_Renderer;
    
    bool m_IsImGuiEnabled = true;
    bool m_IsSwapchainDirty = true;
};

}