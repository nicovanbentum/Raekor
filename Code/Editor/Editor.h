#pragma once

#include "Application.h"
#include "physics.h"
#include "assets.h"
#include "scene.h"
#include "gui.h"
#include "widget.h"
#include "compiler.h"

namespace RK {

class IEditor : public Application
{
public:
	friend class IWidget;

	IEditor(WindowFlags inWindowFlags, IRenderInterface* inRenderInterface);
	virtual ~IEditor();

	void OnUpdate(float dt) override;
	void OnEvent(const SDL_Event& event) override;

	Scene* GetScene() final { return &m_Scene; }
	Assets* GetAssets() final { return &m_Assets; }
	Physics* GetPhysics() final { return &m_Physics; }

	void LogMessage(const String& inMessage) final;

	void SetActiveEntity(Entity inEntity) final { m_ActiveEntity.store(inEntity); }
	Entity GetActiveEntity() final { return m_ActiveEntity.load(); }

	void BeginImGuiDockSpace();
	void EndImGuiDockSpace();

protected:
	Scene m_Scene;
	Assets m_Assets;
	Physics m_Physics;
	Widgets m_Widgets;
	IRenderInterface* m_RenderInterface;

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