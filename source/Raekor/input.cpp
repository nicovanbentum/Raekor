#include "pch.h"
#include "input.h"

namespace Raekor {

Input* g_Input = new Input();

Input::Input() : keyboardState(SDL_GetKeyboardState(NULL)) {}


bool Input::IsKeyPressed(SDL_Keycode code)
{
	return keyboardState[code];
}


bool Input::IsButtonPressed(uint32_t button)
{
	auto state = SDL_GetMouseState(NULL, NULL);
	return state & SDL_BUTTON(button);
}


bool Input::IsRelativeMouseMode()
{
	return SDL_GetRelativeMouseMode();
}


void Input::SetRelativeMouseMode(bool inEnabled)
{
	if (SDL_SetRelativeMouseMode(SDL_bool(inEnabled)) != 0) {
		// Handle error
		printf("SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError());
	}
}


void Input::SetCallback(uint32_t type, const std::function<void()>& callback) {}


} // raekor