#include "PCH.h"
#include "Input.h"

namespace RK {

Input* g_Input = new Input();

Input::Input() : m_KeyboardState(SDL_GetKeyboardState(NULL)) {}


void Input::OnEvent(const SDL_Event& inEvent)
{
    switch (inEvent.type) 
    {
        case SDL_CONTROLLERDEVICEADDED: 
        {
            const int joystick_index = inEvent.cdevice.which;
            m_Controller = SDL_GameControllerOpen(joystick_index);
        } break;

        case SDL_CONTROLLERDEVICEREMOVED: 
        {
            SDL_GameControllerClose(m_Controller);
            m_Controller = nullptr;
        } break;
    }
}


void Input::OnUpdate(float inDeltaTime)
{
    if (m_Controller == nullptr)
    {
        for (int i = 0; i < SDL_NumJoysticks(); i++)
        {
            if (SDL_IsGameController(i))
            {
                m_Controller = SDL_GameControllerOpen(i);
                break;
            }
        }
    }
}


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