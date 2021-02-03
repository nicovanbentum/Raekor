#pragma once

#include "application.h"

namespace Raekor {

class Input {
public:
    Input();

    static bool isKeyPressed(SDL_Keycode code);
    static bool isButtonPressed(unsigned int button);
    static void setCallback(Uint32 type, const std::function<void()>& callback);

    void pollEvents();

    inline static std::unique_ptr<Input> singleton = std::make_unique<Input>();
private:

    Uint32 mouseState;
    const Uint8* keyboardState;

};

}