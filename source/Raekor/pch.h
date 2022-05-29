#pragma once

#define WIN32_LEAN_AND_MEAN
#define M_PI 3.14159265358979323846


#ifdef NDEBUG
    #define RAEKOR_DEBUG 0
#else 
    #define RAEKOR_DEBUG 1
#endif


#ifdef RAEKOR_SCRIPT
    #define SCRIPT_INTERFACE __declspec(dllimport)
#else
    #define SCRIPT_INTERFACE __declspec(dllexport)
#endif


//////////////////////////////
// header only Cereal library
#include "cereal/cereal.hpp"
#include "cereal/macros.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/complex.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/utility.hpp"
#include "cereal/types/variant.hpp"
#include "cereal/types/polymorphic.hpp"


/////////////////
// ImGui library
#include "imconfig.h"
#include "imgui.h"
#include "imnodes.h"
#include "imnodes_internal.h"
#include "imgui_impl_sdl.h"
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
    #include <DbgHelp.h>
    #include <Psapi.h>  
#elif __linux__
    #include <GL/gl.h>
    #include <gtk/gtk.h>
#endif


////////////////////////////
// Simple DirectMedia Layer
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"
#undef main //stupid sdl_main


///////////////////////
// openGL math library
#include "glm.hpp"
#include "ext.hpp"
#include "gtx/quaternion.hpp"
#include "gtx/string_cast.hpp"
#include "gtc/type_ptr.hpp"
#include "gtx/matrix_decompose.hpp"
#include "gtx/euler_angles.hpp"

/////////////////////////
// Jolt physics library
#include "Jolt.h"
#include "Physics/PhysicsSystem.h"
#include "RegisterTypes.h"
#include "Core/TempAllocator.h"
#include "Core/JobSystemThreadPool.h"
#include "Physics/PhysicsSettings.h"
#include "Physics/PhysicsSystem.h"
#include "Physics/StateRecorderImpl.h"
#include "Physics/Collision/Shape/BoxShape.h"
#include "Physics/Collision/Shape/SphereShape.h"
#include "Physics/Body/BodyCreationSettings.h"
#include "Physics/Body/BodyActivationListener.h"

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
#include <optional>
#include <iostream>
#include <execution>
#include <algorithm>
#include <type_traits>
#include <unordered_map>

#include <filesystem>
namespace fs = std::filesystem;

/////////////////////
// include stb image
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize.h"
#include "stb_dxt.h"

//////////////////////////
// include asset importer
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"
#include "assimp/pbrmaterial.h"
#include "assimp/Exporter.hpp"

//////////////////////////
#include "cgltf.h"


////////////////////////////////////////
// EnTT entity-component system library
#include "entt/entity/entity.hpp"
#include "entt/entity/helper.hpp"
#include "entt/entity/registry.hpp"
#include "entt/entity/snapshot.hpp"
#include "entt/entity/view.hpp"


///////////////////////////
// lz4 compression library
#include "lz4.h"
