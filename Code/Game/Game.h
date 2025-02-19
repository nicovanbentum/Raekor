#pragma once

#include "Assets.h"
#include "Physics.h"
#include "Application.h"
#include "Renderer/Device.h"
#include "Renderer/Resource.h"
#include "Renderer/Renderer.h"

namespace RK {

class GameApp : public Game
{
public:
    GameApp();
    ~GameApp();

    void OnUpdate(float inDeltaTime) override;
    void OnEvent(const SDL_Event& inEvent) override;

    void SetCameraEntity(Entity inEntity) { m_CameraEntity = inEntity; }
    Entity GetCameraEntity() const { return m_CameraEntity; }

    Scene* GetScene() override { return &m_Scene; }
    Assets* GetAssets() override { return &m_Assets; }
    Physics* GetPhysics() override { return &m_Physics; }
    IRenderInterface* GetRenderInterface() override { return &m_RenderInterface; }

private:
    Scene m_Scene;
    Assets m_Assets;
    Physics m_Physics;

    DX12::Device m_Device;
    DX12::Renderer m_Renderer;
    DX12::RayTracedScene m_RayTracedScene;
    DX12::RenderInterface m_RenderInterface;

    DX12::TextureID m_DefaultWhiteTexture;
    DX12::TextureID m_DefaultBlackTexture;
    DX12::TextureID m_DefaultNormalTexture;

    Camera m_Camera;
    Entity m_CameraEntity = Entity::Null;
};

} // RK