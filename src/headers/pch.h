#pragma once

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#define _USE_MATH_DEFINES
#include <cmath>

// platform specific includes
#include "GL/gl3w.h"

#ifdef _WIN32
    #include <Windows.h>
    #include <d3d11.h>
    #include <commdlg.h>
    #include <GL/GL.h>
    #include <SDL_syswm.h>
    #include <wrl.h>
    #include <d3dcompiler.h>
    #include <ShObjIdl_core.h>

    // include DirectXTK
    #include "DirectXTK/DDSTextureLoader.h"
    #include "DirectXTK/WICTextureLoader.h"
    
#elif __linux__
    #include <GL/gl.h>
    #include <gtk/gtk.h>
    
#endif // _WIN32

// Vulkan includes
#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#endif
#include "vulkan/vulkan.h"

// SDL includes
#include "SDL.h"
#include "SDL_vulkan.h"
#undef main //stupid sdl_main

// imgui headers to build once
#include "imconfig.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"
#include "misc/cpp/imgui_stdlib.h"

// imguizmo header
#include "ImGuizmo.h"

// if we're on windows we also include the directx11 implementation for ImGui
#ifdef _WIN32
    #include "imgui_impl_dx11.h"
#endif

// openGL math library
#include "glm.hpp"
#include "ext.hpp"
#include "gtx/quaternion.hpp"
#include "gtx/string_cast.hpp"
#include "gtc/type_ptr.hpp"
#include "gtx/matrix_decompose.hpp"

// Bullet3 Physics library
#include "bullet/btBulletDynamicsCommon.h"

// ChaiScript
#include "chaiscript/chaiscript.hpp"
#include "chaiscript/chaiscript_stdlib.hpp"

// c++ (17) includes
#include <set>
#include <map>
#include <array>
#include <future>
#include <chrono>
#include <numeric>
#include <random>
#include <fstream>
#include <sstream>
#include <optional>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <filesystem>

// header only Cereal library
#include "cereal/archives/json.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/complex.hpp"
#include "cereal/types/vector.hpp"

// include stb image
#include "stb_image.h"

// include asset importer
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"
#include "assimp/pbrmaterial.h"