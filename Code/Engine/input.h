#pragma once

namespace RK {

typedef int32_t SDL_Keycode;

class Input
{
public:
	Input();

	/* Gets the current state of a keyboard key.
		@param code pass the desired key from SDL_SCANCODE_ */
	bool IsKeyPressed(SDL_Keycode code);

	/* Gets the current state of a mouse button.
	   @param button 1 = LMB, 2 = MMB, 3 = RMB */
	bool IsButtonPressed(uint32_t button);

	void SetCallback(uint32_t type, const std::function<void()>& callback);

	bool IsRelativeMouseMode();
	void SetRelativeMouseMode(bool inEnabled);

private:
	bool relMouseMode = false;
	const uint8_t* keyboardState;
};

extern Input* g_Input;

}