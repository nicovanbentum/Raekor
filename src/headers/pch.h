#pragma once

#define WIN32_LEAN_AND_MEAN

#define M_PI 3.14159265358979323846

#ifdef NDEBUG
    #define RAEKOR_DEBUG 0
#else 
    #define RAEKOR_DEBUG 1
#endif

extern bool PATHTRACE;

//////////////////////////////////////////////////////////////////////////////////////////////////
// header only Cereal library
#include "cereal/archives/json.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/complex.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/variant.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////
// ImGui library
#include "imconfig.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#ifdef _WIN32
#include "imgui_impl_dx11.h"
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////
// platform specific includes

#ifdef _WIN32
    #include <Windows.h>
    #include <d3d11.h>
    #include <commdlg.h>
    #include <SDL2/SDL_syswm.h>
    #include <wrl.h>
    #include <d3dcompiler.h>
    #include <ShObjIdl_core.h>
    #include <mbstring.h>
    #include <DbgHelp.h>
    #include <Psapi.h>

    template<typename T>
    using com_ptr = Microsoft::WRL::ComPtr<T>;
    // DirectXTK Framework Header only 
    //#include "DirectXTK/DDSTextureLoader.h"
    //#include "DirectXTK/WICTextureLoader.h"
    
#elif __linux__
    #include <GL/gl.h>
    #include <gtk/gtk.h>
    
#endif

// OpenGL includes
#include "glad/glad.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// Vulkan SDK
#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#endif
#define VK_ENABLE_BETA_EXTENSIONS
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
#include "spirv_reflect.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// Simple DirectMedia Layer
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"
#undef main //stupid sdl_main

//////////////////////////////////////////////////////////////////////////////////////////////////
// ImGuizmo library
#include "ImGuizmo.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// openGL math library
#include "glm.hpp"
#include "ext.hpp"
#include "gtx/quaternion.hpp"
#include "gtx/string_cast.hpp"
#include "gtc/type_ptr.hpp"
#include "gtx/matrix_decompose.hpp"
#include "gtx/euler_angles.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////
// PhysX 
#define PX_PHYSX_STATIC_LIB
#include "PxPhysicsAPI.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// c++ (17) includes
#include <set>
#include <map>
#include <stack>
#include <array>
#include <queue>
#include <future>
#include <chrono>
#include <bitset>
#include <numeric>
#include <random>
#include <limits>
#include <variant>
#include <fstream>
#include <sstream>
#include <optional>
#include <iostream>
#include <execution>
#include <algorithm>
#include <type_traits>
#include <unordered_map>

#include <filesystem>
namespace fs = std::filesystem;

//////////////////////////////////////////////////////////////////////////////////////////////////
// include stb image
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize.h"
#include "stb_dxt.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// include asset importer
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"
#include "assimp/pbrmaterial.h"
#include "assimp/Exporter.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////
// Entity-component system entt
#include "entt/entt.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////
// lz4 compression
#include "lz4.h"
