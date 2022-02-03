cmake_minimum_required(VERSION 3.15)

project(Raekor C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Add ImGuizmo and SPIRV Reflect to the cpp files list, disable PCH for .c files
set_source_files_properties(${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo/ImGuizmo.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND CppFiles ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo/ImGuizmo.cpp)

# Rule to build the actual exectuable and enable c++17
add_library(${PROJECT_NAME} ${CppFiles} ${HppFiles} ${CMAKE_SOURCE_DIR}/Raekor.rc)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

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
target_link_libraries(${PROJECT_NAME} PRIVATE DbgHelp) # Windows SDK helper library for parsing PDB files
target_link_libraries(${PROJECT_NAME} PRIVATE lz4::lz4)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2::SDL2-static)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PhysXPvdSDK_LIBRARY} ${PhysXExtensions_LIBRARY} ${PhysXFoundation_LIBRARY} ${PhysXCommon_LIBRARY} ${PhysX_LIBRARY})

# include dependency directories 
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/dependencies
    ${CMAKE_SOURCE_DIR}/dependencies/stb
    ${CMAKE_SOURCE_DIR}/dependencies/glm/glm
    ${CMAKE_SOURCE_DIR}/dependencies/entt/src
    ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo
    ${CMAKE_SOURCE_DIR}/dependencies/cereal/include
    ${CMAKE_SOURCE_DIR}/dependencies/IconFontCppHeaders
    ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)




