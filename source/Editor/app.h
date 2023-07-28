#pragma once

#include "Raekor/application.h"
#include "Raekor/physics.h"
#include "Raekor/assets.h"
#include "Raekor/scene.h"
#include "Raekor/gui.h"
#include "renderer.h"
#include "widget.h"

namespace Raekor::GL {

class GLApp : public Application {
public:
    friend class IWidget;

    GLApp();
    virtual ~GLApp() = default;

    void OnUpdate(float dt) override;
    void OnEvent(const SDL_Event& event) override;

    Scene* GetScene() { return &m_Scene; }
    Assets* GetAssets() { return &m_Assets; }
    Physics* GetPhysics() { return &m_Physics; }
    IRenderer* GetRenderer() { return &m_Renderer; }

    void LogMessage(const std::string& inMessage) override;

    void SetActiveEntity(Entity inEntity) override { m_ActiveEntity = inEntity; }
    Entity GetActiveEntity() override { return m_ActiveEntity; }

private:
    Scene m_Scene;
    Assets m_Assets;
    Physics m_Physics;
    Widgets m_Widgets;
    Renderer m_Renderer;
  
    ImGuiID m_DockSpaceID;
    Entity m_ActiveEntity = sInvalidEntity;

    std::vector<std::string> m_Messages;
};


} // Raekor