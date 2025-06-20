#pragma once

#define WIN32_LEAN_AND_MEAN
#define M_PI 3.14159265358979323846


#ifdef NDEBUG
    #define RK_DEBUG 0
    #define IF_DEBUG(code)
    #define IF_DEBUG_ELSE(debug_code, rel_code) rel_code
#else 
    #define RK_DEBUG 1
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


#ifndef DEPRECATE_ASSIMP
#define DEPRECATE_ASSIMP
#endif


#ifndef RAEKOR_SCRIPT

/////////////////
// Icons font awesome 5 library
#include "IconsFontAwesome5.h"

// DirectX-Headers helper library
#include "directx/d3dx12.h"

#include <dxgi.h>
#include <dxgi1_6.h>
#include <shellapi.h>
#include <d3dcompiler.h>

// AGILITY SDK Version 600
#include "d3d12.h"
#include "d3d12sdklayers.h"
#include "d3dcommon.h"
#include "dxgiformat.h"

// DIRECTX Compiler 6_6 Supported
#include "dxc/dxcapi.h"
#include "dxc/d3d12shader.h"

// DIRECTX Memory Allocator
#include "D3D12MemAlloc.h"

// DirectStorage Library
#include "dstorage.h"
#include "dstorageerr.h"

// AMD Fidelity-FX Super Resolution 2.1
#include "ffx_fsr2.h"
#include "dx12/ffx_fsr2_dx12.h"

// Nvidia DLSS 3.1
#include "nvsdk_ngx.h"
#include "nvsdk_ngx_defs.h"
#include "nvsdk_ngx_params.h"
#include "nvsdk_ngx_helpers.h"

// Intel XeSS
#include "xess.h"
#include "xess_debug.h"
#include "xess_d3d12.h"
#include "xess_d3d12_debug.h"

// PIX Runtime Events
#define USE_PIX
#include "pix3.h"

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

//////////////////////////////
// platform specific includes
#ifdef _WIN32
#include <Windows.h>
#include <commdlg.h>
#include <SDL3/SDL_system.h>
#include <dwmapi.h>
#include <DbgHelp.h>
#include <shellapi.h>
#include <Psapi.h>  
#include <ShObjIdl_core.h>
#elif __linux__
#include <GL/gl.h>
#include <gtk/gtk.h>
#endif


/////////////////////
// include stb image
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"
#include "stb_dxt.h"

//////////////////////////
// include asset importer
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

///////////////////////////
// lz4 compression library
#include "lz4.h"

///////////////////////////
// meshlet library
#include "meshoptimizer.h"

/////////////////
// ImGui library
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imconfig.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "imgui/backends/imgui_impl_sdlrenderer3.h"

#endif // RAEKOR_SCRIPT


/////////////////////
// c++ (17) includes
#include <set>
#include <map>
#include <regex>
#include <stack>
#include <array>
#include <cmath>
#include <queue>
#include <chrono>
#include <random>
#include <ranges>
#include <limits>
#include <numeric>
#include <variant>
#include <fstream>
#include <sstream>
#include <charconv>
#include <optional>
#include <iostream>
#include <semaphore>
#include <execution>
#include <algorithm>
#include <filesystem>
#include <type_traits>
#include <unordered_map>
#include <source_location>

namespace RK {

namespace fs = std::filesystem;

using Path = fs::path;
using File = std::fstream;

using String = std::string;
using WString = std::wstring;
using StringView = std::string_view;
using StringBuilder = std::stringstream;

using Mutex = std::mutex;

template<typename T>
using Slice = std::span<T>;
using ByteSlice = Slice<const uint8_t>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T1, typename T2>
using Pair = std::pair<T1, T2>;

template<typename Type>
using Atomic = std::atomic<Type>;

template<typename Type>
using Stack = std::stack<Type>;

template<typename Type>
using Queue = std::queue<Type>;

template<typename Type>
using Array = std::vector<Type>;

template<typename Type, size_t Count>
using StaticArray = std::array<Type, Count>;

template<typename Key>
using HashSet = std::set<Key>;

template<typename Key, typename Value>
using HashMap = std::unordered_map<Key, Value>;

template<typename T>
using Optional = std::optional<T>;

}

///////////////////////
// Math library
#include "glm.hpp"
#include "ext.hpp"
#include "gtx/quaternion.hpp"
#include "gtx/spline.hpp"
#include "gtx/intersect.hpp"
#include "gtc/epsilon.hpp"
#include "gtc/constants.hpp"
#include "vector_relational.hpp"
#include "ext/matrix_relational.hpp"
#include "gtx/string_cast.hpp"
#include "gtc/type_ptr.hpp"
#include "gtx/matrix_decompose.hpp"
#include "gtx/euler_angles.hpp"
#include "gtx/texture.hpp"

namespace RK 
{
    using Quat   = glm::quat;
    using Vec2   = glm::vec2;
    using Vec3   = glm::vec3;
    using Vec4   = glm::vec4;
    using UVec2  = glm::uvec2;
    using UVec3  = glm::uvec3;
    using UVec4  = glm::uvec4;
    using IVec2  = glm::ivec2;
    using IVec3  = glm::ivec3;
    using IVec4  = glm::ivec4;
    using Mat3x3 = glm::mat3x3;
    using Mat4x3 = glm::mat4x3;
    using Mat4x4 = glm::mat4x4;
}


////////////////////////////
// Simple DirectMedia Layer
// Part of the API headers, types are used all over the place
#include "SDL3/SDL.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_vulkan.h"
#undef main //stupid sdl_main


/////////////////////////////
// BinaryRelations containers
#include "BinaryRelations/BinaryRelations.h"


//////////////////////////
// GLTF import library
// Part of API headers as we re-use the JSON parser
#include "cgltf.h"

/////////////////////////
// Jolt physics library
// Part of the API headers as components.h stores a couple types
#ifndef JPH_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER
#endif

#include "Jolt/Jolt.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/StreamWrapper.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Renderer/DebugRenderer.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/ObjectStream/ObjectStreamBinaryIn.h"
#include "Jolt/ObjectStream/ObjectStreamBinaryOut.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/StateRecorderImpl.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Constraints/SixDOFConstraint.h"
#include "Jolt/Physics/Body/BodyActivationListener.h"
#include "Jolt/Physics/SoftBody/SoftBodyShape.h"
#include "Jolt/Physics/SoftBody/SoftBodyCreationSettings.h"
#include "Jolt/Physics/SoftBody/SoftBodyMotionProperties.h"