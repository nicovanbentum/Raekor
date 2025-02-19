#include "PCH.h"
#include "Application.h"
#include "Components.h"
#include "Threading.h"
#include "Profiler.h"
#include "Physics.h"
#include "Archive.h"
#include "Script.h"
#include "Camera.h"
#include "Member.h"
#include "Input.h"
#include "Timer.h"
#include "iter.h"
#include "OS.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(Application)
{
}

RTTI_DEFINE_TYPE(ConfigSettings)
{
	RTTI_DEFINE_MEMBER(ConfigSettings, SERIALIZE_ALL, "Display", mDisplayID);
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

	g_RTTIFactory.Register(RTTI_OF<ConfigSettings>());

	if (OS::sCheckCommandLineOption("-run_tests"))
	{
		RunArchiveTests();
	}

	RunECStorageTests();

    JSON::ReadArchive archive(CONFIG_FILE_STR);
    archive >> m_ConfigSettings;

	SDL_SetHint("SDL_BORDERLESS_RESIZABLE_STYLE", "1");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
	{
		LogMessage(SDL_GetError());
		std::abort();
	}

    int num_displays = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&num_displays);
	assert(num_displays > 0);

    bool display_found = false;
    for (int i = 0; i < num_displays; i++)
    {
        if (m_ConfigSettings.mDisplayID == displays[i])
        {
            display_found = true;
            break;
        }
    }

	// if the config setting is higher than the nr of displays we pick the default display
    m_ConfigSettings.mDisplayID = display_found ? m_ConfigSettings.mDisplayID : displays[0];

    SDL_Rect display_rect = {};
    SDL_GetDisplayBounds(m_ConfigSettings.mDisplayID, &display_rect);

	int width = int(display_rect.w * 0.88f);
	int height = int(display_rect.h * 0.88f);

	//width = 1920, height = 1080;

	m_Window = SDL_CreateWindow(
		m_ConfigSettings.mAppName.c_str(),
		width, height,
		inFlags | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_HIDDEN
	);

    SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_ConfigSettings.mDisplayID),
                                    SDL_WINDOWPOS_CENTERED_DISPLAY(m_ConfigSettings.mDisplayID));

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

    // SDL_SetWindowsMessageHook(OnNativeEvent, this);

	m_DiscordRPC.Init(this);
}


Application::~Application()
{
	m_DiscordRPC.Destroy();

	m_ConfigSettings.mDisplayID = SDL_GetDisplayForWindow(m_Window);
	JSON::WriteArchive write_archive(CONFIG_FILE_STR);
	write_archive << m_ConfigSettings;

	SDL_DestroyWindow(m_Window);
	SDL_Quit();
}


void RandomlyResizeWindow(SDL_Window* inWindow)
{
    int num_display_modes = 0;
    SDL_DisplayMode** display_modes = SDL_GetFullscreenDisplayModes(SDL_GetDisplayForWindow(inWindow), &num_display_modes);

	const SDL_DisplayMode* mode = display_modes[gRandomUInt() % num_display_modes];

	SDL_SetWindowSize(inWindow, mode->w, mode->h);

	std::cout << std::format("Resizing Window to {}x{}\n", mode->w, mode->h);
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

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			g_Input->OnEvent(event);
			
			OnEvent(event);

			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
				if (SDL_GetWindowID(m_Window) == event.window.windowID)
				    m_Running = false;
		}

		if (!m_Running)
			break;

		g_Input->OnUpdate(dt);

		OnUpdate(dt);

		m_DiscordRPC.OnUpdate();

		dt = timer.Restart();

		m_FrameCounter++;
	}
}


bool Application::OnNativeEvent(void* inUserData, MSG* inMessage)
{
	Application* app = (Application*)inUserData;

	if (inMessage->message == WM_COPYDATA)
	{
		const PCOPYDATASTRUCT copied_data = (PCOPYDATASTRUCT)inMessage->lParam;

		if (copied_data->dwData == IPC::LOG_MESSAGE_SENT)
		{
			app->LogMessage((const char*)copied_data->lpData);
		}
	}

	return 0;
}


bool Application::IsWindowBorderless() const
{
    const Uint32 window_flags = SDL_GetWindowFlags(m_Window);
    const SDL_DisplayMode* mode = SDL_GetWindowFullscreenMode(m_Window);
    return ( ( window_flags & SDL_WINDOW_FULLSCREEN ) && mode == NULL );
}


bool Application::IsWindowExclusiveFullscreen() const
{
	const Uint32 window_flags = SDL_GetWindowFlags(m_Window);
    const SDL_DisplayMode* mode = SDL_GetWindowFullscreenMode(m_Window);
	return ( ( window_flags & SDL_WINDOW_FULLSCREEN ) && mode != NULL );
}


void Application::AddRecentScene(const Path& inPath) 
{ 
	static constexpr int cMaxSize = 5;

	Array<Path> new_paths;
	new_paths.reserve(cMaxSize);

	new_paths.push_back(inPath);

	for (const Path& path : m_ConfigSettings.mRecentScenes)
	{
		if (path == inPath)
			continue;

		if (path.empty())
			continue;

		new_paths.push_back(path);
	}

	if (new_paths.size() > cMaxSize)
		new_paths.pop_back();

	m_ConfigSettings.mRecentScenes = new_paths;
}


void Game::Start()
{
    if (GetGameState() == GAME_RUNNING)
        return;

    if (Physics* physics = GetPhysics())
    {
        physics->SaveState();
        physics->SetState(Physics::Stepping);
    }

    if (Scene* scene = GetScene())
    {
        for (auto [entity, script] : scene->Each<NativeScript>())
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

    SetGameState(GAME_RUNNING);
}


void Game::Pause()
{
    if (GetGameState() != GAME_RUNNING)
        return;

    if (Physics* physics = GetPhysics())
        physics->SetState(Physics::Idle);

    g_Input->SetRelativeMouseMode(false);

    SetGameState(GAME_PAUSED);
}


void Game::Unpause()
{
    if (GetGameState() != GAME_PAUSED)
        return;

    if (Physics* physics = GetPhysics())
        physics->SetState(Physics::Stepping);

    g_Input->SetRelativeMouseMode(true);

    SetGameState(GAME_RUNNING);
}


void Game::Stop()
{
    if (GetGameState() == GAME_STOPPED)
        return;

    if (Physics* physics = GetPhysics())
    {
        if (physics->GetState() != Physics::Idle)
        {
            physics->RestoreState();
            physics->SetState(Physics::Idle);

            if (Scene* scene = GetScene())
                physics->Step(*scene, 1.0f / 60.0f);
        }
    }

    if (Scene* scene = GetScene())
    {
        for (auto [entity, script] : scene->Each<NativeScript>())
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

    SetCameraEntity(Entity::Null);

    g_Input->SetRelativeMouseMode(false);

    SetGameState(GAME_STOPPED);
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