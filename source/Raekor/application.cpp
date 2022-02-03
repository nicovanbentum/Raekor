#include "pch.h"
#include "application.h"

#include "util.h"
#include "timer.h"
#include "camera.h"

namespace Raekor {

Application::Application(RendererFlags flag) {
    {   // scoped to make sure it flushes
        std::ifstream is("config.json");
        cereal::JSONInputArchive archive(is);
        settings.serialize(archive);
    }

    int sdlError = SDL_Init(SDL_INIT_VIDEO);
    
    if (sdlError != 0) {
        std::cout << SDL_GetError() << '\n';
        abort();
    }

    Uint32 wflags = SDL_WINDOW_RESIZABLE | flag |
        SDL_WINDOW_ALLOW_HIGHDPI;

    std::vector<SDL_Rect> displays(SDL_GetNumVideoDisplays());
    for (size_t i = 0; i < displays.size(); i++) {
        SDL_GetDisplayBounds(int(i), &displays[i]);
    }

    // if the config setting is higher than the nr of displays we pick the default display
    settings.display = settings.display > displays.size() - 1 ? 0 : settings.display;
    const auto& rect = displays[settings.display];

    const int width = int(rect.w * 0.9f);
    const int height = int(rect.h * 0.9f);
    
    window = SDL_CreateWindow(
        settings.name.c_str(),
        SDL_WINDOWPOS_CENTERED_DISPLAY(settings.display),
        SDL_WINDOWPOS_CENTERED_DISPLAY(settings.display),
        width, height,
        SDL_WINDOW_RESIZABLE | flag | SDL_WINDOW_ALLOW_HIGHDPI
    );
    
    SDL_SetWindowMinimumSize(window, width / 4, height / 4);
    viewport = Viewport(glm::vec2(width, height));

    auto quit_function = [&]() {
        running = false;
    };

    ConVars::func("quit", quit_function);
    ConVars::func("exit", quit_function);

    {
        std::ofstream is("config.json");
        cereal::JSONOutputArchive archive(is);
        settings.serialize(archive);
    }
}



Application::~Application() {
    settings.display = SDL_GetWindowDisplayIndex(window);
    SDL_DestroyWindow(window);
    SDL_Quit();

    {   // scoped to make sure it flushes
        std::ofstream is("config.json");
        cereal::JSONOutputArchive archive(is);
        settings.serialize(archive);
    }
}



void Application::run() {
    Timer timer;
    float dt = 0;

    while (running) {
        timer.start();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            onEvent(ev);
        }

        onUpdate(dt);

        dt = float(timer.stop());
    }
}

} // namespace Raekor  