#pragma once

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#define _USE_MATH_DEFINES
#include <cmath>

#define NOMINMAX // i hate computers

#ifdef NDEBUG
    #define RAEKOR_DEBUG 0
#else 
    #define RAEKOR_DEBUG 1
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
#include "spirv_cross/spirv_cross.hpp"
#include "spirv_cross/spirv_glsl.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////
// Simple DirectMedia Layer
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"
#undef main //stupid sdl_main

//////////////////////////////////////////////////////////////////////////////////////////////////
// ImGui library
#include "imconfig.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui_internal.h"

#ifdef _WIN32
    #include "imgui_impl_dx11.h"
#endif

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
#include "PxPhysics.h"
#include "PxMaterial.h"
#include "PxRigidDynamic.h"
#include "cooking/PxCooking.h"
#include "extensions/PxDefaultAllocator.h"
#include "extensions/PxDefaultErrorCallback.h"
#include "extensions/PxDefaultCpuDispatcher.h"
#include "extensions/PxExtensionsAPI.h"
#include "PxPhysicsVersion.h"
#include "PxScene.h"
#include "gpu/PxGpu.h"

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
#include <fstream>
#include <sstream>
#include <optional>
#include <iostream>
#include <execution>
#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <filesystem>

//////////////////////////////////////////////////////////////////////////////////////////////////
// header only Cereal library
#include "cereal/archives/json.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/complex.hpp"
#include "cereal/types/vector.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////
// include stb image
#include "stb_image.h"
#include "stb_image_write.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// include asset importer
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"
#include "assimp/pbrmaterial.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// Entity-component system entt
#include "entt/entt.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////
// lz4 compression
#include "lz4.h"