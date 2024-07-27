#include "pch.h"
#include "input.h"

namespace RK {

Input* g_Input = new Input();

Input::Input() : keyboardState(SDL_GetKeyboardState(NULL)) {}


bool Input::IsKeyPressed(SDL_Keycode code)
{
	return keyboardState[code];
}


bool Input::IsButtonPressed(uint32_t button)
{
	uint32_t state = SDL_GetMouseState(NULL, NULL);
	return state & SDL_BUTTON(button);
}


bool Input::IsRelativeMouseMode()
{
	return relMouseMode;
}


void Input::SetRelativeMouseMode(bool inEnabled)
{
	if (SDL_SetRelativeMouseMode(SDL_bool(inEnabled)) != 0) 
		printf("SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError());

	relMouseMode = inEnabled;
}


void Input::SetCallback(uint32_t type, const std::function<void()>& callback) {}


} // raekor