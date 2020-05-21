#pragma once

#include "util.h"
#include "camera.h"

namespace Raekor {

void handleEvents(SDL_Window* window, Raekor::Camera& camera, bool mouseInViewport, double dt);

} // namesapce Raekor
