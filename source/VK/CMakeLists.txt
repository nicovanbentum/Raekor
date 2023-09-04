cmake_minimum_required(VERSION 3.15)

project(VK C CXX)

find_package(Vulkan REQUIRED)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Rule to build the actual exectuable and enable c++20
add_executable(${PROJECT_NAME} ${CppFiles} ${HppFiles} ${CMAKE_SOURCE_DIR}/assets/system/icon/Raekor.rc)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

# Link all the VCPKG things
target_link_libraries(${PROJECT_NAME} PRIVATE Raekor)
target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES})

# include dependency directories 
target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/dependencies
    ${CMAKE_SOURCE_DIR}/dependencies/VulkanMemoryAllocator/include/
    ${Vulkan_INCLUDE_DIRS}
)



