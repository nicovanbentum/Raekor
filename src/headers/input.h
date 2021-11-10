#pragma once

#include "application.h"

namespace Raekor {

class Input {
public:
    Input();

    static bool isKeyPressed(SDL_Keycode code);
    static bool isButtonPressed(uint32_t button);
    static void setCallback(uint32_t type, const std::function<void()>& callback);

private:
    const Uint8* keyboardState;
    inline static std::unique_ptr<Input> singleton = std::make_unique<Input>();
};

}