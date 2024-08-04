#include "PCH.h"
#include "Input.h"

namespace RK {

Input* g_Input = new Input();

Input::Input() : m_KeyboardState(SDL_GetKeyboardState(NULL)) {}


bool Input::IsKeyDown(Key key)
{
	return m_KeyboardState[static_cast<int>(key)];
}


bool Input::IsButtonDown(uint32_t button)
{
	uint32_t state = SDL_GetMouseState(NULL, NULL);
	return state & SDL_BUTTON(button);
}


bool Input::IsRelativeMouseMode()
{
	return m_RelMouseMode;
}


void Input::SetRelativeMouseMode(bool inEnabled)
{
	if (SDL_SetRelativeMouseMode(SDL_bool(inEnabled)) != 0) 
		printf("SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError());

	m_RelMouseMode = inEnabled;
}

} // raekor