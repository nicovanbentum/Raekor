cmake_minimum_required(VERSION 3.15)

project(DX12 C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Rule to build the actual exectuable and enable c++17
add_executable(${PROJECT_NAME} ${CppFiles} ${HppFiles} ${CMAKE_SOURCE_DIR}/assets/system/icon/Raekor.rc)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

# Link the DirectX Compiler
find_library(DXC_LIBRARY NAMES dxcompiler lib PATHS ${CMAKE_SOURCE_DIR}/dependencies/dxc NO_DEFAULT_PATH REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Raekor)
target_link_libraries(${PROJECT_NAME} PRIVATE ${DXC_LIBRARY} d3d12.lib dxgi.lib d3dcompiler.lib)
target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft::DirectX-Guids Microsoft::DirectX-Headers)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dependencies/dxc/dxil.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dependencies/dxc/dxcompiler.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dependencies/Agility-SDK/D3D12Core.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dependencies/Agility-SDK/d3d12SDKLayers.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# include dependency directories 
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/dependencies
)