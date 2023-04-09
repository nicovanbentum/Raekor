#pragma once

#include "camera.h"

namespace Raekor {

struct ConfigSettings {
    int display = 0;
    bool vsync = true;
    std::string name = "";
    std::string font = "";
    std::string defaultScene = "";
    std::array<std::array<float, 4>, ImGuiCol_COUNT> themeColors;

    template<typename Archive> 
    void serialize(Archive& archive) {
        archive( CEREAL_NVP(name), CEREAL_NVP(display), CEREAL_NVP(vsync),
            CEREAL_NVP(font), CEREAL_NVP(defaultScene), CEREAL_NVP(themeColors)
        );
    }
};


enum WindowFlag {
    NONE = 0,
    HIDDEN = SDL_WINDOW_HIDDEN,
    RESIZE = SDL_WINDOW_RESIZABLE,
    OPENGL = SDL_WINDOW_OPENGL,
    VULKAN = SDL_WINDOW_VULKAN,
    BORDERLESS = SDL_WINDOW_BORDERLESS,
};

using WindowFlags = uint32_t;


class Application {
public:
    friend class InputHandler;

    Application(WindowFlags inFlags);
    virtual ~Application();

    void Run();
    void Terminate() { m_Running = false; }

    virtual void OnUpdate(float dt)  = 0;
    virtual void OnEvent(const SDL_Event& event) = 0;

    ConfigSettings& GetSettings() { return m_Settings; }
    const ConfigSettings& GetSettings() const { return m_Settings; }

    SDL_Window* GetWindow() { return m_Window; }
    Viewport& GetViewport() { return m_Viewport; }

public:
    bool m_Running = true;

protected:
    Viewport m_Viewport;
    ConfigSettings m_Settings;
    SDL_Window* m_Window = nullptr;
};


} // Namespace Raekor