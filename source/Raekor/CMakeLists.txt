cmake_minimum_required(VERSION 3.15)

project(Raekor C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Add ImGuizmo and SPIRV Reflect to the cpp files list, disable PCH for .c files
set_source_files_properties(${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo/ImGuizmo.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo/ImGuizmo.cpp)

set_source_files_properties(${CMAKE_SOURCE_DIR}/dependencies/imnodes/imnodes.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/dependencies/imnodes/imnodes.cpp)

# Rule to build the actual exectuable and enable c++20

add_library(${PROJECT_NAME} ${CppFiles} ${HppFiles} raekor.natvis ${CMAKE_SOURCE_DIR}/config.json)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# Natvis files
target_sources(${PROJECT_NAME} PRIVATE raekor.natvis)
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/dependencies/glm/util/glm.natvis)
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/dependencies/JoltPhysics/Jolt/Jolt.natvis)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

# Add Jolt Physics library using their CMake file
set(PHYSICS_REPO_ROOT ${CMAKE_SOURCE_DIR}/dependencies/JoltPhysics)
include(${PHYSICS_REPO_ROOT}/Jolt/Jolt.cmake)
target_compile_features(Jolt PUBLIC cxx_std_17)
set_property(TARGET Jolt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:RelWithDebInfo>:JPH_DEBUG_RENDERER>")

# Link all the things
target_link_libraries(${PROJECT_NAME} PRIVATE Jolt)
target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi)
target_link_libraries(${PROJECT_NAME} PRIVATE DbgHelp) # Windows SDK helper library for parsing PDB files
target_link_libraries(${PROJECT_NAME} PRIVATE lz4::lz4)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2::SDL2-static)

# include dependency directories 
target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/dependencies
    ${CMAKE_SOURCE_DIR}/dependencies/stb
    ${CMAKE_SOURCE_DIR}/dependencies/cgltf
    ${CMAKE_SOURCE_DIR}/dependencies/imnodes
    ${CMAKE_SOURCE_DIR}/dependencies/glm/glm
    ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo
    ${CMAKE_SOURCE_DIR}/dependencies/cereal/include
    ${CMAKE_SOURCE_DIR}/dependencies/JoltPhysics
    ${CMAKE_SOURCE_DIR}/dependencies/IconFontCppHeaders
    ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)




