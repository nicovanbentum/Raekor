#pragma once

#include "GUI.h"
#include "Undo.h"
#include "Scene.h"
#include "Timer.h"
#include "Assets.h"
#include "Widget.h"
#include "Physics.h"
#include "Compiler.h"
#include "Threading.h"
#include "Application.h"

namespace RK {

enum ESceneTaskType
{
	SCENE_TASK_OPEN,
	SCENE_TASK_IMPORT
};


struct SceneTask
{
	ESceneTaskType mType;
	Path mFile;
	Timer mTimer;
	Job::Ptr mJob;
	UniquePtr<Scene> mScene;
	Atomic<bool> mSucceeded = false;
};


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
	bool OnCloseRequested() override;

	Scene* GetScene() final { return &m_Scene; }
	Assets* GetAssets() final { return &m_Assets; }
	Physics* GetPhysics() final { return &m_Physics; }
	UndoSystem* GetUndo() final { return &m_UndoSystem; }

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

	bool GetViewportChanged() const { return m_ViewportChanged; }
	void SetViewportChanged(bool inChanged) { m_ViewportChanged = inChanged; }

	void NewScene();
	void OpenScene(const Path& inFile);
	void ImportScene(const Path& inFile);
	void SaveScene();
	void SaveSceneAs();
	void SaveScene(const Path& inFile);

	void OpenSceneDialog();
	void ImportSceneDialog();

	void DeleteSelection();
	void DuplicateSelection();

	void BuildScripts();
	bool IsBuildingScripts() const { return m_ScriptBuildJob && !m_ScriptBuildJob->IsFinished(); }

	void MarkSceneChanged() { m_ChangeCount++; }
	bool IsSceneDirty() const { return m_ChangeCount != m_SavedChangeCount; }
	void RunAfterUnsavedChangesCheck(const std::function<void()>& inAction);

	const SceneTask* GetSceneTask() const { return m_SceneTask.get(); }
	bool IsSaving() const { return m_SaveJob && !m_SaveJob->IsFinished(); }

	void ResetLayout() { m_ResetLayout = true; }

	void BeginImGuiDockSpace();
	void EndImGuiDockSpace();

protected:
	void UpdateSceneTask();
	void UpdateWindowTitle();
	void StartSceneTask(ESceneTaskType inType, const Path& inFile);

	void DrawStatusBar();
	void DrawUnsavedChangesPopup();

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

	Atomic<Entity> m_ActiveEntity = Entity::Null;
	ImGuiSelectionBasicStorage m_Selection;

	Job::Ptr m_SaveJob;
	Job::Ptr m_ScriptBuildJob;
	UniquePtr<SceneTask> m_SceneTask;

	uint64_t m_ChangeCount = 0;
	uint64_t m_SavedChangeCount = 0;
	String m_WindowTitle;
	std::function<void()> m_PendingAction;
	bool m_OpenUnsavedChangesPopup = false;
	bool m_ResetLayout = false;
};


} // Raekor
