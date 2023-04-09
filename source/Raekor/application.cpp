#include "pch.h"
#include "application.h"

#include "OS.h"
#include "util.h"
#include "timer.h"
#include "camera.h"

namespace Raekor {

Application::Application(WindowFlags inFlags) {
    {   // scoped to make sure it flushes
        std::ifstream is("config.json");
        cereal::JSONInputArchive archive(is);
        m_Settings.serialize(archive);
    }

    SDL_SetHint("SDL_BORDERLESS_RESIZABLE_STYLE", "1");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << SDL_GetError() << '\n';
        std::abort();
    }

    auto displays = std::vector<SDL_Rect>(SDL_GetNumVideoDisplays());
    for (auto& [index, display] : gEnumerate(displays))
        SDL_GetDisplayBounds(index, &display);

    // if the config setting is higher than the nr of displays we pick the default display
    m_Settings.display = m_Settings.display > displays.size() - 1 ? 0 : m_Settings.display;
    const auto& rect = displays[m_Settings.display];

    int width = int(rect.w * 0.9f);
    int height = int(rect.h * 0.9f);

    width = 1920, height = 1080;

    m_Window = SDL_CreateWindow(
        m_Settings.name.c_str(),
        SDL_WINDOWPOS_CENTERED_DISPLAY(m_Settings.display),
        SDL_WINDOWPOS_CENTERED_DISPLAY(m_Settings.display),
        width, height,
        inFlags | SDL_WINDOW_INPUT_FOCUS
    );

    OS::sSetDarkTitleBar(m_Window);

    SDL_SetWindowMinimumSize(m_Window, width / 4, height / 4);
    m_Viewport = Viewport(glm::vec2(width, height));

    auto quit_function = [&]() {
        m_Running = false;
    };

    CVars::sCreateFn("quit", quit_function);
    CVars::sCreateFn("exit", quit_function);

    {
        std::ofstream is("config.json");
        cereal::JSONOutputArchive archive(is);
        m_Settings.serialize(archive);
    }
}


Application::~Application() {
    m_Settings.display = SDL_GetWindowDisplayIndex(m_Window);
    SDL_DestroyWindow(m_Window);
    SDL_Quit();

    {   // scoped to make sure it flushes
        std::ofstream is("config.json");
        cereal::JSONOutputArchive archive(is);
        m_Settings.serialize(archive);
    }
}


void Application::Run() {
    Timer timer;
    float dt = 0;

    while (m_Running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            OnEvent(ev);
        }

        if (!m_Running)
            break;

        OnUpdate(dt);

        dt = timer.Restart();
    }
}

} // namespace Raekor  