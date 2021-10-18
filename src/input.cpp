#include "pch.h"
#include "input.h"

namespace Raekor {

Input::Input() :
    keyboardState(SDL_GetKeyboardState(NULL))
{}



bool Input::isKeyPressed(SDL_Keycode code) {
    return singleton->keyboardState[code];
}



bool Input::isButtonPressed(uint32_t button) {
    auto state = SDL_GetMouseState(NULL, NULL);
    return state & SDL_BUTTON(button);
}



void Input::setCallback(uint32_t type, const std::function<void()>& callback) {

}

} // raekor