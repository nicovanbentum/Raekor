#pragma once

#include "application.h"

namespace Raekor {

class Input {
public:
    Input();
    /* Gets the current state of a keyboard key.
        @param code pass the desired key from SDL_SCANCODE_ */
    static bool isKeyPressed(SDL_Keycode code);

    /* Gets the current state of a mouse button.
       @param button 0 = LMB, 1 = MMB, 2 = RMB */
    static bool isButtonPressed(uint32_t button);

    static void setCallback(uint32_t type, const std::function<void()>& callback);

private:
    const Uint8* keyboardState;
    inline static std::unique_ptr<Input> singleton = std::make_unique<Input>();
};

}