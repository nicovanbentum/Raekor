#include "pch.h"
#include "editor.h"

#include "OS.h"
#include "rmath.h"
#include "timer.h"
#include "debug.h"
#include "systems.h"
#include "components.h"

#include "widgets/assetsWidget.h"
#include "widgets/menubarWidget.h"
#include "widgets/consoleWidget.h"
#include "widgets/viewportWidget.h"
#include "widgets/inspectorWidget.h"
#include "widgets/hierarchyWidget.h"
#include "widgets/NodeGraphWidget.h"

namespace Raekor {

IEditor::IEditor(WindowFlags inWindowFlags, IRenderInterface* inRenderInterface) :
	Application(inWindowFlags /* | WindowFlag::BORDERLESS */),
	m_Scene(inRenderInterface),
	m_Physics(inRenderInterface),
	m_RenderInterface(inRenderInterface)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImNodes::CreateContext();
	ImGui::StyleColorsDark();

	// get GUI i/o and set a bunch of settings
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	// io.ConfigDockingWithShift = true;

	GUI::SetTheme();
	ImGui::GetStyle().ScaleAllSizes(1.33333333f);
	ImNodes::StyleColorsDark();

	if (!m_Settings.mFontFile.empty())
		GUI::SetFont(m_Settings.mFontFile.string());

	m_Widgets.Register<AssetsWidget>(this);
	m_Widgets.Register<MenubarWidget>(this);
	m_Widgets.Register<ConsoleWidget>(this);
	m_Widgets.Register<NodeGraphWidget>(this);
	m_Widgets.Register<ViewportWidget>(this);
	m_Widgets.Register<InspectorWidget>(this);
	m_Widgets.Register<HierarchyWidget>(this);

	LogMessage("[Editor] initialization done.");

	// sponza specific
	m_Viewport.GetCamera().Move(Vec2(-42.0f, 10.0f));
	m_Viewport.GetCamera().Zoom(0.0f);
	m_Viewport.GetCamera().Look(Vec2(-1.65f, 0.0f));
	m_Viewport.SetFieldOfView(68.0f);

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
		auto compiler_app_cmd_line = OS::sGetExecutablePath().string() + " -asset_compiler";

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
	// clear the debug renderer vertex buffers
	g_DebugRenderer.Reset();

	// check if any BoxCollider's are waiting to be registered
	m_Physics.OnUpdate(m_Scene);

	// step the physics simulation
	if (m_Physics.GetState() == Physics::Stepping)
		m_Physics.Step(m_Scene, inDeltaTime);

	// update camera matrices
	m_Viewport.OnUpdate(inDeltaTime);

	static auto& update_transforms = g_CVars.Create("update_transforms", 1, true);

	if (update_transforms)
	{
		// update Transform components
		m_Scene.UpdateTransforms();

		// update Light and DirectionalLight components
		m_Scene.UpdateLights();
	}

	// render any scene dependent debug shapes
	if (GetActiveEntity() != NULL_ENTITY)
		m_Scene.RenderDebugShapes(GetActiveEntity());

	// update Skeleton and Animation components
	m_Scene.UpdateAnimations(inDeltaTime);

	// update NativeScript components
	m_Scene.UpdateNativeScripts(inDeltaTime);

	// start ImGui
	GUI::BeginFrame();

	if (SDL_GetRelativeMouseMode())
		ImGui::GetIO().MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

	if (GetSettings().mShowUI)
	{
		auto dockspace_flags = ImGuiWindowFlags(ImGuiWindowFlags_None);

		if (auto menubar_widget = m_Widgets.GetWidget<MenubarWidget>())
			if (menubar_widget->IsOpen())
				dockspace_flags |= ImGuiWindowFlags_MenuBar;

		GUI::BeginDockSpace(dockspace_flags);

		// draw widgets
		m_Widgets.Draw(inDeltaTime);

		// end ImGui
		GUI::EndDockSpace();
	}

	// detect any changes to the viewport (mainly used to reset path tracers)
	m_ViewportChanged = false;
	if (auto widget = m_Widgets.GetWidget<ViewportWidget>())
		if (widget->Changed())
			m_ViewportChanged = true;

	// this has to check deltas in camera matrices, so only do it if the previous check failed
	if (!m_ViewportChanged)
		m_ViewportChanged = m_Viewport.GetCamera().Changed();

	// ImGui::ShowDemoWindow();
	// ImGui::ShowStyleEditor();
	
	GUI::EndFrame();

	// Applications that implement IEditor should call IEditor::OnUpdate first, then do their own rendering
}



void IEditor::OnEvent(const SDL_Event& event)
{
	ImGui_ImplSDL2_ProcessEvent(&event);

	if ((m_Widgets.GetWidget<ViewportWidget>()->IsHovered() || SDL_GetRelativeMouseMode()) || !m_Settings.mShowUI)
		CameraController::OnEvent(m_Viewport.GetCamera(), event);

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
			case SDLK_DELETE:
			{
				if (m_ActiveEntity != NULL_ENTITY)
				{
					if (m_Scene.Has<Node>(m_ActiveEntity))
					{
						auto tree = NodeSystem::sGetFlatHierarchy(m_Scene, m_ActiveEntity);

						for (auto entity : tree)
						{
							NodeSystem::sRemove(m_Scene, m_Scene.Get<Node>(entity));
							m_Scene.Destroy(entity);
						}

						NodeSystem::sRemove(m_Scene, m_Scene.Get<Node>(m_ActiveEntity));
					}

					m_Scene.Destroy(m_ActiveEntity);
					m_ActiveEntity = NULL_ENTITY;
				}
			} break;
			case SDLK_d:
			{
				if (SDL_GetModState() & KMOD_LCTRL)
				{
					if (m_ActiveEntity != NULL_ENTITY)
					{
						SetActiveEntity(m_Scene.Clone(m_ActiveEntity));
					}
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
		}
	}

	if (m_Settings.mShowUI)
		m_Widgets.OnEvent(event);
}


void IEditor::LogMessage(const std::string& inMessage)
{
	Application::LogMessage(inMessage);

	auto console_widget = m_Widgets.GetWidget<ConsoleWidget>();
	if (console_widget)
	{
		// Flush any pending messages
		if (!m_Messages.empty())
			for (const auto& message : m_Messages)
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