#pragma once

#include "camera.h"
#include "renderpass.h"

namespace Raekor {

class ApplicationSettings {
public:
    int display;
    std::string name;
    std::string font;
    std::string defaultScene;
    std::array<std::array<float, 4>, ImGuiCol_COUNT> themeColors;

    ApplicationSettings(const std::filesystem::path path);
    ~ApplicationSettings();

private:
    template<class C>
    void serialize(C& archive);

    const std::filesystem::path path;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class WindowApplication {
public:
    friend class InputHandler;

    WindowApplication(RenderAPI api);
    virtual ~WindowApplication();

    virtual void update(double dt)  = 0;

    Viewport& getViewport() { return viewport; }
    SDL_Window* getWindow() { return window; }

protected:
    Viewport viewport;
    SDL_Window* window;
    ApplicationSettings settings;

public:
    bool running;
    bool shouldResize;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class C>
void ApplicationSettings::serialize(C& archive) {
    archive( CEREAL_NVP(name), CEREAL_NVP(display),
        CEREAL_NVP(font), CEREAL_NVP(defaultScene), CEREAL_NVP(themeColors)
    );
}

} // Namespace Raekor