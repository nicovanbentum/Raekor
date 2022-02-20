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
    NONE = 0,
    OPENGL = SDL_WINDOW_OPENGL,
    VULKAN = SDL_WINDOW_VULKAN,
};



class Application {
public:
    friend class InputHandler;

    Application(RendererFlags flag);
    virtual ~Application();

    void run();
    void Terminate() { m_Running = false; }

    virtual void onUpdate(float dt)  = 0;
    virtual void onEvent(const SDL_Event& event) = 0;

    SDL_Window* getWindow() { return window; }
    Viewport& getViewport() { return m_Viewport; }

public:
    bool m_Running = true;

protected:
    Viewport m_Viewport;
    SDL_Window* window = nullptr;
    ConfigSettings settings;
};

} // Namespace Raekor