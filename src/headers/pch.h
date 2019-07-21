#pragma once

// platform specific includes
#include "GL/gl3w.h"

#ifdef _WIN32
	#include <Windows.h>
	#include <d3d11.h>
	#include <commdlg.h>
    #include <GL/GL.h>
	#include <SDL_syswm.h>
#elif __linux__
    #include <GL/gl.h>
    #include <gtk/gtk.h>
    
#endif


// imgui headers to build once
#include "imconfig.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imstb_rectpack.h"
#include "imstb_textedit.h"
#include "imstb_truetype.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"

// openGL math library
#include "glm.hpp"
#include "ext.hpp"
#include "gtx/quaternion.hpp"

// SDL includes
#include "SDL.h"
#undef main //stupid sdl_main

// c++ includes
#include <map>
#include <array>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>


// header only Cereal library
#include "cereal/archives/json.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/complex.hpp"
#include "cereal/types/vector.hpp"

// include stb
#include "stb_image.h"

