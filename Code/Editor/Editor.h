#pragma once

#include "GUI.h"
#include "Scene.h"
#include "Assets.h"
#include "Widget.h"
#include "Physics.h"
#include "Compiler.h"
#include "Application.h"

namespace RK {

class Editor : public Application
{
public:
	friend class IWidget;

	Editor(WindowFlags inWindowFlags, IRenderInterface* inRenderInterface);
	virtual ~Editor();

	void OnUpdate(float dt) override;
	void OnEvent(const SDL_Event& event) override;

	Scene* GetScene() final { return &m_Scene; }
	Assets* GetAssets() final { return &m_Assets; }
	Physics* GetPhysics() final { return &m_Physics; }

	void LogMessage(const String& inMessage) final;

	Camera& GetCamera() { return m_Camera; }
	const Camera& GetCamera() const { return m_Camera; }

	void SetCameraEntity(Entity inEntity) { m_CameraEntity = inEntity; }
	Entity GetCameraEntity() const { return m_CameraEntity; }

	void SetActiveEntity(Entity inEntity) final { m_ActiveEntity.store(inEntity); }
	Entity GetActiveEntity() const final { return m_ActiveEntity.load(); }

	void BeginImGuiDockSpace();
	void EndImGuiDockSpace();

protected:
	Scene m_Scene;
	Assets m_Assets;
	Physics m_Physics;
	Widgets m_Widgets;
	IRenderInterface* m_RenderInterface;
	
	Camera m_Camera;
	Entity m_CameraEntity = Entity::Null;

	bool m_ViewportChanged = false;
	bool m_ViewportFullscreen = false;
	void* m_CompilerWindow = nullptr;
	void* m_CompilerProcess = nullptr;
	
	ImGuiID m_DockSpaceID;
	bool m_DockSpaceBuilt = false;

	Array<String> m_Messages;
	Atomic<Entity> m_ActiveEntity = Entity::Null;
	Array<Atomic<Entity>> m_MultiSelectEntities;

};


} // Raekor