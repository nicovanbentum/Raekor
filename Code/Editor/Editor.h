#pragma once

#include "GUI.h"
#include "Undo.h"
#include "Scene.h"
#include "Assets.h"
#include "Widget.h"
#include "Physics.h"
#include "Compiler.h"
#include "Application.h"

namespace RK {

class Editor : public Game
{
	struct Settings
	{
		float& scaleSnap = g_CVariables->Create("editor_scale_snap", 0.1f);
		float& rotationSnap = g_CVariables->Create("editor_rotation_snap", 1.0f);
		float& translationSnap = g_CVariables->Create("editor_translation_snap", 0.1f);
	} m_Settings;

public:
	friend class IWidget;

	Editor(WindowFlags inWindowFlags, IRenderInterface* inRenderInterface);
	virtual ~Editor();

	void OnUpdate(float dt) override;
	void OnEvent(const SDL_Event& event) override;

	Scene* GetScene() final { return &m_Scene; }
	Assets* GetAssets() final { return &m_Assets; }
	Physics* GetPhysics() final { return &m_Physics; }
	UndoSystem* GetUndo() final { return &m_UndoSystem; }

	void LogMessage(const String& inMessage) final;

	Settings& GetSettings() { return m_Settings; }
	const Settings& GetSettings() const { return m_Settings; }

	Camera& GetCamera() { return m_Camera; }
	const Camera& GetCamera() const { return m_Camera; }

	void SetCameraEntity(Entity inEntity) { m_CameraEntity = inEntity; }
	Entity GetCameraEntity() const { return m_CameraEntity; }

	void SetActiveEntity(Entity inEntity) final;
	Entity GetActiveEntity() const final { return m_ActiveEntity.load(); }

	ImGuiSelectionBasicStorage& GetMultiSelect() { return m_Selection; }
	const ImGuiSelectionBasicStorage& GetMultiSelect() const { return m_Selection; }

	void BeginImGuiDockSpace();
	void EndImGuiDockSpace();

protected:
	Scene m_Scene;
	Assets m_Assets;
	Physics m_Physics;
	Widgets m_Widgets;
	UndoSystem m_UndoSystem;
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
	ImGuiSelectionBasicStorage m_Selection;


};


} // Raekor