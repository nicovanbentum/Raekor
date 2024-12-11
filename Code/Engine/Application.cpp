#include "PCH.h"
#include "Application.h"
#include "Components.h"
#include "Threading.h"
#include "Profiler.h"
#include "Archive.h"
#include "Camera.h"
#include "Member.h"
#include "Input.h"
#include "Timer.h"
#include "iter.h"
#include "OS.h"

namespace RK {

RTTI_DEFINE_TYPE(ConfigSettings)
{
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Display", mDisplayIndex);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Vsync", mVsyncEnabled);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "App Name", mAppName);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Font File", mFontFile);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Scene File", mSceneFile);
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Recent Scene Files", mRecentScenes);
}

static constexpr const char* CONFIG_FILE_STR = "config.json";

Application::Application(WindowFlags inFlags)
{
	gRegisterPrimitiveTypes();
	gRegisterComponentTypes();

	g_RTTIFactory.Register(RTTI_OF(ConfigSettings));

	if (OS::sCheckCommandLineOption("-run_tests"))
	{
		RunArchiveTests();
		RunECStorageTests();
	}

    JSON::ReadArchive archive(CONFIG_FILE_STR);
    archive >> m_Settings;

	SDL_SetHint("SDL_BORDERLESS_RESIZABLE_STYLE", "1");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
	{
		LogMessage(SDL_GetError());
		std::abort();
	}

	Array<SDL_Rect> displays(SDL_GetNumVideoDisplays());
	assert(!displays.empty());

	for (const auto& [index, display] : gEnumerate(displays))
		SDL_GetDisplayBounds(index, &display);

	// if the config setting is higher than the nr of displays we pick the default display
	m_Settings.mDisplayIndex = m_Settings.mDisplayIndex > displays.size() - 1 ? 0 : m_Settings.mDisplayIndex;
	const SDL_Rect& rect = displays[m_Settings.mDisplayIndex];

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
	m_Viewport.SetRenderSize(UVec2(width, height));
	m_Viewport.SetDisplaySize(UVec2(width, height));

	auto quit_function = [&]()
	{
		m_Running = false;
		g_ThreadPool.WaitForJobs();
	};

	g_CVariables->CreateFn("quit", quit_function);
	g_CVariables->CreateFn("exit", quit_function);

	if (( inFlags & WindowFlag::HIDDEN ) == 0)
		SDL_ShowWindow(m_Window);

	SDL_AddEventWatch(OnNativeEvent, this);
}


Application::~Application()
{
	m_Settings.mDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
	JSON::WriteArchive write_archive(CONFIG_FILE_STR);
	write_archive << m_Settings;

	SDL_DestroyWindow(m_Window);
	SDL_Quit();
}


void RandomlyResizeWindow(SDL_Window* inWindow)
{
	const int current_display_index = SDL_GetWindowDisplayIndex(inWindow);
	const int num_display_modes = SDL_GetNumDisplayModes(current_display_index);

	Array<SDL_DisplayMode> display_modes(num_display_modes);

	for (const auto& [index, display_mode] : gEnumerate(display_modes))
		SDL_GetDisplayMode(current_display_index, index, &display_mode);

	const SDL_DisplayMode& mode = display_modes[gRandomUInt() % num_display_modes];

	SDL_SetWindowSize(inWindow, mode.w, mode.h);

	std::cout << std::format("Resizing Window to {}x{}\n", mode.w, mode.h);
}


void Application::Run()
{
	Timer timer;
	float dt = 0;

	static bool do_resize_test = OS::sCheckCommandLineOption("-resize_test");

	while (m_Running)
	{
		if (do_resize_test && (m_FrameCounter % 100) == 0)
			RandomlyResizeWindow(m_Window);

		SDL_Event ev;
		while (SDL_PollEvent(&ev))
		{
			g_Input->OnEvent(ev);
			
			OnEvent(ev);

			if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)
					if (SDL_GetWindowID(m_Window) == ev.window.windowID)
						m_Running = false;
		}

		if (!m_Running)
			break;

		g_Input->OnUpdate(dt);

		OnUpdate(dt);

		dt = timer.Restart();

		m_FrameCounter++;
	}
}


int Application::OnNativeEvent(void* inUserData, SDL_Event* inEvent)
{
	Application* app = (Application*)inUserData;

	if (inEvent && inEvent->type == SDL_SYSWMEVENT)
	{
		auto& win_msg = inEvent->syswm.msg->msg.win;

		if (win_msg.msg == WM_COPYDATA)
		{
			const PCOPYDATASTRUCT copied_data = (PCOPYDATASTRUCT)win_msg.lParam;

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
	const Uint32 window_flags = SDL_GetWindowFlags(m_Window);
	return ( ( window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP ) == SDL_WINDOW_FULLSCREEN_DESKTOP );
}


bool Application::IsWindowExclusiveFullscreen() const
{
	const Uint32 window_flags = SDL_GetWindowFlags(m_Window);
	return ( ( window_flags & SDL_WINDOW_FULLSCREEN ) && !IsWindowBorderless() );
}


void Application::AddRecentScene(const Path& inPath) 
{ 
	static constexpr int cMaxSize = 5;

	Array<Path> new_paths;
	new_paths.reserve(cMaxSize);

	new_paths.push_back(inPath);

	for (const Path& path : m_Settings.mRecentScenes)
	{
		if (path == inPath)
			continue;

		if (path.empty())
			continue;

		new_paths.push_back(path);
	}

	if (new_paths.size() > cMaxSize)
		new_paths.pop_back();

	m_Settings.mRecentScenes = new_paths;
}


void IRenderInterface::UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets)
{
	assert(Material::Default.IsLoaded() && "Default material not loaded, did the programmer forget to initialize its gpu maps before opening a scene?");

	auto UploadTexture = [&](const String& inFile, bool inIsSRGB, uint8_t inSwizzle, uint32_t inDefaultMap, uint32_t& ioGpuMap)
	{
		if (TextureAsset::Ptr asset = inAssets.GetAsset<TextureAsset>(inFile))
			ioGpuMap = UploadTextureFromAsset(asset, inIsSRGB, inSwizzle);
		else
			ioGpuMap = inDefaultMap;
	};

	UploadTexture(inMaterial.albedoFile, true, inMaterial.gpuAlbedoMapSwizzle, Material::Default.gpuAlbedoMap, inMaterial.gpuAlbedoMap);
	UploadTexture(inMaterial.normalFile, false, inMaterial.gpuNormalMapSwizzle, Material::Default.gpuNormalMap, inMaterial.gpuNormalMap);
	UploadTexture(inMaterial.emissiveFile, false, inMaterial.gpuEmissiveMapSwizzle, Material::Default.gpuEmissiveMap, inMaterial.gpuEmissiveMap);
	UploadTexture(inMaterial.metallicFile, false, inMaterial.gpuMetallicMapSwizzle, Material::Default.gpuMetallicMap, inMaterial.gpuMetallicMap);
	UploadTexture(inMaterial.roughnessFile, false, inMaterial.gpuRoughnessMapSwizzle, Material::Default.gpuRoughnessMap, inMaterial.gpuRoughnessMap);
}

} // namespace Raekor  