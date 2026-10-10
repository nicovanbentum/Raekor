#pragma once

namespace RK::GUI {

void SetFont(const String& inFilePath, float inScale = 1.0f);
void SetDarkTheme(float inScale = 1.0f);

float GetDisplayScale(SDL_Window* inWindow);

} // namespace RK::GUI
