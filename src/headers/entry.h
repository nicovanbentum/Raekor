#pragma once

#include "pch.h"
#include "util.h"
#include "camera.h"

namespace Raekor {

void handle_sdl_gui_events(std::vector<SDL_Window*> windows, Raekor::Camera& camera, double dt);

} // namesapce Raekor
