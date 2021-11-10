#pragma once

#include "async.h"
#include "camera.h"

namespace Raekor {

struct ConfigSettings {
    int display = 0;
    bool vsync = true;
    std::string name = "Raekor";
    std::string font = "resources/Roboto-Regular.ttf";
    std::string defaultScene = "scenes/sponza.scene";
    std::array<std::array<float, 4>, ImGuiCol_COUNT> themeColors;

    template<typename Archive> 
    void serialize(Archive& archive) {
        archive( CEREAL_NVP(name), CEREAL_NVP(display), CEREAL_NVP(vsync),
            CEREAL_NVP(font), CEREAL_NVP(defaultScene), CEREAL_NVP(themeColors)
        );
    }
};



enum RendererFlags {
    OPENGL = SDL_WINDOW_OPENGL,
    VULKAN = SDL_WINDOW_VULKAN,
};



class Application {
public:
    friend class InputHandler;

    Application(RendererFlags flag);
    virtual ~Application();

    void run();

    virtual void onUpdate(float dt)  = 0;
    virtual void onEvent(const SDL_Event& event) = 0;

    Viewport& getViewport() { return viewport; }
    SDL_Window* getWindow() { return window; }

public:
    bool running = true;

protected:
    Viewport viewport;
    SDL_Window* window = nullptr;
    ConfigSettings settings;
};

} // Namespace Raekor