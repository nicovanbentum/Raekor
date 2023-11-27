#pragma once

#define WIN32_LEAN_AND_MEAN
#define M_PI 3.14159265358979323846


#ifdef NDEBUG
    #define RK_DEBUG_BUILD 0
    #define IF_DEBUG(code)
    #define IF_DEBUG_ELSE(debug_code, rel_code) rel_code
#else 
    #define RK_DEBUG_BUILD 1
    #define IF_DEBUG(code) code
    #define IF_DEBUG_ELSE(debug_code, rel_code) debug_code
#endif




#ifdef RAEKOR_SCRIPT
#ifndef SCRIPT_INTERFACE
    #define SCRIPT_INTERFACE __declspec(dllimport)
#endif
#else
#ifndef SCRIPT_INTERFACE
    #define SCRIPT_INTERFACE __declspec(dllexport)
#endif
#endif


/////////////////
// ImGui library
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imconfig.h"
#include "imgui.h"
#include "imnodes.h"
#include "imnodes_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer.h"
#include "ImGuizmo.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"
#include "IconsFontAwesome5.h"


//////////////////////////////
// platform specific includes
#ifdef _WIN32
    #include <Windows.h>
    #include <commdlg.h>
    #include <SDL2/SDL_syswm.h>
    #include <dwmapi.h>
    #include <DbgHelp.h>
    #include <shellapi.h>
    #include <Psapi.h>  
    #include <ShObjIdl_core.h>
#elif __linux__
    #include <GL/gl.h>
    #include <gtk/gtk.h>
#endif


////////////////////////////
// Simple DirectMedia Layer
#include "SDL2/SDL.h"
#include "SDL2/SDL_render.h"
#include "SDL2/SDL_vulkan.h"
#undef main //stupid sdl_main


///////////////////////
// openGL math library
#include "glm.hpp"
#include "ext.hpp"
#include "gtx/quaternion.hpp"
#include "gtc/epsilon.hpp"
#include "gtc/constants.hpp"
#include "vector_relational.hpp"
#include "ext/matrix_relational.hpp"
#include "gtx/string_cast.hpp"
#include "gtc/type_ptr.hpp"
#include "gtx/matrix_decompose.hpp"
#include "gtx/euler_angles.hpp"

namespace Raekor {
using Quat   = glm::quat;
using Vec2   = glm::vec2;
using Vec3   = glm::vec3;
using Vec4   = glm::vec4;
using UVec2  = glm::uvec2;
using UVec3  = glm::uvec3;
using IVec3  = glm::ivec3;
using IVec4  = glm::ivec4;
using Mat3x3 = glm::mat3x3;
using Mat4x3 = glm::mat4x3;
using Mat4x4 = glm::mat4x4;
}


/////////////////////////
// Jolt physics library
#ifndef JPH_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER
#endif

#include "Jolt/Jolt.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Renderer/DebugRenderer.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/StateRecorderImpl.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/BodyActivationListener.h"
#include "Jolt/Physics/SoftBody/SoftBodyShape.h"
#include "Jolt/Physics/SoftBody/SoftBodyCreationSettings.h"
#include "Jolt/Physics/SoftBody/SoftBodyMotionProperties.h"


/////////////////////
// c++ (17) includes
#include <set>
#include <map>
#include <stack>
#include <array>
#include <cmath>
#include <queue>
#include <chrono>
#include <random>
#include <limits>
#include <numeric>
#include <variant>
#include <fstream>
#include <sstream>
#include <charconv>
#include <optional>
#include <iostream>
#include <execution>
#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;
namespace Raekor {
using Path = fs::path;
using File = std::fstream;
}

/////////////////////
// include stb image
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"
#include "stb_dxt.h"

//////////////////////////
// include asset importer
#ifndef DEPRECATE_ASSIMP
#define DEPRECATE_ASSIMP
#endif

#ifndef DEPRECATE_ASSIMP

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"
#include "assimp/GltfMaterial.h"
#include "assimp/Exporter.hpp"

#endif // DEPRECATE_ASSIMP

//////////////////////////
// FBX import library
#include "ufbx.h"

//////////////////////////
// GLTF import library
#include "cgltf.h"

///////////////////////////
// lz4 compression library
#include "lz4.h"
