cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE "ThirdParty/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET x64-windows-static)
set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release" CACHE STRING "" FORCE)

project(Solution)

find_package(lz4 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(directx-headers REQUIRED)

# Enable MSVC parallel build process
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

add_subdirectory(Code/Engine)
add_subdirectory(Code/Editor)
# add_subdirectory(Code/Scripts)
