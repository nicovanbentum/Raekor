#pragma once

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
    #include "CommonStates.h"
    #include "DDSTextureLoader.h"
    //#include "DirectXHelpers.h"
    #include "Effects.h"
    #include "GamePad.h"
    #include "GeometricPrimitive.h"
    #include "GraphicsMemory.h"
    #include "Keyboard.h"
    //#include "Model.h"
    #include "Mouse.h"
    #include "PostProcess.h"
    #include "PrimitiveBatch.h"
    #include "ScreenGrab.h"
    #include "SimpleMath.h"
    #include "SpriteBatch.h"
    #include "SpriteFont.h"
    #include "VertexTypes.h"
    #include "WICTextureLoader.h"
    
#elif __linux__
    #include <GL/gl.h>
    #include <gtk/gtk.h>
    
    
#endif

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
#include "imgui_internal.h"
#include "imstb_rectpack.h"
#include "imstb_textedit.h"
#include "imstb_truetype.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"

// if we're on windows we also include 
// the directx11 implementation for ImGui
#ifdef _WIN32
    #include "imgui_impl_dx11.h"
#endif

// openGL math library
#include "glm.hpp"
#include "ext.hpp"
#include "gtx/quaternion.hpp"
#include "gtx/string_cast.hpp"
#include "gtc/type_ptr.hpp"


// c++ includes
#include <set>
#include <map>
#include <array>
#include <future>
#include <chrono>
#include <fstream>
#include <sstream>
#include <optional>
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

