cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE "ThirdParty/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Triplets")
set(VCPKG_TARGET_TRIPLET x64-windows-raekor)
set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release" CACHE STRING "" FORCE)

project(Solution)

find_package(lz4 CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(directx-headers REQUIRED)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Lib)

# Enable MSVC parallel build process
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

add_subdirectory(Code/FSR2)
add_subdirectory(Code/Engine)
add_subdirectory(Code/Editor)
add_subdirectory(Code/Game)
add_subdirectory(Code/Tests)
