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

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if the config setting is higher than the nr of displays we pick the default display
    settings.display = settings.display > displays.size() - 1 ? 0 : settings.display;
    
    window = SDL_CreateWindow(
        settings.name.c_str(),
        SDL_WINDOWPOS_CENTERED_DISPLAY(settings.display),
        SDL_WINDOWPOS_CENTERED_DISPLAY(settings.display),
        int(displays[settings.display].w * 0.9f),
        int(displays[settings.display].h * 0.9f),
        SDL_WINDOW_RESIZABLE | flag | SDL_WINDOW_ALLOW_HIGHDPI
    );

    SDL_Rect rect;
    SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window), &rect);
    viewport = Viewport(glm::vec2(rect.w, rect.h));

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
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            onEvent(ev);
        }

        timer.start();

        onUpdate(dt);

        dt = float(timer.stop());
    }
}

} // namespace Raekor  