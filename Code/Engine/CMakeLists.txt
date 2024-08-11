cmake_minimum_required(VERSION 3.15)

project(Engine C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Add ImGuizmo and SPIRV Reflect to the cpp files list, disable PCH for .c files
set_source_files_properties(${CMAKE_SOURCE_DIR}/ThirdParty/ImGuizmo/ImGuizmo.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/ImGuizmo/ImGuizmo.cpp)

set_source_files_properties(${CMAKE_SOURCE_DIR}/ThirdParty/imnodes/imnodes.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/imnodes/imnodes.cpp)

set_source_files_properties(${CMAKE_SOURCE_DIR}/ThirdParty/ufbx/ufbx.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/ufbx/ufbx.c)

set_source_files_properties(${CMAKE_SOURCE_DIR}/ThirdParty/meshoptimizer/src/clusterizer.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/meshoptimizer/src/clusterizer.cpp)

# Add ImGui
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/ThirdParty/imgui)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui_draw.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui_demo.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui_tables.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui_widgets.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp)
set_source_files_properties(ImGuiFiles PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# Append library cpp's
list(APPEND CppFiles ${ImGuiFiles})

# Rule to build the actual exectuable and enable c++20
add_library(${PROJECT_NAME} ${CppFiles} ${HppFiles} ../Core.natvis)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# Natvis files
target_sources(${PROJECT_NAME} PRIVATE ../Core.natvis)
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/glm/util/glm.natvis)
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/ufbx/misc/ufbx.natvis)
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/JoltPhysics/Jolt/Jolt.natvis)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

find_library(DLSS NAMES nvsdk_ngx_s PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/DLSS/lib/Windows_x86_64/x86_64 NO_DEFAULT_PATH REQUIRED)
find_library(DLSSd NAMES nvsdk_ngx_s_dbg PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/DLSS/lib/Windows_x86_64/x86_64 NO_DEFAULT_PATH REQUIRED)

find_library(FSR2 NAMES ffx_fsr2_api_x64 PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/lib NO_DEFAULT_PATH REQUIRED)
find_library(FSR2d NAMES ffx_fsr2_api_x64d PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/lib NO_DEFAULT_PATH REQUIRED)

find_library(FSR2DX12 NAMES ffx_fsr2_api_dx12_x64 PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/lib NO_DEFAULT_PATH REQUIRED)
find_library(FSR2DX12d NAMES ffx_fsr2_api_dx12_x64d PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/lib NO_DEFAULT_PATH REQUIRED)

find_library(XESS NAMES libxess PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/xess/lib NO_DEFAULT_PATH REQUIRED)

find_library(PIX NAMES WinPixEventRuntime PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/PIX/bin/x64 NO_DEFAULT_PATH REQUIRED)

find_library(DXC_LIBRARY NAMES dxcompiler lib PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/dxc NO_DEFAULT_PATH REQUIRED)
find_library(DIRECT_STORAGE_LIBRARY NAMES dstorage lib PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage NO_DEFAULT_PATH REQUIRED)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Renderer/Shared.h" "${CMAKE_SOURCE_DIR}/Assets/Shaders/Backend/Include/Shared.hlsli")

# Add Jolt Physics library using their CMake file
set(PHYSICS_REPO_ROOT ${CMAKE_SOURCE_DIR}/ThirdParty/JoltPhysics)
include(${PHYSICS_REPO_ROOT}/Jolt/Jolt.cmake)
target_compile_features(Jolt PUBLIC cxx_std_17)
set_property(TARGET Jolt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:RelWithDebInfo>:JPH_DEBUG_RENDERER>")

# Link all the things
target_link_libraries(${PROJECT_NAME} PRIVATE Jolt)
target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi)
target_link_libraries(${PROJECT_NAME} PRIVATE lz4::lz4)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static)

# include dependency directories 
target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_SOURCE_DIR}/ThirdParty/stb
    ${CMAKE_SOURCE_DIR}/ThirdParty/ufbx
    ${CMAKE_SOURCE_DIR}/ThirdParty/cgltf
    ${CMAKE_SOURCE_DIR}/ThirdParty/imgui
    ${CMAKE_SOURCE_DIR}/ThirdParty/imnodes
    ${CMAKE_SOURCE_DIR}/ThirdParty/glm/glm
    ${CMAKE_SOURCE_DIR}/ThirdParty/ImGuizmo
    ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/src
    ${CMAKE_SOURCE_DIR}/ThirdParty/JoltPhysics
    ${CMAKE_SOURCE_DIR}/ThirdParty/DLSS/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/xess/inc/xess
    ${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage
	${CMAKE_SOURCE_DIR}/ThirdParty/BinaryRelations
    ${CMAKE_SOURCE_DIR}/ThirdParty/meshoptimizer/src
    ${CMAKE_SOURCE_DIR}/ThirdParty/IconFontCppHeaders
    ${CMAKE_SOURCE_DIR}/ThirdParty/D3D12MemoryAllocator/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/PIX/Include/WinPixEventRuntime
    ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)




