cmake_minimum_required(VERSION 3.15)

project(Engine C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Add library source files manually
set_source_files_properties(${CMAKE_SOURCE_DIR}/ThirdParty/ImGuizmo/ImGuizmo.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/ImGuizmo/ImGuizmo.cpp)

set_source_files_properties(${CMAKE_SOURCE_DIR}/ThirdParty/imnodes/imnodes.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/imnodes/imnodes.cpp)

set_source_files_properties(${CMAKE_SOURCE_DIR}/ThirdParty/ufbx/ufbx.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/ufbx/ufbx.c)

set_source_files_properties(${CMAKE_SOURCE_DIR}/ThirdParty/meshoptimizer/src/clusterizer.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/meshoptimizer/src/clusterizer.cpp)

set_source_files_properties(${CMAKE_SOURCE_DIR}/ThirdParty/samplerCPP/samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/samplerCPP/samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp.cpp)

#file(GLOB_RECURSE RawPDBHppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/raw_pdb/src/*.h)
#file(GLOB_RECURSE RawPDBCppFiles ${CMAKE_SOURCE_DIR}/ThirdParty/raw_pdb/src/*.cpp)
#set_source_files_properties(${RawPDBCppFiles} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
#list(APPEND CppFiles ${RawPDBCppFiles})

file(GLOB_RECURSE DiscordHeaderFiles ${CMAKE_SOURCE_DIR}/ThirdParty/discord_game_sdk/cpp/*.h)
file(GLOB_RECURSE DiscordSourceFiles ${CMAKE_SOURCE_DIR}/ThirdParty/discord_game_sdk/cpp/*.cpp)
set_source_files_properties(${DiscordSourceFiles} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${DiscordSourceFiles})

# Add ImGui
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/ThirdParty/imgui)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui_draw.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui_demo.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui_tables.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/imgui_widgets.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp)
list(APPEND ImGuiFiles ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
set_source_files_properties(ImGuiFiles PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# Append library cpp's
list(APPEND CppFiles ${ImGuiFiles})

# Rule to build the actual exectuable and enable c++20
add_library(${PROJECT_NAME} SHARED ${CppFiles} ${HppFiles} ../Core.natvis)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
set_target_properties(${PROJECT_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_compile_definitions(${PROJECT_NAME} PRIVATE RK_ENGINE_EXPORTS "IMGUI_API=__declspec(dllexport)")
target_compile_definitions(${PROJECT_NAME} INTERFACE "IMGUI_API=__declspec(dllimport)")

# Natvis files
target_sources(${PROJECT_NAME} PRIVATE ../Core.natvis)
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/glm/util/glm.natvis)
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/ufbx/misc/ufbx.natvis)
# target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/raw_pdb/raw_pdb.natvis)
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/JoltPhysics/Jolt/Jolt.natvis)

# MSVC stuff for root working directory and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

find_library(DLSS_LIBRARY NAMES nvsdk_ngx_d PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/DLSS/lib/Windows_x86_64/x86_64 NO_DEFAULT_PATH REQUIRED)
find_library(DLSS_LIBRARY_DEBUG NAMES nvsdk_ngx_d_dbg PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/DLSS/lib/Windows_x86_64/x86_64 NO_DEFAULT_PATH REQUIRED)

find_library(XESS NAMES libxess PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/xess/lib NO_DEFAULT_PATH REQUIRED)

find_library(PIX NAMES WinPixEventRuntime PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/PIX/bin/x64 NO_DEFAULT_PATH REQUIRED)

find_library(DXC_LIBRARY NAMES dxcompiler lib PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/dxc NO_DEFAULT_PATH REQUIRED)
find_library(DIRECT_STORAGE_LIBRARY NAMES dstorage lib PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage NO_DEFAULT_PATH REQUIRED)

find_library(DISCORD_LIB NAMES discord_game_sdk lib PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/discord_game_sdk/lib/x86_64 NO_DEFAULT_PATH REQUIRED)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Renderer/Shared.h" "${CMAKE_SOURCE_DIR}/Assets/Shaders/Backend/Include/Shared.hlsli")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/xess/bin/libxess.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/DLSS/lib/Windows_x86_64/dev/nvngx_dlss.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/dxc/dxil.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/dxc/dxcompiler.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage/dstorage.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage/dstoragecore.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/Agility-SDK/D3D12Core.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/Agility-SDK/d3d12SDKLayers.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/discord_game_sdk/lib/x86_64/discord_game_sdk.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/PIX/bin/x64/WinPixEventRuntime.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# Add Jolt Physics library using their CMake file
set(PHYSICS_REPO_ROOT ${CMAKE_SOURCE_DIR}/ThirdParty/JoltPhysics)
set(BUILD_SHARED_LIBS ON)
include(${PHYSICS_REPO_ROOT}/Jolt/Jolt.cmake)
set(BUILD_SHARED_LIBS OFF)
target_compile_features(Jolt PUBLIC cxx_std_17)
target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:RelWithDebInfo>:JPH_DEBUG_RENDERER>")

# Link all the things
target_link_libraries(${PROJECT_NAME} PUBLIC Jolt)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
add_library(D3D12MA STATIC ${CMAKE_SOURCE_DIR}/ThirdParty/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp)
target_include_directories(D3D12MA PUBLIC ${CMAKE_SOURCE_DIR}/ThirdParty/D3D12MemoryAllocator/include)

target_link_libraries(${PROJECT_NAME} PRIVATE D3D12MA)
target_link_libraries(${PROJECT_NAME} PRIVATE FSR2)
target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi)
target_link_libraries(${PROJECT_NAME} PRIVATE lz4::lz4)
target_link_libraries(${PROJECT_NAME} PRIVATE debug ${XESS} optimized ${XESS})
target_link_libraries(${PROJECT_NAME} PRIVATE debug ${DLSS_LIBRARY_DEBUG} optimized ${DLSS_LIBRARY})
target_link_libraries(${PROJECT_NAME} PRIVATE debug ${DISCORD_LIB} optimized ${DISCORD_LIB})
target_link_libraries(${PROJECT_NAME} PUBLIC ${DXC_LIBRARY} ${DIRECT_STORAGE_LIBRARY} ${PIX} d3d12.lib dxgi.lib d3dcompiler.lib dxguid.lib)

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
    # ${CMAKE_SOURCE_DIR}/ThirdParty/raw_pdb/src
    ${CMAKE_SOURCE_DIR}/ThirdParty/JoltPhysics
    ${CMAKE_SOURCE_DIR}/ThirdParty/DLSS/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/xess/inc/xess
    ${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage
	${CMAKE_SOURCE_DIR}/ThirdParty/BinaryRelations
    ${CMAKE_SOURCE_DIR}/ThirdParty/meshoptimizer/src
    ${CMAKE_SOURCE_DIR}/ThirdParty/IconFontCppHeaders
    ${CMAKE_SOURCE_DIR}/ThirdParty/discord_game_sdk/cpp
    ${CMAKE_SOURCE_DIR}/ThirdParty/D3D12MemoryAllocator/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/PIX/Include/WinPixEventRuntime
    ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)
