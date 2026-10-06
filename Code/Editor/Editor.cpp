#include "PCH.h"
#include "Editor.h"

#include "OS.h"
#include "OBJ.h"
#include "FBX.h"
#include "GLTF.h"
#include "Maths.h"
#include "Input.h"
#include "Timer.h"
#include "Script.h"
#include "Profiler.h"
#include "Components.h"
#include "UIRenderer.h"
#include "DebugRenderer.h"
#include "ShaderGraphNodes.h"
#include "IconsFontAwesome5.h"

#include "Game/Scripts/Scripts.h"

#include "Widgets/AssetsWidget.h"
#include "Widgets/MenubarWidget.h"
#include "Widgets/ConsoleWidget.h"
#include "Widgets/ProfileWidget.h"
#include "Widgets/ViewportWidget.h"
#include "Widgets/SequenceWidget.h"
#include "Widgets/InspectorWidget.h"
#include "Widgets/HierarchyWidget.h"
#include "Widgets/NodeGraphWidget.h"

namespace RK {

static constexpr const char* cSceneFileFilter = "Scene Files (*.scene)\0*.scene\0";
static constexpr const char* cImportFileFilter = "Importable Files (*.scene, *.gltf, *.glb, *.fbx, *.obj)\0*.scene;*.gltf;*.glb;*.fbx;*.obj\0";
static constexpr const char* cUnsavedChangesPopup = "Unsaved Changes";

Editor::Editor(WindowFlags inWindowFlags, IRenderInterface* inRenderInterface) :
	Game(inWindowFlags /* | WindowFlag::BORDERLESS */),

	m_Scene(inRenderInterface),
	m_Physics(inRenderInterface),
	m_UndoSystem(m_Scene),
	m_RenderInterface(inRenderInterface)
{
	gRegisterScriptTypes();
	gRegisterShaderNodeTypes();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImNodes::CreateContext();
	ImGui::StyleColorsDark();

	// get GUI i/o and set a bunch of settings
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	io.ConfigDockingWithShift = false;

	const float ui_scale = GUI::GetDisplayScale(m_Window);

	GUI::SetDarkTheme(ui_scale);
	ImNodes::StyleColorsDark();

	if (!m_ConfigSettings.mFontFile.empty() && fs::exists(m_ConfigSettings.mFontFile))
		GUI::SetFont(m_ConfigSettings.mFontFile.string(), ui_scale);

	if (OS::sCheckCommandLineOption("-shader_editor"))
	{
		m_Widgets.Register<ShaderGraphWidget>(this);
	}
	else
	{
		m_Widgets.Register<SequenceWidget>(this);
		m_Widgets.Register<MaterialsWidget>(this);
		m_Widgets.Register<MenubarWidget>(this);
		m_Widgets.Register<ConsoleWidget>(this);
		m_Widgets.Register<ShaderGraphWidget>(this);
		m_Widgets.Register<ViewportWidget>(this);
		m_Widgets.Register<ProfileWidget>(this);
		m_Widgets.Register<InspectorWidget>(this);
		m_Widgets.Register<HierarchyWidget>(this);
	}

	gLogInfo("Editor", "Initialization done");

	// hide the console window
	if (!IsDebuggerPresent())
		ShowWindow(GetConsoleWindow(), SW_HIDE);

	// launch the asset compiler  app to the system tray
	// Make sure we don't try this when we are the the compiler app
	// (I've had to restart my PC 3 times already because of recursive process creation lol)
	if (OS::sCheckCommandLineOption("-asset_compiler"))
	{
		Application::Terminate();
		assert(false);
		return;
	}

	if (g_CVariables->Create("launch_asset_compiler_on_startup", 0))
	{
		const HWND editor_window = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(m_Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
		String compiler_app_cmd_line = std::format("{} -asset_compiler -log_file=AssetCompiler -ipc_window={}", OS::sGetExecutablePath().string(), uint64_t(editor_window));

		PROCESS_INFORMATION pi = {};
		STARTUPINFO si = { sizeof(si) };

		if (CreateProcessA(NULL, &compiler_app_cmd_line[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		{
			m_CompilerProcess = pi.hProcess;
			CloseHandle(pi.hThread);
		}

		if (m_CompilerProcess == nullptr)
			gLogError("Editor", "Failed to start asset compiler process.");
	}

	m_Camera.SetPosition(Vec3(1.0f, 1.0f, -1.0f));
	m_Camera.LookAt(Vec3(0.0f, 0.0f, 0.0f));

	UpdateWindowTitle();
}


Editor::~Editor()
{
	if (m_SceneTask && m_SceneTask->mJob)
		m_SceneTask->mJob->Wait();

	if (m_SceneTask)
		m_SceneTask->mScene->ReleaseResources();

	if (m_SaveJob)
		m_SaveJob->Wait();

	if (m_CompilerProcess)
	{
		TerminateProcess(m_CompilerProcess, 0);
		CloseHandle(m_CompilerProcess);
	}
}


void Editor::OnUpdate(float inDeltaTime)
{
	// clear all profile sections
	g_Profiler->Reset();

	PROFILE_FUNCTION_CPU();

	UpdateSceneTask();

    // Clear UI draw command buffers
    g_UIRenderer.Reset();

	// clear the debug renderer vertex buffers
	g_DebugRenderer.Reset();

    // update relative mouse mode
    SDL_SetWindowRelativeMouseMode(m_Window, g_Input->IsRelativeMouseMode());

	// update the physics system
	m_Physics.OnUpdate(m_Scene);

	// step the physics simulation
	if (m_Physics.GetState() == Physics::Stepping)
		m_Physics.Step(m_Scene, inDeltaTime);

	static int& update_transforms = g_CVariables->Create("update_transforms", 1, true);

	if (update_transforms)
	{
		// update Transform components
		m_Scene.UpdateTransforms();

	    // update camera transforms
	    m_Scene.UpdateCameras();

		// update Light and DirectionalLight components
		m_Scene.UpdateLights();
	}

    if (m_CameraEntity != Entity::Null)
	{
		m_Viewport.OnUpdate(m_Scene.Get<Camera>(m_CameraEntity));
	}
	else // if the game has not taken over the camera, use the editor controls
	{
		m_Viewport.OnUpdate(m_Camera);
		if (m_GameState != GAME_RUNNING)
			EditorCameraController::OnUpdate(m_Camera, inDeltaTime);
	}

	int cGridSize = 20;
	float cGridCellSize = 0.5f;

	int cGridHalfSize = cGridSize / 2;
	for (float i = -cGridHalfSize; i <= cGridHalfSize; i += cGridCellSize)
	{
		Vec4 color = Vec4(0.2, 0.2, 0.2, 0.2);
		g_DebugRenderer.AddLine(Vec3(i, 0, -cGridHalfSize), Vec3(i, 0, cGridHalfSize), color);
		g_DebugRenderer.AddLine(Vec3(-cGridHalfSize, 0, i), Vec3(cGridHalfSize, 0, i), color);
	}

	g_DebugRenderer.AddLine(Vec3(0, 0.001, 0), Camera::cUp * 2.0f, Vec4(0, 1, 0, 1));
	g_DebugRenderer.AddLine(Vec3(0, 0.001, 0), Camera::cRight * 2.0f, Vec4(1, 0, 0, 1));
	g_DebugRenderer.AddLine(Vec3(0, 0.001, 0), Camera::cForward * 2.0f, Vec4(0, 0, 1, 1));

	// render any scene dependent debug shapes
	if (GetActiveEntity() != Entity::Null && m_ActiveEntity != m_Scene.GetRootEntity())
	{
		static float time = 0.0f;
		time = time + inDeltaTime * 3.0f;

		m_Scene.RenderDebugShapes(GetActiveEntity(), std::cos(time) * 0.5 + 0.5);

	}

	// update Skeleton and Animation components
	m_Scene.UpdateAnimations(inDeltaTime);

    // update NativeScript components
    if (GetGameState() == GAME_RUNNING)
        m_Scene.UpdateNativeScripts(inDeltaTime, this);

	// start ImGui
	GUI::BeginFrame();

	if (g_Input->IsRelativeMouseMode())
		ImGui::GetIO().MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

	if (GetConfigSettings().mShowUI)
	{
		DrawStatusBar();

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
			BeginImGuiDockSpace();

		// draw widgets
		m_Widgets.Draw(inDeltaTime);

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
			EndImGuiDockSpace();

		DrawUnsavedChangesPopup();
	}

	bool scene_changed = false;

	// detect any changes to the viewport (mainly used to reset path tracers)
	if (ViewportWidget* widget = m_Widgets.GetWidget<ViewportWidget>())
		scene_changed |= widget->Changed();

	// detect any changes to the scene
	if (InspectorWidget* widget = m_Widgets.GetWidget<InspectorWidget>())
		scene_changed |= widget->SceneChanged();

	if (scene_changed && GetGameState() == GAME_STOPPED)
		MarkSceneChanged();

	m_ViewportChanged |= scene_changed;

	// this has to check deltas in camera matrices, so only do it if the previous checks failed
	if (!m_ViewportChanged)
		m_ViewportChanged = m_Viewport.Changed();

	// ImGui::ShowDemoWindow();
	// ImGui::ShowStyleEditor();

	GUI::EndFrame();

	UpdateWindowTitle();

	// Applications that implement Editor should call Editor::OnUpdate first, then do their own rendering
}



void Editor::OnEvent(const SDL_Event& event)
{
	ImGui_ImplSDL3_ProcessEvent(&event);

	if (m_GameState != GAME_RUNNING && m_CameraEntity == Entity::Null)
	{
		if (const ViewportWidget* viewport_widget = m_Widgets.GetWidget<ViewportWidget>())
		{
			if (viewport_widget->IsHovered() || g_Input->IsRelativeMouseMode() || !m_ConfigSettings.mShowUI)
			{
				EditorCameraController::OnEvent(m_Camera, event);
			}
		}
	}

	if (event.type == SDL_EVENT_WINDOW_RESIZED)
	{
		if (!m_ConfigSettings.mShowUI)
		{
			int w, h;
			SDL_GetWindowSize(m_Window, &w, &h);
			m_Viewport.SetDisplaySize(UVec2(w, h));
		}
	}

	const bool is_typing = ImGui::GetIO().WantTextInput;

	if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && !is_typing)
	{
		const SDL_Keymod modifiers = SDL_GetModState();
		const bool ctrl = ( modifiers & SDL_KMOD_CTRL ) != 0;
		const bool shift = ( modifiers & SDL_KMOD_SHIFT ) != 0;
		const bool editing = GetGameState() != GAME_RUNNING;

		switch (event.key.key)
		{
			case SDLK_N:
			{
				if (ctrl && editing)
					RunAfterUnsavedChangesCheck([this]() { NewScene(); });
			} break;

			case SDLK_O:
			{
				if (ctrl && editing)
					RunAfterUnsavedChangesCheck([this]() { OpenSceneDialog(); });
			} break;

			case SDLK_I:
			{
				if (ctrl && editing)
					ImportSceneDialog();
			} break;

			case SDLK_S:
			{
				if (ctrl && editing)
					shift ? SaveSceneAs() : SaveScene();
			} break;

			case SDLK_D:
			{
				if (ctrl && editing)
					DuplicateSelection();
			} break;

			case SDLK_DELETE:
			{
				if (editing && !g_Input->IsRelativeMouseMode())
					DeleteSelection();
			} break;

			case SDLK_Z:
			{
				if (ctrl && editing && m_UndoSystem.HasUndo())
				{
					m_UndoSystem.Undo();
					m_ViewportChanged = true;
					MarkSceneChanged();
				}
			} break;

			case SDLK_Y:
			{
				if (ctrl && editing && m_UndoSystem.HasRedo())
				{
					m_UndoSystem.Redo();
					m_ViewportChanged = true;
					MarkSceneChanged();
				}
			} break;

			case SDLK_F1:
			{
				for (auto& widget : m_Widgets)
				{
					if (widget->GetRTTI() != RTTI_OF<ViewportWidget>())
					{
						if (m_ViewportFullscreen)
							widget->Restore();
						else
							widget->Hide();
					}
				}

				m_ViewportFullscreen = !m_ViewportFullscreen;
			} break;

			case SDLK_LALT:
			case SDLK_RALT:
			{
				g_Input->SetRelativeMouseMode(!g_Input->IsRelativeMouseMode());
			} break;

			case SDLK_F5:
			{
				if (GetGameState() == GAME_PAUSED)
					Game::Unpause();
				else
					Game::Start();
			} break;

			case SDLK_ESCAPE:
			{
				if (GetGameState() != GAME_STOPPED)
					Game::Stop();
				else
					SetActiveEntity(Entity::Null);
			} break;
		}
	}

	if (GetGameState() == GAME_RUNNING)
	{
		for (auto [entity, script] : m_Scene.Each<NativeScript>())
		{
			if (script.script)
			{
				try
				{
					script.script->OnEvent(event);
				}
				catch (const std::exception& e)
				{
					gLogError("Script", "{}", e.what());
				}
			}
		}
	}

	if (m_ConfigSettings.mShowUI && GetGameState() != GAME_RUNNING)
		m_Widgets.OnEvent(event);
}


bool Editor::OnCloseRequested()
{
	if (!IsSceneDirty())
		return true;

	RunAfterUnsavedChangesCheck([this]() { Terminate(); });

	return false;
}


inline void Editor::SetActiveEntity(Entity inEntity)
{
	if (inEntity != Entity::Null)
		m_Selection.Clear();

	m_ActiveEntity.store(inEntity);
}


void Editor::NewScene()
{
	if (m_SceneTask)
		return;

	Game::Stop();

	Scene empty_scene(m_RenderInterface);
	m_Scene.Swap(empty_scene);
	empty_scene.ReleaseResources();

	m_UndoSystem.Clear();
	m_Selection.Clear();
	SetActiveEntity(Entity::Null);
	SetCameraEntity(Entity::Null);

	m_ChangeCount = 0;
	m_SavedChangeCount = 0;
	m_ViewportChanged = true;

	GetDiscordRPC().SetActivityDetails("Untitled");
}


void Editor::OpenScene(const Path& inFile)
{
	StartSceneTask(SCENE_TASK_OPEN, inFile);
}


void Editor::ImportScene(const Path& inFile)
{
	StartSceneTask(SCENE_TASK_IMPORT, inFile);
}


void Editor::OpenSceneDialog()
{
	const String file_path = OS::sOpenFileDialog(cSceneFileFilter);

	if (!file_path.empty())
		OpenScene(file_path);
}


void Editor::ImportSceneDialog()
{
	const String file_path = OS::sOpenFileDialog(cImportFileFilter);

	if (!file_path.empty())
		ImportScene(file_path);
}


void Editor::SaveScene()
{
	if (m_Scene.GetFilePath().empty())
		SaveSceneAs();
	else
		SaveScene(m_Scene.GetFilePath());
}


void Editor::SaveSceneAs()
{
	const String file_path = OS::sSaveFileDialog("Scene File (*.scene)\0", "scene");

	if (!file_path.empty())
		SaveScene(file_path);
}


void Editor::SaveScene(const Path& inFile)
{
	if (IsSaving())
	{
		gLogWarning("Editor", "Already saving the scene, try again when it's done.");
		return;
	}

	Timer timer;

	SharedPtr<Scene> snapshot = std::make_shared<Scene>(nullptr);
	snapshot->CopyFrom(m_Scene);

	gLogDebug("Editor", "Scene snapshot took {:.2f} ms", Timer::sToMilliseconds(timer.GetElapsedTime()));

	m_Scene.SetFilePath(inFile);
	m_SavedChangeCount = m_ChangeCount;

	AddRecentScene(inFile);

	m_SaveJob = g_JobSystem.Schedule([this, snapshot, inFile]()
	{
		Timer timer;
		snapshot->SaveToFile(inFile.string(), m_Assets);
		gLogInfo("Editor", "Saved scene to {} in {:.2f} seconds", inFile.string(), timer.GetElapsedTime());
	}, JOB_PRIORITY_LOW);
}


void Editor::DeleteSelection()
{
	bool deleted = false;

	void* iterator = nullptr;
	ImGuiID id = 0;

	while (m_Selection.GetNextSelectedItem(&iterator, &id))
	{
		const Entity entity = Entity(id);

		if (entity != m_Scene.GetRootEntity() && m_Scene.Exists(entity))
		{
			m_Scene.Destroy(entity);
			deleted = true;
		}
	}

	m_Selection.Clear();

	const Entity active_entity = GetActiveEntity();

	if (active_entity != Entity::Null && active_entity != m_Scene.GetRootEntity() && m_Scene.Exists(active_entity))
	{
		m_Scene.Destroy(active_entity);
		deleted = true;
	}

	SetActiveEntity(Entity::Null);

	if (deleted)
	{
		m_UndoSystem.Clear();
		m_ViewportChanged = true;
		MarkSceneChanged();
	}
}


void Editor::DuplicateSelection()
{
	const Entity active_entity = GetActiveEntity();

	if (active_entity == Entity::Null || active_entity == m_Scene.GetRootEntity() || !m_Scene.Exists(active_entity))
		return;

	SetActiveEntity(m_Scene.Clone(active_entity));

	m_ViewportChanged = true;
	MarkSceneChanged();
}


void Editor::RunAfterUnsavedChangesCheck(const std::function<void()>& inAction)
{
	if (!IsSceneDirty())
	{
		inAction();
		return;
	}

	m_PendingAction = inAction;
	m_OpenUnsavedChangesPopup = true;
}


void Editor::StartSceneTask(ESceneTaskType inType, const Path& inFile)
{
	if (m_SceneTask)
	{
		gLogWarning("Editor", "Still busy loading {}, try again when it's done.", m_SceneTask->mFile.filename().string());
		return;
	}

	if (!fs::exists(inFile))
	{
		gLogError("Editor", "File {} does not exist.", inFile.string());
		return;
	}

	if (inType == SCENE_TASK_OPEN)
		Game::Stop();

	m_SceneTask = std::make_unique<SceneTask>();
	m_SceneTask->mType = inType;
	m_SceneTask->mFile = inFile;
	m_SceneTask->mScene = std::make_unique<Scene>(m_RenderInterface);

	SceneTask* task = m_SceneTask.get();

	gLogInfo("Editor", "{} {}..", inType == SCENE_TASK_OPEN ? "Opening" : "Importing", inFile.string());

	task->mJob = g_JobSystem.Schedule([this, task]()
	{
		const String file_path = task->mFile.string();

		try
		{
			if (task->mType == SCENE_TASK_OPEN)
			{
				task->mSucceeded = task->mScene->LoadFromFile(file_path, m_Assets);
				return;
			}

			const Path extension = task->mFile.extension();

			UniquePtr<Importer> importer;

			if (extension == ".gltf" || extension == ".glb")
				importer = std::make_unique<GltfImporter>(*task->mScene, nullptr);
			else if (extension == ".fbx")
				importer = std::make_unique<FBXImporter>(*task->mScene, nullptr);
			else if (extension == ".obj")
				importer = std::make_unique<OBJImporter>(*task->mScene, nullptr);
			else if (extension == ".scene")
				importer = std::make_unique<SceneImporter>(*task->mScene, nullptr);

			if (importer)
				task->mSucceeded = importer->LoadFromFile(file_path, &m_Assets);
			else
				gLogError("Editor", "No importer available for {}", file_path);
		}
		catch (const std::exception& e)
		{
			gLogError("Editor", "Failed to load {}: {}", file_path, e.what());
			task->mSucceeded = false;
		}
	}, JOB_PRIORITY_LOW);
}


void Editor::UpdateSceneTask()
{
	if (!m_SceneTask || !m_SceneTask->mJob->IsFinished())
		return;

	UniquePtr<SceneTask> task = std::move(m_SceneTask);

	const String file_name = task->mFile.filename().string();

	if (!task->mSucceeded)
	{
		gLogError("Editor", "Failed to load {}", task->mFile.string());
		task->mScene->ReleaseResources();
		return;
	}

	if (task->mType == SCENE_TASK_OPEN)
	{
		Game::Stop();

		m_Scene.Swap(*task->mScene);
		task->mScene->ReleaseResources();

		m_Scene.UploadMeshes();
		m_Scene.BindScripts(m_Assets, this);

		m_UndoSystem.Clear();
		m_Selection.Clear();
		SetActiveEntity(Entity::Null);
		SetCameraEntity(Entity::Null);

		m_ChangeCount = 0;
		m_SavedChangeCount = 0;

		AddRecentScene(task->mFile);
		GetDiscordRPC().SetActivityDetails(file_name.c_str());

		gLogInfo("Editor", "Opened {} in {:.2f} seconds", file_name, task->mTimer.GetElapsedTime());
	}
	else
	{
		const Array<Entity> new_entities = m_Scene.Merge(*task->mScene);

		m_Scene.UploadMeshes(new_entities);

		for (Entity entity : new_entities)
		{
			if (m_Scene.GetParent(entity) == m_Scene.GetRootEntity())
			{
				SetActiveEntity(entity);
				break;
			}
		}

		MarkSceneChanged();

		gLogInfo("Editor", "Imported {} in {:.2f} seconds", file_name, task->mTimer.GetElapsedTime());
	}

	m_ViewportChanged = true;
}


void Editor::UpdateWindowTitle()
{
	const Path& file_path = m_Scene.GetFilePath();
	const String scene_name = file_path.empty() ? "Untitled" : file_path.filename().string();

	const String title = std::format("{}{} - {}", scene_name, IsSceneDirty() ? "*" : "", m_ConfigSettings.mAppName);

	if (title != m_WindowTitle)
	{
		m_WindowTitle = title;
		SDL_SetWindowTitle(m_Window, m_WindowTitle.c_str());
	}
}


void Editor::DrawStatusBar()
{
	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 4.0f));
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));

	if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, ImGui::GetFrameHeight(), flags))
	{
		if (ImGui::BeginMenuBar())
		{
			const Path& file_path = m_Scene.GetFilePath();
			const String scene_name = file_path.empty() ? "Untitled" : file_path.filename().string();

			ImGui::TextUnformatted((const char*)ICON_FA_CUBES);
			ImGui::TextUnformatted(scene_name.c_str());

			if (IsSceneDirty())
				ImGui::TextDisabled("(modified)");

			ImGui::Separator();

			ImGui::TextDisabled("%u entities", uint32_t(m_Scene.GetEntities().size()));

			if (m_SceneTask)
			{
				ImGui::Separator();

				const float spinner_radius = ImGui::GetTextLineHeight() * 0.4f;
				ImGui::Spinner("##SceneTaskSpinner", spinner_radius, 2, ImGui::GetColorU32(ImGuiCol_CheckMark));

				ImGui::Text("%s %s (%.1fs)", m_SceneTask->mType == SCENE_TASK_OPEN ? "Opening" : "Importing", m_SceneTask->mFile.filename().string().c_str(), m_SceneTask->mTimer.GetElapsedTime());
			}

			if (IsSaving())
			{
				ImGui::Separator();
				ImGui::TextUnformatted((const char*)ICON_FA_SAVE " Saving..");
			}

			const char* game_state_text = "Editing";
			ImVec4 game_state_color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

			switch (GetGameState())
			{
				case GAME_RUNNING: game_state_text = "Playing"; game_state_color = ViewportWidget::cRunningColor; break;
				case GAME_PAUSED:  game_state_text = "Paused";  game_state_color = ViewportWidget::cPausedColor;  break;
				default: break;
			}

			const String right_text = std::format("{}   {:.1f} ms ({:.0f} FPS)", game_state_text, 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			const float right_width = ImGui::CalcTextSize(right_text.c_str()).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;

			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - right_width);
			ImGui::TextColored(game_state_color, "%s", game_state_text);
			ImGui::SameLine();
			ImGui::TextDisabled("  %.1f ms (%.0f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			ImGui::EndMenuBar();
		}
	}

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}


void Editor::DrawUnsavedChangesPopup()
{
	if (m_OpenUnsavedChangesPopup)
	{
		ImGui::OpenPopup(cUnsavedChangesPopup);
		m_OpenUnsavedChangesPopup = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(cUnsavedChangesPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		const Path& file_path = m_Scene.GetFilePath();
		const String scene_name = file_path.empty() ? "Untitled" : file_path.filename().string();

		ImGui::TextUnformatted((const char*)ICON_FA_EXCLAMATION_TRIANGLE);
		ImGui::SameLine();
		ImGui::Text("Save changes to %s before continuing?", scene_name.c_str());
		ImGui::TextDisabled("Your changes will be lost if you don't save them.");

		ImGui::Spacing();

		const float button_width = ImGui::CalcTextSize("Don't Save").x + ImGui::GetStyle().FramePadding.x * 4.0f;

		auto RunPendingAction = [this]()
		{
			std::function<void()> action = std::move(m_PendingAction);
			m_PendingAction = nullptr;

			if (action)
				action();
		};

		if (ImGui::Button("Save", ImVec2(button_width, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			SaveScene();

			if (!IsSceneDirty())
				RunPendingAction();
			else
				m_PendingAction = nullptr;
		}

		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();

		if (ImGui::Button("Don't Save", ImVec2(button_width, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			m_SavedChangeCount = m_ChangeCount;
			RunPendingAction();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(button_width, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			ImGui::CloseCurrentPopup();
			m_PendingAction = nullptr;
		}

		ImGui::EndPopup();
	}
}


void Editor::BeginImGuiDockSpace()
{
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking;
	flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	if (IWidget* widget = m_Widgets.GetWidget<MenubarWidget>())
	{
		if (widget->IsOpen())
			flags |= ImGuiWindowFlags_MenuBar;
	}

	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

	ImGuiViewport* imgui_viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(imgui_viewport->WorkPos);
	ImGui::SetNextWindowSize(imgui_viewport->WorkSize);
	ImGui::SetNextWindowViewport(imgui_viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGuiWindowClass window_class = {};
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
	ImGui::SetNextWindowClass(&window_class);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", nullptr, flags);
	ImGui::PopStyleVar();
	ImGui::PopStyleVar(2);

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, &window_class);

	if (m_ResetLayout || ( m_DockSpaceID != dockspace_id && !fs::exists("imgui.ini") ))
	{
		m_ResetLayout = false;

		ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
		ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspace_id, imgui_viewport->WorkSize);

		ImGuiID center_node = dockspace_id;
		ImGuiID right_node = ImGui::DockBuilderSplitNode(center_node, ImGuiDir_Right, 0.22f, nullptr, &center_node);
		ImGuiID bottom_node = ImGui::DockBuilderSplitNode(center_node, ImGuiDir_Down, 0.25f, nullptr, &center_node);
		ImGuiID inspector_node = ImGui::DockBuilderSplitNode(right_node, ImGuiDir_Down, 0.6f, nullptr, &right_node);

		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<ViewportWidget>()->GetTitle().c_str(), center_node);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<ShaderGraphWidget>()->GetTitle().c_str(), center_node);

		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<HierarchyWidget>()->GetTitle().c_str(), right_node);

		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<InspectorWidget>()->GetTitle().c_str(), inspector_node);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<ProfileWidget>()->GetTitle().c_str(), inspector_node);

		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<ConsoleWidget>()->GetTitle().c_str(), bottom_node);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<MaterialsWidget>()->GetTitle().c_str(), bottom_node);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<SequenceWidget>()->GetTitle().c_str(), bottom_node);

		ImGui::DockBuilderFinish(dockspace_id);
	}

	m_DockSpaceID = dockspace_id;
}


void Editor::EndImGuiDockSpace()
{
	ImGui::End();
}


} // Raekor
