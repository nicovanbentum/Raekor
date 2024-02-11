#pragma once

namespace Raekor {

typedef int32_t SDL_Keycode;

class Input
{
public:
	Input();

	/* Gets the current state of a keyboard key.
		@param code pass the desired key from SDL_SCANCODE_ */
	static bool sIsKeyPressed(SDL_Keycode code);

	/* Gets the current state of a mouse button.
	   @param button 0 = LMB, 1 = MMB, 2 = RMB */
	static bool sIsButtonPressed(uint32_t button);

	static void sSetCallback(uint32_t type, const std::function<void()>& callback);

private:
	const uint8_t* keyboardState;
};

extern Input* g_Input;

}