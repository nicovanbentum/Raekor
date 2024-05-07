#include "pch.h"
#include "editor.h"

#include "OS.h"
#include "rmath.h"
#include "input.h"
#include "timer.h"
#include "debug.h"
#include "script.h"
#include "profile.h"
#include "components.h"
#include "shadernodes.h"

#include "widgets/assetsWidget.h"
#include "widgets/menubarWidget.h"
#include "widgets/consoleWidget.h"
#include "widgets/ProfileWidget.h"
#include "widgets/viewportWidget.h"
#include "widgets/SequenceWidget.h"
#include "widgets/inspectorWidget.h"
#include "widgets/hierarchyWidget.h"
#include "widgets/NodeGraphWidget.h"

namespace RK {

IEditor::IEditor(WindowFlags inWindowFlags, IRenderInterface* inRenderInterface) :
	Application(inWindowFlags /* | WindowFlag::BORDERLESS */),
	m_Scene(inRenderInterface),
	m_Physics(inRenderInterface),
	m_RenderInterface(inRenderInterface)
{
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
	io.ConfigDockingWithShift = true;

	GUI::SetTheme();
	ImGui::GetStyle().ScaleAllSizes(1.33333333f);
	ImNodes::StyleColorsDark();

	if (!m_Settings.mFontFile.empty() && fs::exists(m_Settings.mFontFile))
		GUI::SetFont(m_Settings.mFontFile.string());

	if (OS::sCheckCommandLineOption("-shader_editor"))
	{
		m_Widgets.Register<ShaderGraphWidget>(this);
	}
	else
	{
		m_Widgets.Register<SequenceWidget>(this);
		m_Widgets.Register<ComponentsWidget>(this);
		m_Widgets.Register<MenubarWidget>(this);
		m_Widgets.Register<ConsoleWidget>(this);
		m_Widgets.Register<ShaderGraphWidget>(this);
		m_Widgets.Register<ViewportWidget>(this);
		m_Widgets.Register<ProfileWidget>(this);
		m_Widgets.Register<InspectorWidget>(this);
		m_Widgets.Register<HierarchyWidget>(this);
	}

	LogMessage("[Editor] initialization done.");

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

	if (g_CVars.Create("launch_asset_compiler_on_startup", 0))
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
					IEditor* editor = (IEditor*)lParam;
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
}


IEditor::~IEditor()
{
	if (m_CompilerProcess)
	{
		TerminateProcess(m_CompilerProcess, 0);
		CloseHandle(m_CompilerProcess);
	}
}


void IEditor::OnUpdate(float inDeltaTime)
{
	// clear all profile sections
	g_Profiler->Reset();

	PROFILE_FUNCTION_CPU();

	// clear the debug renderer vertex buffers
	g_DebugRenderer.Reset();

	// check if any BoxCollider's are waiting to be registered
	m_Physics.OnUpdate(m_Scene);

	// step the physics simulation
	if (m_Physics.GetState() == Physics::Stepping)
		m_Physics.Step(m_Scene, inDeltaTime);

    // Apply sequence to camera
    if (SequenceWidget* sequence_widget = m_Widgets.GetWidget<SequenceWidget>())
    {
        if (sequence_widget->IsLockedToCamera())
            sequence_widget->ApplyToCamera(m_Viewport.GetCamera(), inDeltaTime);
    }

	// if the game has not taken over the camera, use the editor controls
	if (m_GameState != GAME_RUNNING)
		EditorCameraController::OnUpdate(m_Viewport.GetCamera(), inDeltaTime);

	// update camera matrices
	m_Viewport.OnUpdate(inDeltaTime);

	static int& update_transforms = g_CVars.Create("update_transforms", 1, true);

	if (update_transforms)
	{
		// update Transform components
		m_Scene.UpdateTransforms();

		// update Light and DirectionalLight components
		m_Scene.UpdateLights();
	}

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

	if (GetSettings().mShowUI)
	{
		GUI::BeginDockSpace(m_Widgets);

		// draw widgets
		m_Widgets.Draw(inDeltaTime);

		// end ImGui
		GUI::EndDockSpace();
	}

	// detect any changes to the viewport (mainly used to reset path tracers)
	m_ViewportChanged = false;
	if (ViewportWidget* widget = m_Widgets.GetWidget<ViewportWidget>())
		if (widget->Changed())
			m_ViewportChanged = true;

	// this has to check deltas in camera matrices, so only do it if the previous check failed
	if (!m_ViewportChanged)
		m_ViewportChanged = m_Viewport.GetCamera().Changed();

	// ImGui::ShowDemoWindow();
	//ImGui::ShowStyleEditor();
	
	GUI::EndFrame();

	// Applications that implement IEditor should call IEditor::OnUpdate first, then do their own rendering
}



void IEditor::OnEvent(const SDL_Event& event)
{
	ImGui_ImplSDL2_ProcessEvent(&event);

	if (m_GameState != GAME_RUNNING)
	{
		if (const ViewportWidget* viewport_widget = m_Widgets.GetWidget<ViewportWidget>())
		{
			if (viewport_widget->IsHovered() || SDL_GetRelativeMouseMode() || !m_Settings.mShowUI)
				EditorCameraController::OnEvent(m_Viewport.GetCamera(), event);
		}
	}

	if (event.type == SDL_WINDOWEVENT)
	{
		if (event.window.event == SDL_WINDOWEVENT_MINIMIZED)
		{
			for (;;)
			{
				auto temp_event = SDL_Event {};
				SDL_PollEvent(&temp_event);

				if (temp_event.window.event == SDL_WINDOWEVENT_RESTORED)
					break;
			}
		}

		if (event.window.event == SDL_WINDOWEVENT_RESIZED)
		{
			if (!m_Settings.mShowUI)
			{
				int w, h;
				SDL_GetWindowSize(m_Window, &w, &h);
				m_Viewport.SetDisplaySize(UVec2(w, h));
			}
		}
	}

	if (event.type == SDL_KEYDOWN && !event.key.repeat)
	{
		switch (event.key.keysym.sym)
		{
			case SDLK_d:
			{
				if (SDL_GetModState() & KMOD_LCTRL)
				{
					if (m_ActiveEntity != Entity::Null && m_ActiveEntity != m_Scene.GetRootEntity())
						SetActiveEntity(m_Scene.Clone(m_ActiveEntity));
				}
			} break;

			case SDLK_s:
			{
				if (SDL_GetModState() & KMOD_LCTRL)
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

			case SDLK_F1:
			{
				m_Settings.mShowUI = !m_Settings.mShowUI;

				SDL_Event sdl_event;
				sdl_event.type = SDL_WINDOWEVENT;
				sdl_event.window.event = SDL_WINDOWEVENT_RESIZED;
				SDL_PushEvent(&sdl_event);
			} break;

			case SDLK_LALT:
			case SDLK_RALT:
			{
				g_Input->SetRelativeMouseMode(!g_Input->IsRelativeMouseMode());
			} break;

			case SDLK_ESCAPE:
			{
				if (m_Physics.GetState() != Physics::Idle)
				{
					m_Physics.RestoreState();
					m_Physics.Step(m_Scene, 1.0f / 60.0f);
					m_Physics.SetState(Physics::Idle);

				}

				EGameState state = GetGameState();
				SetGameState(GAME_STOPPED);

				if (state != GAME_STOPPED)
				{
					for (auto [entity, script] : m_Scene.Each<NativeScript>())
					{
						if (script.script)
						{
							try
							{
								script.script->OnStop();
							}
							catch (std::exception& e)
							{
								std::cout << e.what() << '\n';
							}
						}
					}
				}

				g_Input->SetRelativeMouseMode(false);

			} break;

			case SDLK_F5:
			{
				EGameState state = GetGameState();
				SetGameState(GAME_RUNNING);

				m_Physics.SetState(Physics::Stepping);

				if (state != GAME_RUNNING)
				{
					for (auto [entity, script] : m_Scene.Each<NativeScript>())
					{
						if (script.script)
						{
							try
							{
								script.script->OnStart();
							}
							catch (std::exception& e)
							{
								std::cout << e.what() << '\n';
							}
						}
					}
				}

				g_Input->SetRelativeMouseMode(true);

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

	if (m_Settings.mShowUI)
		m_Widgets.OnEvent(event);
}


void IEditor::LogMessage(const String& inMessage)
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

} // Raekor 