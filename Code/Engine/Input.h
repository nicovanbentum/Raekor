#pragma once

namespace RK {

enum class Key : int
{
    UNKNOWN = 0,
    A = 4,
    B = 5,
    C = 6,
    D = 7,
    E = 8,
    F = 9,
    G = 10,
    H = 11,
    I = 12,
    J = 13,
    K = 14,
    L = 15,
    M = 16,
    N = 17,
    O = 18,
    P = 19,
    Q = 20,
    R = 21,
    S = 22,
    T = 23,
    U = 24,
    V = 25,
    W = 26,
    X = 27,
    Y = 28,
    Z = 29,
    DIGIT_1 = 30,
    DIGIT_2 = 31,
    DIGIT_3 = 32,
    DIGIT_4 = 33,
    DIGIT_5 = 34,
    DIGIT_6 = 35,
    DIGIT_7 = 36,
    DIGIT_8 = 37,
    DIGIT_9 = 38,
    DIGIT_0 = 39,
    RETURN = 40,
    ESCAPE = 41,
    BACKSPACE = 42,
    TAB = 43,
    SPACE = 44,
    MINUS = 45,
    EQUALS = 46,
    LEFTBRACKET = 47,
    RIGHTBRACKET = 48,
    BACKSLASH = 49, 
    NONUSHASH = 50,
    SEMICOLON = 51,
    APOSTROPHE = 52,
    GRAVE = 53,
    COMMA = 54,
    PERIOD = 55,
    SLASH = 56,
    CAPSLOCK = 57,
    F1 = 58,
    F2 = 59,
    F3 = 60,
    F4 = 61,
    F5 = 62,
    F6 = 63,
    F7 = 64,
    F8 = 65,
    F9 = 66,
    F10 = 67,
    F11 = 68,
    F12 = 69,
    PRINTSCREEN = 70,
    SCROLLLOCK = 71,
    PAUSE = 72,
    INSERT = 73, 
    HOME = 74,
    PAGEUP = 75,
    DEL = 76,
    END = 77,
    PAGEDOWN = 78,
    RIGHT = 79,
    LEFT = 80,
    DOWN = 81,
    UP = 82,
    LCTRL = 224,
    LSHIFT = 225,
    LALT = 226,
    LGUI = 227,
    RCTRL = 228,
    RSHIFT = 229,
    RALT = 230,
    RGUI = 231,
    MODE = 257
};

class Input
{
public:
	Input();

	/* Gets the current state of a keyboard key */
	bool IsKeyDown(Key key);

	/* Gets the current state of a mouse button.
	   @param button 1 = LMB, 2 = MMB, 3 = RMB */
	bool IsButtonDown(uint32_t button);

	bool IsRelativeMouseMode();
	void SetRelativeMouseMode(bool inEnabled);

private:
	bool m_RelMouseMode = false;
	const uint8_t* m_KeyboardState;
};

extern Input* g_Input;

}