#include "PCH.h"
#include "Editor.h"

#include "OS.h"
#include "Maths.h"
#include "Input.h"
#include "Timer.h"
#include "Script.h"
#include "Profiler.h"
#include "Components.h"
#include "UIRenderer.h"
#include "DebugRenderer.h"
#include "ShaderGraphNodes.h"

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
	io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	io.ConfigDockingWithShift = true;

	GUI::SetDarkTheme();
	ImGui::GetStyle().ScaleAllSizes(1.33333333f);
	ImNodes::StyleColorsDark();

	if (!m_ConfigSettings.mFontFile.empty() && fs::exists(m_ConfigSettings.mFontFile))
		GUI::SetFont(m_ConfigSettings.mFontFile.string());

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

	LogMessage("[Editor] initialization done");

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
		String compiler_app_cmd_line = OS::sGetExecutablePath().string() + " -asset_compiler";

		PROCESS_INFORMATION pi = {};
		STARTUPINFO si = { sizeof(si) };

		if (CreateProcessA(NULL, &compiler_app_cmd_line[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		{
			m_CompilerProcess = pi.hProcess;
#if 0
			DWORD result = WaitForInputIdle(pi.hProcess, INFINITE); // scary
			{
				auto EnumThreadWindowCallback = [](HWND hwnd, LPARAM lParam) -> BOOL 
				{
					Editor* editor = (Editor*)lParam;
					editor->m_CompilerWindow = hwnd;
					return false;
				};

				EnumThreadWindows(pi.dwThreadId, EnumThreadWindowCallback, (LPARAM)this);
			}
#endif
		}

		if (m_CompilerProcess == nullptr)
			LogMessage("[Editor] Failed to start asset compiler process.");

		if (m_CompilerWindow == nullptr)
			LogMessage("[Editor] Failed to hook asset compiler window.");
	}

	m_Camera.SetPosition(Vec3(1.0f, 1.0f, -1.0f));
	m_Camera.LookAt(Vec3(0.0f, 0.0f, 0.0f));
}


Editor::~Editor()
{
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
		m_Scene.RenderDebugShapes(GetActiveEntity());

	// update Skeleton and Animation components
	m_Scene.UpdateAnimations(inDeltaTime);

    // update NativeScript components
    if (GetGameState() == GAME_RUNNING)
        m_Scene.UpdateNativeScripts(inDeltaTime);

	// start ImGui
	GUI::BeginFrame();

	if (g_Input->IsRelativeMouseMode())
		ImGui::GetIO().MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

	if (GetConfigSettings().mShowUI)
	{
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
			BeginImGuiDockSpace();

		// draw widgets
		m_Widgets.Draw(inDeltaTime);

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
			EndImGuiDockSpace();
	}

	// detect any changes to the viewport (mainly used to reset path tracers)
	m_ViewportChanged = false;

	if (ViewportWidget* widget = m_Widgets.GetWidget<ViewportWidget>())
			m_ViewportChanged |= widget->Changed();

	// detect any changes to the scene
	if (InspectorWidget* widget = m_Widgets.GetWidget<InspectorWidget>())
		m_ViewportChanged |= widget->SceneChanged();

	// this has to check deltas in camera matrices, so only do it if the previous checks failed
	if (!m_ViewportChanged)
		m_ViewportChanged = m_Viewport.Changed();

	// ImGui::ShowDemoWindow();
	// ImGui::ShowStyleEditor();
	
	GUI::EndFrame();

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

	if (event.type == SDL_EVENT_WINDOW_MINIMIZED)
	{
		for (;;)
		{
			SDL_Event temp_event;
			SDL_PollEvent(&temp_event);

			if (temp_event.type == SDL_EVENT_WINDOW_RESTORED)
				break;
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

	if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
	{
		switch (event.key.key)
		{
			case SDLK_D:
			{
				if (g_Input->IsKeyDown(Key::LCTRL))
				{
					if (m_ActiveEntity != Entity::Null && m_ActiveEntity != m_Scene.GetRootEntity())
						SetActiveEntity(m_Scene.Clone(m_ActiveEntity));
				}
			} break;

			case SDLK_S:
			{
				if (g_Input->IsKeyDown(Key::LCTRL))
				{
					String filepath = OS::sSaveFileDialog("Scene File (*.scene)\0", "scene");

					if (!filepath.empty())
					{
						g_ThreadPool.QueueJob([this, filepath]()
						{
							LogMessage("[Editor] Saving scene...");
							m_Scene.SaveToFile(filepath, m_Assets);
							LogMessage("[Editor] Saved scene to " + fs::relative(filepath).string() + "");
						});
					}

					AddRecentScene(filepath);
				}
			} break;

			case SDLK_Z:
			{
				if (g_Input->IsKeyDown(Key::LCTRL))
					m_UndoSystem.Undo();
			} break;

			case SDLK_Y:
			{
				if (g_Input->IsKeyDown(Key::LCTRL))
					m_UndoSystem.Redo();
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

				/*m_Settings.mShowUI = !m_Settings.mShowUI;

				SDL_Event sdl_event;
				sdl_event.type = SDL_WINDOWEVENT;
				sdl_event.window.event = SDL_WINDOWEVENT_RESIZED;
				SDL_PushEvent(&sdl_event);*/
			} break;

			case SDLK_LALT:
			case SDLK_RALT:
			{
				g_Input->SetRelativeMouseMode(!g_Input->IsRelativeMouseMode());
			} break;

			case SDLK_F5:
			{
                Game::Start();
			} break;

			case SDLK_ESCAPE:
			{
                Game::Stop();
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
					std::cerr << e.what() << '\n';
				}
			}
		}
	}

	if (m_ConfigSettings.mShowUI && GetGameState() != GAME_RUNNING)
		m_Widgets.OnEvent(event);
}


void Editor::LogMessage(const String& inMessage)
{
	Application::LogMessage(inMessage);

	if (ConsoleWidget* console_widget = m_Widgets.GetWidget<ConsoleWidget>())
	{
		// Flush any pending messages
		if (!m_Messages.empty())
			for (const String& message : m_Messages)
				console_widget->LogMessage(message);

		m_Messages.clear();
		console_widget->LogMessage(inMessage);
	}
	else
	{
		m_Messages.push_back(inMessage);
	}
}


inline void Editor::SetActiveEntity(Entity inEntity) 
{
	if (inEntity != Entity::Null)
		m_Selection.Clear();

	m_ActiveEntity.store(inEntity); 
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

	//ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

	ImGuiViewport* imGuiViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(imGuiViewport->Pos);
	ImGui::SetNextWindowSize(imGuiViewport->Size);
	ImGui::SetNextWindowViewport(imGuiViewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGuiWindowClass window_class = {};
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
	ImGui::SetNextWindowClass(&window_class);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", (bool*)true, flags);
	ImGui::PopStyleVar();
	ImGui::PopStyleVar(2);

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, &window_class);

	if (m_DockSpaceID != dockspace_id && !fs::exists("imgui.ini"))
	{
		ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
		ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

		ImGuiID main_node = ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(main_node, viewport->Size);

		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<ViewportWidget>()->GetTitle().c_str(), main_node);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<ShaderGraphWidget>()->GetTitle().c_str(), main_node);

		ImGuiID SplitA, SplitB;
		ImGui::DockBuilderSplitNode(main_node, ImGuiDir_Right, 0.15f, &SplitA, &SplitB);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<HierarchyWidget>()->GetTitle().c_str(), SplitA);

		ImGuiID viewport_node_id = SplitB;
		ImGuiID hierarchy_node_id = SplitA;

		ImGui::DockBuilderSplitNode(SplitB, ImGuiDir_Down, 0.10f, &SplitA, &SplitB);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<SequenceWidget>()->GetTitle().c_str(), SplitA);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<MaterialsWidget>()->GetTitle().c_str(), SplitA);

		ImGui::DockBuilderSplitNode(hierarchy_node_id, ImGuiDir_Down, 0.75f, &SplitA, &SplitB);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<InspectorWidget>()->GetTitle().c_str(), SplitA);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<ProfileWidget>()->GetTitle().c_str(), SplitA);

		ImGui::DockBuilderSplitNode(SplitA, ImGuiDir_Down, 0.20f, &SplitA, &SplitB);
		ImGui::DockBuilderDockWindow(m_Widgets.GetWidget<ConsoleWidget>()->GetTitle().c_str(), SplitA);

		ImGui::DockBuilderFinish(main_node);
	}

	m_DockSpaceID = dockspace_id;
}


void Editor::EndImGuiDockSpace()
{
	ImGui::End();
}


} // Raekor 