#pragma once

#include "camera.h"

namespace Raekor {

struct ConfigSettings {
    RTTI_CLASS_HEADER(ConfigSettings);

    int mDisplayIndex = 0;
    bool mVsyncEnabled = true;
    std::string mAppName = "";
    std::string mFontFile = "";
    std::string mSceneFile = "";
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

protected:
    bool m_Running = true;
    SDL_Window* m_Window = nullptr;

    Viewport m_Viewport;
    ConfigSettings m_Settings;
};


} // Namespace Raekor