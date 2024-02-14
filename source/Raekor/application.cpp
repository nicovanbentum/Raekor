#include "pch.h"
#include "application.h"

#include "OS.h"
#include "iter.h"
#include "async.h"
#include "timer.h"
#include "member.h"
#include "camera.h"
#include "archive.h"
#include "components.h"

namespace Raekor {

RTTI_DEFINE_TYPE(ConfigSettings)
{
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Display", mDisplayIndex);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Vsync", mVsyncEnabled);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "App Name", mAppName);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Font File", mFontFile);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Scene File", mSceneFile);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Recent Scene Files", mRecentScenes);
}

static constexpr auto CONFIG_FILE_STR = "config.json";

Application::Application(WindowFlags inFlags)
{
	gRegisterPrimitiveTypes();
	gRegisterComponentTypes();

	g_RTTIFactory.Register(RTTI_OF(ConfigSettings));

    JSON::ReadArchive archive(CONFIG_FILE_STR);
    archive >> m_Settings;

	SDL_SetHint("SDL_BORDERLESS_RESIZABLE_STYLE", "1");

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		LogMessage(SDL_GetError());
		std::abort();
	}

	auto displays = std::vector<SDL_Rect>(SDL_GetNumVideoDisplays());
	for (const auto& [index, display] : gEnumerate(displays))
		SDL_GetDisplayBounds(index, &display);

	// if the config setting is higher than the nr of displays we pick the default display
	m_Settings.mDisplayIndex = m_Settings.mDisplayIndex > displays.size() - 1 ? 0 : m_Settings.mDisplayIndex;
	const auto& rect = displays[m_Settings.mDisplayIndex];

	int width = int(rect.w * 0.88f);
	int height = int(rect.h * 0.88f);

	//width = 1920, height = 1080;

	m_Window = SDL_CreateWindow(
		m_Settings.mAppName.c_str(),
		SDL_WINDOWPOS_CENTERED_DISPLAY(m_Settings.mDisplayIndex),
		SDL_WINDOWPOS_CENTERED_DISPLAY(m_Settings.mDisplayIndex),
		width, height,
		inFlags | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_HIDDEN
	);

	OS::sSetDarkTitleBar(m_Window);

	SDL_SetWindowMinimumSize(m_Window, width / 4, height / 4);
	m_Viewport = Viewport(glm::vec2(width, height));

	auto quit_function = [&]()
	{
		m_Running = false;
		g_ThreadPool.WaitForJobs();
	};

	g_CVars.CreateFn("quit", quit_function);
	g_CVars.CreateFn("exit", quit_function);

	if (( inFlags & WindowFlag::HIDDEN ) == 0)
		SDL_ShowWindow(m_Window);

	SDL_AddEventWatch(OnNativeEvent, this);
}


Application::~Application()
{
	m_Settings.mDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
	auto write_archive = JSON::WriteArchive(CONFIG_FILE_STR);
	write_archive << m_Settings;

	SDL_DestroyWindow(m_Window);
	SDL_Quit();
}


void Application::Run()
{
	Timer timer;
	float dt = 0;

	while (m_Running)
	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev))
		{
			OnEvent(ev);

			if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)
					if (SDL_GetWindowID(m_Window) == ev.window.windowID)
						m_Running = false;
		}

		if (!m_Running)
			break;

		OnUpdate(dt);

		dt = timer.Restart();

		m_FrameCounter++;
	}
}


int Application::OnNativeEvent(void* inUserData, SDL_Event* inEvent)
{
	auto app = (Application*)inUserData;

	if (inEvent && inEvent->type == SDL_SYSWMEVENT)
	{
		auto& win_msg = inEvent->syswm.msg->msg.win;

		if (win_msg.msg == WM_COPYDATA)
		{
			const auto copied_data = (PCOPYDATASTRUCT)win_msg.lParam;
			constexpr auto LOG_MESSAGE = 1u;

			if (copied_data->dwData == IPC::LOG_MESSAGE_SENT)
			{
				app->LogMessage((const char*)copied_data->lpData);
			}
		}
	}

	return 0;
}


bool Application::IsWindowBorderless() const
{
	const auto window_flags = SDL_GetWindowFlags(m_Window);
	return ( ( window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP ) == SDL_WINDOW_FULLSCREEN_DESKTOP );
}


bool Application::IsWindowExclusiveFullscreen() const
{
	const auto window_flags = SDL_GetWindowFlags(m_Window);
	return ( ( window_flags & SDL_WINDOW_FULLSCREEN ) && !IsWindowBorderless() );
}


void Application::AddRecentScene(const Path& inPath) 
{ 
	const auto cMaxSize = 5u;

	std::vector<Path> new_paths;
	new_paths.reserve(cMaxSize);

	new_paths.push_back(inPath);

	for (const auto& path : m_Settings.mRecentScenes)
	{
		if (path == inPath)
			continue;

		new_paths.push_back(path);
	}

	if (new_paths.size() > cMaxSize)
		new_paths.pop_back();

	m_Settings.mRecentScenes = new_paths;
}

} // namespace Raekor  