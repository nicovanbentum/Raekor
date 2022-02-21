#pragma once

#include "application.h"

namespace Raekor {

class Input {
public:
    Input::Input() : keyboardState(SDL_GetKeyboardState(NULL)) {}

    /* Gets the current state of a keyboard key.
        @param code pass the desired key from SDL_SCANCODE_ */
    static bool sIsKeyPressed(SDL_Keycode code);

    /* Gets the current state of a mouse button.
       @param button 0 = LMB, 1 = MMB, 2 = RMB */
    static bool sIsButtonPressed(uint32_t button);

    static void sSetCallback(uint32_t type, const std::function<void()>& callback);

private:
    const Uint8* keyboardState;
    inline static std::unique_ptr<Input> global = std::make_unique<Input>();
};

}