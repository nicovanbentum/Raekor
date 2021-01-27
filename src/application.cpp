#include "pch.h"
#include "application.h"

#include "util.h"
#include "camera.h"

namespace Raekor {

WindowApplication::WindowApplication(RendererFlags flag) : 
    window(nullptr), running(true), shouldResize(false), settings("config.json") 
{
    int sdlError = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdlError == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | flag |
        SDL_WINDOW_ALLOW_HIGHDPI;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    settings.display = settings.display > displays.size() - 1 ? 0 : settings.display;
    window = SDL_CreateWindow(settings.name.c_str(),
        SDL_WINDOWPOS_CENTERED_DISPLAY(settings.display),
        SDL_WINDOWPOS_CENTERED_DISPLAY(settings.display),
        static_cast<int>(displays[settings.display].w * 0.9f),
        static_cast<int>(displays[settings.display].h * 0.9f),
        wflags);

    SDL_Rect rect;
    SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(window), &rect);
    viewport = Viewport(glm::vec2(rect.w, rect.h));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

WindowApplication::~WindowApplication() {
    //clean up SDL
    settings.display = SDL_GetWindowDisplayIndex(window);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

ApplicationSettings::ApplicationSettings(const std::filesystem::path path) : path(path) {
    std::ifstream is(path);
    cereal::JSONInputArchive archive(is);
    serialize(archive);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

ApplicationSettings::~ApplicationSettings() {
    std::ofstream os(path);
    cereal::JSONOutputArchive archive(os);
    serialize(archive);
}

} // namespace Raekor  