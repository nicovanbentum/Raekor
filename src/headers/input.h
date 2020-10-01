#pragma once

#include "application.h"

namespace Raekor {

class InputHandler {
public:
    static bool handleEvents(WindowApplication* app, bool mouseInViewport, double dt);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Input {
public:
    Input();

    static bool isKeyPressed(SDL_Keycode code);
    static bool isButtonPressed(unsigned int button);
    static void setCallback(Uint32 type, const std::function<void()>& callback);

    void pollEvents();

private:
    static std::unique_ptr<Input> singleton;

    Uint32 mouseState;
    const Uint8* keyboardState;

};

}