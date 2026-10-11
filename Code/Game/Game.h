#pragma once

#include "Assets.h"
#include "Physics.h"
#include "Application.h"
#include "GameConsole.h"
#include "Renderer/RenderSystem.h"

namespace RK {

class GameApp : public Game
{
public:
    GameApp(const Path& inSceneFile);
    ~GameApp();

    void OnUpdate(float inDeltaTime) override;
    void OnEvent(const SDL_Event& inEvent) override;

    void SetCameraEntity(Entity inEntity) { m_CameraEntity = inEntity; }
    Entity GetCameraEntity() const { return m_CameraEntity; }

    Scene* GetScene() override { return &m_Scene; }
    Assets* GetAssets() override { return &m_Assets; }
    Physics* GetPhysics() override { return &m_Physics; }
    IRenderInterface* GetRenderInterface() override { return &m_RenderSystem; }

private:
    Scene m_Scene;
    Assets m_Assets;
    Physics m_Physics;

    DX12::RenderSystem m_RenderSystem;

    Camera m_Camera;
    Entity m_CameraEntity = Entity::Null;

    GameConsole m_Console;
    bool m_IgnoreTextInput = false;
};

} // RK