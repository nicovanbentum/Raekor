#include "pch.h"
#include "input.h"

namespace Raekor {

bool Input::sIsKeyPressed(SDL_Keycode code)
{
	return global->keyboardState[code];
}


bool Input::sIsButtonPressed(uint32_t button)
{
	auto state = SDL_GetMouseState(NULL, NULL);
	return state & SDL_BUTTON(button);
}


void Input::sSetCallback(uint32_t type, const std::function<void()>& callback) {}

} // raekor