#pragma once

#include "application.h"
#include "physics.h"
#include "assets.h"
#include "scene.h"
#include "gui.h"
#include "widget.h"
#include "compiler.h"

namespace Raekor {

class IEditor : public Application
{
public:
	friend class IWidget;

	IEditor(WindowFlags inWindowFlags, IRenderInterface* inRenderer);
	virtual ~IEditor();

	void OnUpdate(float dt) override;
	void OnEvent(const SDL_Event& event) override;

	Scene* GetScene() final { return &m_Scene; }
	Assets* GetAssets() final { return &m_Assets; }
	Physics* GetPhysics() final { return &m_Physics; }

	void LogMessage(const std::string& inMessage) final;

	void SetActiveEntity(Entity inEntity) final { m_ActiveEntity = inEntity; }
	Entity GetActiveEntity() final { return m_ActiveEntity; }

protected:
	Scene m_Scene;
	Assets m_Assets;
	Physics m_Physics;
	Widgets m_Widgets;

	void* m_CompilerProcess = nullptr;
	ImGuiID m_DockSpaceID;
	Entity m_ActiveEntity = NULL_ENTITY;

	std::vector<std::string> m_Messages;
};


} // Raekor