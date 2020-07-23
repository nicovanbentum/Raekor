#pragma once

namespace Raekor {

class Input {
public:
    Input() :
        keyboardState(SDL_GetKeyboardState(NULL))
    {}

    static inline bool isKeyPressed(SDL_Keycode code) {
        return singleton->keyboardState[code];
    }

    static inline bool isButtonPressed(unsigned int button) {
        return singleton->mouseState & SDL_BUTTON(button);
    }

    static void setCallback(Uint32 type, const std::function<void()>& callback) {

    }

    void pollEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {

        }
    }

private:
    static std::unique_ptr<Input> singleton;

    Uint32 mouseState;
    const Uint8* keyboardState;

};

}