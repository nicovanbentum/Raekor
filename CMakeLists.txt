cmake_minimum_required(VERSION 3.15)

# Point CMake to the VCPKG submodule and set static linkage for it
set(CMAKE_TOOLCHAIN_FILE "dependencies/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET x64-windows-static)

# The actual project
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
project(Raekor C CXX)

# Find all VCPKG packages and Vulkan
find_package(Vulkan REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles src/*.h)
file(GLOB_RECURSE CppFiles src/*.cpp)

# Add ImGuizmo and SPIRV Reflect to the cpp files list, disable PCH for .c files
set_source_files_properties(dependencies/ImGuizmo/ImGuizmo.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles dependencies/ImGuizmo/ImGuizmo.cpp)

set_source_files_properties(dependencies/SPIRV-Reflect/spirv_reflect.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles dependencies/SPIRV-Reflect/spirv_reflect.h dependencies/SPIRV-Reflect/spirv_reflect.c)

# Rule to build the actual exectuable and enable c++17
add_executable(${CMAKE_PROJECT_NAME} ${CppFiles} ${HppFiles} ${CMAKE_CURRENT_SOURCE_DIR}/Raekor.rc)
target_compile_features(${CMAKE_PROJECT_NAME} PUBLIC cxx_std_17)

# Enable MSVC parallel build process
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

# Build a list of all include directories and include them
file(GLOB_RECURSE IncludeDirs src)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${IncludeDirs})

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME})
target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE src/headers/pch.h)

# PhysX does not have a find_package setup therefore we have to find it manually
include(SelectLibraryConfigurations)

find_library(PhysXPvdSDK_LIBRARY_RELEASE NAMES PhysXPvdSDK_64 PhysXPvdSDK_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}" NO_DEFAULT_PATH REQUIRED)
find_library(PhysXPvdSDK_LIBRARY_DEBUG NAMES PhysXPvdSDK_64 PhysXPvdSDK_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug" NO_DEFAULT_PATH REQUIRED)
select_library_configurations(PhysXPvdSDK)

find_library(PhysXExtensions_LIBRARY_RELEASE NAMES PhysXExtensions_64 PhysXExtensions_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}" NO_DEFAULT_PATH REQUIRED)
find_library(PhysXExtensions_LIBRARY_DEBUG NAMES PhysXExtensions_64 PhysXExtensions_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug" NO_DEFAULT_PATH REQUIRED)
select_library_configurations(PhysXExtensions)

find_library(PhysXFoundation_LIBRARY_RELEASE NAMES PhysXFoundation_64 PhysXFoundation_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}" NO_DEFAULT_PATH REQUIRED)
find_library(PhysXFoundation_LIBRARY_DEBUG NAMES PhysXFoundation_64 PhysXFoundation_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug" NO_DEFAULT_PATH REQUIRED)
select_library_configurations(PhysXFoundation)

find_library(PhysXCommon_LIBRARY_RELEASE NAMES PhysXCommon_64 PhysXCommon_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}" NO_DEFAULT_PATH REQUIRED)
find_library(PhysXCommon_LIBRARY_DEBUG NAMES PhysXCommon_64 PhysXCommon_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug" NO_DEFAULT_PATH REQUIRED)
select_library_configurations(PhysXCommon)

find_library(PhysX_LIBRARY_RELEASE NAMES PhysX_64 PhysX_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}" NO_DEFAULT_PATH REQUIRED)
find_library(PhysX_LIBRARY_DEBUG NAMES PhysX_64 PhysX_static_64 PATH_SUFFIXES lib PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug" NO_DEFAULT_PATH REQUIRED)
select_library_configurations(PhysX)

# Link all the things
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE DbgHelp)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE d3d11.lib dxgi.lib d3dcompiler.lib)

# Link all the VCPKG packages
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE lz4::lz4)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE glad::glad)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE imgui::imgui)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE assimp::assimp)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2::SDL2-static)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${PhysXPvdSDK_LIBRARY} ${PhysXExtensions_LIBRARY} ${PhysXFoundation_LIBRARY} ${PhysXCommon_LIBRARY} ${PhysX_LIBRARY})

# include dependency directories 
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC 
    "src/headers"
    "dependencies/stb"
    "dependencies/ImGuizmo"
    "dependencies/glm/glm"
    "dependencies/cereal/include"
    "dependencies/entt/src"
    "dependencies/VulkanMemoryAllocator/src"
    "dependencies/IconFontCppHeaders"
    "dependencies/SPIRV-Reflect"
    ${Vulkan_INCLUDE_DIRS}
)




