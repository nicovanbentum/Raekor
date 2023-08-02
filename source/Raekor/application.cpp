#include "pch.h"
#include "application.h"

#include "OS.h"
#include "util.h"
#include "async.h"
#include "timer.h"
#include "camera.h"
#include "archive.h"

namespace Raekor {

RTTI_CLASS_CPP(ConfigSettings) {
    RTTI_MEMBER_CPP(ConfigSettings, SERIALIZE_ALL, "Display",    mDisplayIndex);
    RTTI_MEMBER_CPP(ConfigSettings, SERIALIZE_ALL, "Vsync",      mVsyncEnabled);
    RTTI_MEMBER_CPP(ConfigSettings, SERIALIZE_ALL, "App Name",   mAppName);
    RTTI_MEMBER_CPP(ConfigSettings, SERIALIZE_ALL, "Font File",  mFontFile);
    RTTI_MEMBER_CPP(ConfigSettings, SERIALIZE_ALL, "Scene File", mSceneFile);
    RTTI_MEMBER_CPP(ConfigSettings, SERIALIZE_ALL, "My Array",   mArray);
    RTTI_MEMBER_CPP(ConfigSettings, SERIALIZE_ALL, "My Vector",  mVector);
}

static constexpr auto CONFIG_FILE_STR = "config.json";

Application::Application(WindowFlags inFlags) {
    auto json_data = JSON::JSONData(CONFIG_FILE_STR);
    auto read_archive = JSON::ReadArchive(json_data);
    
    if (!json_data.IsEmpty())
        read_archive >> m_Settings;

    SDL_SetHint("SDL_BORDERLESS_RESIZABLE_STYLE", "1");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LogMessage(SDL_GetError());
        std::abort();
    }

    auto displays = std::vector<SDL_Rect>(SDL_GetNumVideoDisplays());
    for (const auto& [index, display] : gEnumerate(displays))
        SDL_GetDisplayBounds(index, &display);

    // if the config setting is higher than the nr of displays we pick the default display
    m_Settings.mDisplayIndex = m_Settings.mDisplayIndex > displays.size() - 1 ? 0 : m_Settings.mDisplayIndex;
    const auto& rect = displays[m_Settings.mDisplayIndex];

    int width = int(rect.w * 0.9f);
    int height = int(rect.h * 0.9f);

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

    auto quit_function = [&]() {
        m_Running = false;
        g_ThreadPool.WaitForJobs();
    };

    g_CVars.CreateFn("quit", quit_function);
    g_CVars.CreateFn("exit", quit_function);

    if (inFlags != WindowFlag::HIDDEN)
        SDL_ShowWindow(m_Window);
}


Application::~Application() {
    m_Settings.mDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
    auto write_archive = JSON::WriteArchive(CONFIG_FILE_STR);
    write_archive << m_Settings;

    SDL_DestroyWindow(m_Window);
    SDL_Quit();
}



void Application::Run() {
    Timer timer;
    float dt = 0;

    while (m_Running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            OnEvent(ev);

            if (ev.type == SDL_WINDOWEVENT) {
                if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    for (;;) {
                        auto temp_event = SDL_Event{};
                        SDL_PollEvent(&temp_event);

                        if (temp_event.window.event == SDL_WINDOWEVENT_RESTORED)
                            break;
                    }
                }
                if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                    if (SDL_GetWindowID(m_Window) == ev.window.windowID)
                        m_Running = false;
                }
            }
        }

        if (!m_Running)
            break;

        OnUpdate(dt);

        dt = timer.Restart();
    }
}

} // namespace Raekor  