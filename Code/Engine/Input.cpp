#include "PCH.h"
#include "Input.h"

namespace RK {

Input* g_Input = new Input();

Input::Input() : m_KeyboardState(SDL_GetKeyboardState(NULL)) {}


void Input::OnEvent(const SDL_Event& inEvent)
{
    switch (inEvent.type) 
    {
        case SDL_EVENT_GAMEPAD_ADDED: 
        {
            const int joystick_index = inEvent.cdevice.which;
            m_Controller = SDL_OpenGamepad(joystick_index);
        } break;

        case SDL_EVENT_GAMEPAD_REMOVED: 
        {
            SDL_CloseGamepad(m_Controller);
            m_Controller = nullptr;
        } break;
    }
}


void Input::OnUpdate(float inDeltaTime)
{
    if (m_Controller == nullptr)
    {
        int joystick_count = 0;
        SDL_JoystickID* joysticks = SDL_GetJoysticks(&joystick_count);

        for (int i = 0; i < joystick_count; i++)
        {
            SDL_JoystickID joystick = joysticks[i];

            if (SDL_IsGamepad(joystick))
            {
                m_Controller = SDL_OpenGamepad(joystick);
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
	return state & SDL_BUTTON_MASK(button);
}


bool Input::IsRelativeMouseMode()
{
	return m_RelMouseMode;
}


void Input::SetRelativeMouseMode(bool inEnabled)
{
	m_RelMouseMode = inEnabled;
}

} // raekor